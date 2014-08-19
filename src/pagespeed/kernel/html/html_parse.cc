/*
 * Copyright 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: jmarantz@google.com (Joshua Marantz)

#include "pagespeed/kernel/html/html_parse.h"

#include <list>
#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/arena.h"
#include "pagespeed/kernel/base/atom.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/print_message_handler.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/symbol_table.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_event.h"
#include "pagespeed/kernel/html/html_filter.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/html/html_lexer.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {
class DocType;

HtmlParse::HtmlParse(MessageHandler* message_handler)
    : lexer_(NULL),  // Can't initialize here, since "this" should not be used
                     // in the initializer list (it generates an error in
                     // Visual Studio builds).
      current_(queue_.end()),
      message_handler_(message_handler),
      line_number_(1),
      skip_increment_(false),
      determine_enabled_filters_called_(false),
      need_sanity_check_(false),
      coalesce_characters_(true),
      need_coalesce_characters_(false),
      url_valid_(false),
      log_rewrite_timing_(false),
      running_filters_(false),
      parse_start_time_us_(0),
      timer_(NULL),
      current_filter_(NULL),
      dynamically_disabled_filter_list_(NULL) {
  lexer_ = new HtmlLexer(this);
  HtmlKeywords::Init();
}

HtmlParse::~HtmlParse() {
  delete lexer_;
  STLDeleteElements(&queue_);
  STLDeleteElements(&event_listeners_);
  ClearElements();
}

void HtmlParse::AddFilter(HtmlFilter* html_filter) {
  filters_.push_back(html_filter);
}

HtmlEventListIterator HtmlParse::Last() {
  HtmlEventListIterator p = queue_.end();
  --p;
  return p;
}

// Checks that the parent provided when creating the event's Node is
// consistent with the position in the list.  An alternative approach
// here is to use this code and remove the explicit specification of
// parents when constructing nodes.
//
// A complexity we will run into with that approach is that the queue_ is
// cleared on a Flush, so we cannot reliably derive the correct parent
// from the queue.  However, the Lexer keeps an element stack across
// flushes, and therefore can keep correct parent pointers.  So we have
// to inject pessimism in this process.
//
// Note that we also have sanity checks that run after each filter.
void HtmlParse::CheckParentFromAddEvent(HtmlEvent* event) {
  HtmlNode* node = event->GetNode();
  if (node != NULL) {
    message_handler_->Check(lexer_->Parent() == node->parent(),
                            "lexer_->Parent() != node->parent()");
    DCHECK_EQ(lexer_->Parent(), node->parent()) << url_;
  }
}

void HtmlParse::AddEvent(HtmlEvent* event) {
  CheckParentFromAddEvent(event);
  queue_.push_back(event);
  need_sanity_check_ = true;
  need_coalesce_characters_ = true;

  // If this is a leaf-node event, we need to set the iterator of the
  // corresponding leaf node to point to this event's position in the queue.
  // If this is an element event, then the iterators of the element will get
  // set in HtmlParse::AddElement and HtmlParse::CloseElement, so there's no
  // need to do it here.  If this is some other kind of event, there are no
  // iterators to set.
  HtmlLeafNode* leaf = event->GetLeafNode();
  if (leaf != NULL) {
    leaf->set_iter(Last());
    message_handler_->Check(IsRewritable(leaf), "!IsRewritable(leaf)");
  }
  if (!event_listeners_.empty()) {
    running_filters_ = true;
    for (FilterVector::iterator it = event_listeners_.begin();
        it != event_listeners_.end(); ++it) {
      event->Run(*it);
    }
    running_filters_ = false;
  }
}

// Testing helper method
void HtmlParse::SetCurrent(HtmlNode* node) {
  // Note: We use node->end() because that is often the place we want
  // to edit an element. For example, you cannot move an element when
  // current_ is not its end() event.
  current_ = node->end();
}

HtmlCdataNode* HtmlParse::NewCdataNode(HtmlElement* parent,
                                       const StringPiece& contents) {
  HtmlCdataNode* cdata =
      new (&nodes_) HtmlCdataNode(parent, contents, queue_.end());
  return cdata;
}

HtmlCharactersNode* HtmlParse::NewCharactersNode(HtmlElement* parent,
                                                 const StringPiece& literal) {
  HtmlCharactersNode* characters =
      new (&nodes_) HtmlCharactersNode(parent, literal, queue_.end());
  return characters;
}

HtmlCommentNode* HtmlParse::NewCommentNode(HtmlElement* parent,
                                           const StringPiece& contents) {
  HtmlCommentNode* comment =
      new (&nodes_) HtmlCommentNode(parent, contents, queue_.end());
  return comment;
}

HtmlIEDirectiveNode* HtmlParse::NewIEDirectiveNode(
    HtmlElement* parent, const StringPiece& contents) {
  HtmlIEDirectiveNode* directive =
      new (&nodes_) HtmlIEDirectiveNode(parent, contents, queue_.end());
  return directive;
}

HtmlDirectiveNode* HtmlParse::NewDirectiveNode(HtmlElement* parent,
                                               const StringPiece& contents) {
  HtmlDirectiveNode* directive =
      new (&nodes_) HtmlDirectiveNode(parent, contents, queue_.end());
  return directive;
}

HtmlElement* HtmlParse::NewElement(HtmlElement* parent, const HtmlName& name) {
  HtmlElement* element =
      new (&nodes_) HtmlElement(parent, name, queue_.end(), queue_.end());
  if (IsOptionallyClosedTag(name.keyword())) {
    // When we programmatically insert HTML nodes we should default to
    // including an explicit close-tag if they are optionally closed
    // such as <html>, <body>, and <p>.
    element->set_style(HtmlElement::EXPLICIT_CLOSE);
  }
  return element;
}

void HtmlParse::AddElement(HtmlElement* element, int line_number) {
  HtmlStartElementEvent* event =
      new HtmlStartElementEvent(element, line_number);
  AddEvent(event);
  element->set_begin(Last());
  element->set_begin_line_number(line_number);
}

bool HtmlParse::StartParseId(const StringPiece& url, const StringPiece& id,
                             const ContentType& content_type) {
  delayed_start_literal_.reset();
  determine_enabled_filters_called_ = false;

  // Paranoid debug-checking and unconditional clearing of state variables.
  DCHECK(!skip_increment_);
  skip_increment_ = false;
  DCHECK(deferred_nodes_.empty());
  deferred_nodes_.clear();
  DCHECK(open_deferred_nodes_.empty());
  open_deferred_nodes_.clear();
  DCHECK(current_filter_ == NULL);
  current_filter_ = NULL;
  DCHECK(deferred_deleted_nodes_.empty());
  deferred_deleted_nodes_.clear();

  if (dynamically_disabled_filter_list_ != NULL) {
    dynamically_disabled_filter_list_->clear();
  }
  url.CopyToString(&url_);
  GoogleUrl gurl(url);
  // TODO(sligocki): Use IsWebValid() here. For now we need to allow file://
  // URLs as well because some tools use them.
  url_valid_ = gurl.IsAnyValid();
  if (!url_valid_) {
    message_handler_->Message(kWarning, "HtmlParse: Invalid document url %s",
                              url_.c_str());
  } else {
    string_table_.Clear();
    google_url_.Swap(&gurl);
    line_number_ = 1;
    id.CopyToString(&id_);
    if (log_rewrite_timing_) {
      parse_start_time_us_ = timer_->NowUs();
      InfoHere("HtmlParse::StartParse");
    }
    AddEvent(new HtmlStartDocumentEvent(line_number_));
    lexer_->StartParse(id, content_type);
  }
  return url_valid_;
}

void HtmlParse::SetUrlForTesting(const StringPiece& url) {
  url.CopyToString(&url_);
  bool ok = google_url_.Reset(url);
  CHECK(ok) << url;
}

void HtmlParse::ShowProgress(const char* message) {
  if (log_rewrite_timing_) {
    long delta = static_cast<long>(timer_->NowUs() - parse_start_time_us_);
    InfoHere("%ldus: HtmlParse::%s", delta, message);
  }
}

void HtmlParse::FinishParse() {
  BeginFinishParse();
  Flush();
  EndFinishParse();
}

void HtmlParse::BeginFinishParse() {
  DCHECK(url_valid_) << "Invalid to call FinishParse on invalid input";
  if (url_valid_) {
    lexer_->FinishParse();
    DCHECK(delayed_start_literal_.get() == NULL);
    delayed_start_literal_.reset();
    AddEvent(new HtmlEndDocumentEvent(line_number_));
  }
}

void HtmlParse::EndFinishParse() {
  if (url_valid_) {
    ClearElements();
    ShowProgress("FinishParse");
  }
}

void HtmlParse::Clear() {
  string_table_.Clear();
}

void HtmlParse::ParseTextInternal(const char* text, int size) {
  DCHECK(url_valid_) << "Invalid to call ParseText with invalid url";
  if (url_valid_) {
    DetermineEnabledFilters();
    lexer_->Parse(text, size);
  }
}

void HtmlParse::DetermineEnabledFiltersImpl() {
  DetermineEnabledFiltersInList(filters_);
}

void HtmlParse::CheckFilterEnabled(HtmlFilter* filter) {
  GoogleString disabled_reason;
  filter->DetermineEnabled(&disabled_reason);

  if (!filter->is_enabled() && dynamically_disabled_filter_list_ != NULL) {
    GoogleString final_reason(filter->Name());
    if (!disabled_reason.empty()) {
      StrAppend(&final_reason, ": ", disabled_reason);
    }

    dynamically_disabled_filter_list_->push_back(final_reason);
  }
}

// This is factored out of Flush() for testing purposes.
void HtmlParse::ApplyFilter(HtmlFilter* filter) {
  // Keep track of the current filter in a state variable, for associating
  // node-deferrals with the filter that requested them.
  DCHECK(current_filter_ == NULL);
  current_filter_ = filter;

  // If, in a previous flush window, the current filter requested the deferral
  // of an element that has not yet been closed, then move any events we've seen
  // in this flush window into the element's node-list.  Do this up to the close
  // event, if it's in this flush window.  If the close event is not in this
  // flush window, then the entire flush window's worth events get moved.
  FilterElementMap::iterator p = open_deferred_nodes_.find(filter);
  if (p != open_deferred_nodes_.end()) {
    HtmlNode* deferred_node = p->second.first;
    HtmlEventList* node_events = p->second.second;
    if (deferred_node->end() != queue_.end()) {
      // The node is closed, we can now clean up this map.
      open_deferred_nodes_.erase(p);
      HtmlEventListIterator last = deferred_node->end();
      ++last;  // splice is non-inclusive at end and we want to include the end.
      node_events->splice(node_events->end(), queue_, queue_.begin(), last);
    } else {
      // The entire flush-window is part of this unclosed deferred node,
      // so move all if it.
      node_events->splice(node_events->end(), queue_, queue_.begin(),
                          queue_.end());
    }
  }

  if (coalesce_characters_ && need_coalesce_characters_) {
    CoalesceAdjacentCharactersNodes();
    DelayLiteralTag();
    need_coalesce_characters_ = false;
  }

  ShowProgress(StrCat("ApplyFilter:", filter->Name()).c_str());
  for (current_ = queue_.begin(); current_ != queue_.end(); NextEvent()) {
    HtmlEvent* event = *current_;
    line_number_ = event->line_number();
    event->Run(filter);
  }
  filter->Flush();

  if (need_sanity_check_) {
    SanityCheck();
    need_sanity_check_ = false;
  }
  current_filter_ = NULL;
}

void HtmlParse::NextEvent() {
  if (skip_increment_) {
    skip_increment_ = false;
  } else {
    ++current_;
  }
}

void HtmlParse::CoalesceAdjacentCharactersNodes() {
  ShowProgress("CoalesceAdjacentCharactersNodes");
  HtmlCharactersNode* prev = NULL;
  for (current_ = queue_.begin(); current_ != queue_.end(); ) {
    HtmlEvent* event = *current_;
    HtmlCharactersNode* node = event->GetCharactersNode();
    if ((node != NULL) && (prev != NULL)) {
      prev->Append(node->contents());
      current_ = queue_.erase(current_);  // returns element after erased
      delete event;
      node->MarkAsDead(queue_.end());
      need_sanity_check_ = true;
    } else {
      ++current_;
      prev = node;
    }
  }
}

void HtmlParse::DelayLiteralTag() {
  if (queue_.empty()) {
    return;
  }
  current_ = queue_.end();
  --current_;
  HtmlEvent* event = *current_;
  HtmlElement* element = event->GetElementIfStartEvent();
  if ((element != NULL) && IsLiteralTag(element->keyword())) {
    // The current stream ends with the beginning of a literal
    // tag.  We are not going to process this within the current
    // flush window, but instead wait till the EndElement arrives
    // from the lexer.
    delayed_start_literal_.reset(event);
    queue_.erase(current_);
  }
  current_ = queue_.end();
}

void HtmlParse::CheckEventParent(HtmlEvent* event, HtmlElement* expect,
                                 HtmlElement* actual) {
  if ((expect != NULL) && (actual != expect)) {
    GoogleString actual_buf;
    if (actual != NULL) {
      actual_buf = actual->ToString();
    } else {
      actual_buf = "(null)";
    }
    GoogleString expect_buf = expect->ToString();
    GoogleString event_buf = event->ToString();
    FatalErrorHere("HtmlElement Parents of %s do not match:\n"
                   "Actual:   %s\n"
                   "Expected: %s\n",
                   event_buf.c_str(), actual_buf.c_str(), expect_buf.c_str());
  }
}

void HtmlParse::SanityCheck() {
  ShowProgress("SanityCheck");

  // Sanity check that the Node parent-pointers are consistent with the
  // begin/end-element events.  This is done in a second pass to avoid
  // confusion when the filter mutates the event-stream.  Also note that
  // a mid-HTML call to HtmlParse::Flush means that we may pop out beyond
  // the stack we can detect in this event stream.  This is represented
  // here by an empty stack.
  std::vector<HtmlElement*> element_stack;
  HtmlElement* expect_parent = NULL;
  for (current_ = queue_.begin(); current_ != queue_.end(); ++current_) {
    HtmlEvent* event = *current_;

    // Determine whether the event we are looking at is a StartElement,
    // EndElement, or a leaf.  We manipulate our temporary stack when
    // we see StartElement and EndElement, and we always test to make
    // sure the elements have the expected parent based on context, when
    // we can figure out what the expected parent is.
    HtmlElement* start_element = event->GetElementIfStartEvent();
    if (start_element != NULL) {
      CheckEventParent(event, expect_parent, start_element->parent());
      message_handler_->Check(start_element->begin() == current_,
                              "start_element->begin() != current_");
      message_handler_->Check(start_element->live(), "!start_element->live()");
      element_stack.push_back(start_element);
      expect_parent = start_element;
    } else {
      HtmlElement* end_element = event->GetElementIfEndEvent();
      if (end_element != NULL) {
        message_handler_->Check(end_element->end() == current_,
                                "end_element->end() != current_");
        message_handler_->Check(end_element->live(),
                                "!end_element->live()");
        if (!element_stack.empty()) {
          // The element stack can be empty on End can happen due
          // to this sequence:
          //   <tag1>
          //     FLUSH
          //   </tag1>   <!-- tag1 close seen with empty stack -->
          message_handler_->Check(element_stack.back() == end_element,
                                  "element_stack.back() != end_element");
          element_stack.pop_back();
        }
        expect_parent = element_stack.empty() ? NULL : element_stack.back();
        CheckEventParent(event, expect_parent, end_element->parent());
      } else {
        // We only know for sure what the parents are once we have seen
        // a start_element.
        HtmlLeafNode* leaf_node = event->GetLeafNode();
        if (leaf_node != NULL) {   // Start/EndDocument are not leaf nodes
          message_handler_->Check(leaf_node->live(), "!leaf_node->live()");
          message_handler_->Check(leaf_node->end() == current_,
                                  "leaf_node->end() != current_");
          CheckEventParent(event, expect_parent, leaf_node->parent());
        }
      }
    }
  }
}

void HtmlParse::Flush() {
  DCHECK(!running_filters_);
  if (running_filters_) {
    return;
  }

  // If Flush is called before any bytes are received, StartDocument events
  // will propagate to filters before DetermineEnabled is called on them. So we
  // send DetermineEnabled here.
  DetermineEnabledFilters();

  for (FilterVector::iterator it = event_listeners_.begin();
      it != event_listeners_.end(); ++it) {
    (*it)->Flush();
  }

  DCHECK(url_valid_) << "Invalid to call FinishParse with invalid url";
  if (url_valid_) {
    ShowProgress("Flush");

    for (FilterList::iterator i = filters_.begin(); i != filters_.end(); ++i) {
      HtmlFilter* filter = *i;
      if (filter->is_enabled()) {
        ApplyFilter(filter);
      }
    }
    ClearEvents();
  }
}

void HtmlParse::ClearEvents() {
  // Detach all the elements from their events, as we are now invalidating
  // the events and deleting the contents of Closed elements, though we are
  // leaving the HtmlElement* and other HtmlNodes allocated until EndFinishParse
  // is called.
  for (current_ = queue_.begin(); current_ != queue_.end(); ++current_) {
    HtmlEvent* event = *current_;
    line_number_ = event->line_number();
    HtmlElement* element = event->GetElementIfStartEvent();
    if (element != NULL) {
      element->set_begin(queue_.end());
    } else {
      element = event->GetElementIfEndEvent();
      if (element != NULL) {
        element->set_end(queue_.end());
        element->FreeData();
      } else {
        HtmlLeafNode* leaf_node = event->GetLeafNode();
        if (leaf_node != NULL) {
          leaf_node->set_iter(queue_.end());
          leaf_node->FreeData();
        }
      }
    }
    delete event;
  }
  queue_.clear();
  need_sanity_check_ = false;
  need_coalesce_characters_ = false;
}

size_t HtmlParse::GetEventQueueSize() {
  return queue_.size();
}

void HtmlParse::InsertNodeBeforeNode(const HtmlNode* existing_node,
                                     HtmlNode* new_node) {
  // begin() == queue_.end() -> this is an invalid element.
  // TODO(sligocki): Rather than checks, we should probably return this as
  // the status.
  message_handler_->Check(existing_node->begin() != queue_.end(),
                          "InsertNodeBeforeNode: existing_node invalid");
  new_node->set_parent(existing_node->parent());
  InsertNodeBeforeEvent(existing_node->begin(), new_node);
}

void HtmlParse::InsertNodeAfterNode(const HtmlNode* existing_node,
                                    HtmlNode* new_node) {
  message_handler_->Check(existing_node->end() != queue_.end(),
                          "InsertNodeAfterNode: existing_node invalid");
  new_node->set_parent(existing_node->parent());
  InsertNodeAfterEvent(existing_node->end(), new_node);
}

void HtmlParse::PrependChild(const HtmlElement* existing_parent,
                             HtmlNode* new_child) {
  message_handler_->Check(existing_parent->begin() != queue_.end(),
                          "PrependChild: existing_parent invalid");
  new_child->set_parent(const_cast<HtmlElement*>(existing_parent));
  InsertNodeAfterEvent(existing_parent->begin(), new_child);
}

void HtmlParse::AppendChild(const HtmlElement* existing_parent,
                            HtmlNode* new_child) {
  message_handler_->Check(existing_parent->end() != queue_.end(),
                          "AppendChild: existing_parent invalid");
  new_child->set_parent(const_cast<HtmlElement*>(existing_parent));
  InsertNodeBeforeEvent(existing_parent->end(), new_child);
}

void HtmlParse::InsertNodeBeforeCurrent(HtmlNode* new_node) {
  if (skip_increment_) {
    FatalErrorHere("InsertNodeBeforeCurrent after current has been "
                   "deleted.");
  }
  if ((new_node->parent() == NULL) && (current_ != queue_.end())) {
    // Add a parent if one was not provided in new_node.  We figure out
    // what the parent should be by looking at current_.  If that's an
    // EndElement event, then that means that we are adding a new child
    // of that element.  In all other cases, we are adding a sibling.
    HtmlEvent* current_event = *current_;
    HtmlElement* end_element = current_event->GetElementIfEndEvent();
    if (end_element != NULL) {
      // The node pointed to by Current will be our new parent.
      new_node->set_parent(end_element);
    } else {
      // The node pointed to by Current will be our new sibling, so
      // we should grab its parent.
      HtmlNode* node = current_event->GetNode();
      // 'node' can be null when the current event is EndDocument.
      if (node != NULL) {
        new_node->set_parent(node->parent());
      }
    }
  }
  InsertNodeBeforeEvent(current_, new_node);
}

void HtmlParse::InsertNodeBeforeEvent(const HtmlEventListIterator& event,
                                      HtmlNode* new_node) {
  need_sanity_check_ = true;
  need_coalesce_characters_ = true;
  new_node->SynthesizeEvents(event, &queue_);
}

void HtmlParse::InsertNodeAfterEvent(const HtmlEventListIterator& event,
                                     HtmlNode* new_node) {
  message_handler_->Check(event != queue_.end(), "event == queue_.end()");
  HtmlEventListIterator next_event = event;
  ++next_event;
  InsertNodeBeforeEvent(next_event, new_node);
}


void HtmlParse::InsertNodeAfterCurrent(HtmlNode* new_node) {
  if (skip_increment_) {
    FatalErrorHere("InsertNodeAfterCurrent after current has been "
                   "deleted.");
  }
  if (current_ == queue_.end()) {
    FatalErrorHere("InsertNodeAfterCurrent called with queue at end.");
  }
  ++current_;
  InsertNodeBeforeEvent(current_, new_node);

  // We want to leave current_ pointing to the newly created element.
  --current_;
  message_handler_->Check((*current_)->GetNode() == new_node,
                          "(*current_)->GetNode() != new_node");
}

bool HtmlParse::AddParentToSequence(HtmlNode* first, HtmlNode* last,
                                    HtmlElement* new_parent) {
  bool added = false;
  HtmlElement* original_parent = first->parent();
  if (IsRewritable(first) && IsRewritable(last) &&
      (last->parent() == original_parent) &&
      (new_parent->begin() == queue_.end()) &&
      (new_parent->end() == queue_.end())) {
    InsertNodeBeforeEvent(first->begin(), new_parent);
    // This sequence of checks culminated in inserting the parent's begin
    // and end before 'first'.  Now we must mutate new_parent's end pointer
    // to insert it after the last->end().  list::insert(iter) inserts
    // *before* the iter, so we'll increment last->end().
    HtmlEvent* end_element_event = *new_parent->end();
    queue_.erase(new_parent->end());
    HtmlEventListIterator p = last->end();
    ++p;
    new_parent->set_end(queue_.insert(p, end_element_event));
    FixParents(first->begin(), last->end(), new_parent);
    added = true;
    need_sanity_check_ = true;
    need_coalesce_characters_ = true;
  }
  return added;
}

void HtmlParse::FixParents(const HtmlEventListIterator& begin,
                           const HtmlEventListIterator& end_inclusive,
                           HtmlElement* new_parent) {
  HtmlEvent* event = *begin;
  HtmlNode* first = event->GetNode();
  HtmlElement* original_parent = first->parent();
  // Loop over all the nodes from begin to end, inclusive,
  // and set the parent pointer for the node, if there is one.  A few
  // event types don't have HtmlNodes, such as Comments and IEDirectives.
  message_handler_->Check(end_inclusive != queue_.end(),
                          "end_inclusive == queue_.end()");
  HtmlEventListIterator end = end_inclusive;
  ++end;
  for (HtmlEventListIterator p = begin; p != end; ++p) {
    HtmlNode* node = (*p)->GetNode();
    if ((node != NULL) && (node->parent() == original_parent)) {
      node->set_parent(new_parent);
    }
  }
}

bool HtmlParse::MoveCurrentInto(HtmlElement* new_parent) {
  bool moved = false;
  if (current_ == queue_.end()) {
    DebugLogQueue();
    LOG(DFATAL) << "MoveCurrentInto() called at queue_.end()";
  } else if (new_parent->live()) {
    HtmlNode* current_node = (*current_)->GetNode();
    if (MoveCurrentBeforeEvent(new_parent->end())) {
      current_node->set_parent(new_parent);
      moved = true;
    }
  }
  return moved;
}

bool HtmlParse::MoveCurrentBefore(HtmlNode* element) {
  bool moved = false;
  DCHECK(current_ != queue_.end());
  if (current_ == queue_.end()) {
    DebugLogQueue();
    LOG(DFATAL) << "MoveCurrentBefore() called at queue_.end()";
  } else if (element->live()) {
    HtmlNode* current_node = (*current_)->GetNode();
    if (MoveCurrentBeforeEvent(element->begin())) {
      current_node->set_parent(element->parent());
      moved = true;
    }
  }
  return moved;
}

// NOTE: Only works if current_ is an end() event.
// Additionally, there are common sense constraints like, current_node and
// move_to must be within the event window, etc.
bool HtmlParse::MoveCurrentBeforeEvent(const HtmlEventListIterator& move_to) {
  bool ret = false;
  if (move_to != queue_.end() && current_ != queue_.end()) {
    HtmlNode* move_to_node = (*move_to)->GetNode();
    HtmlNode* current_node = (*current_)->GetNode();
    // TODO(sligocki): This and many other uses can now crash if called with
    // a non-rewritable element :/
    HtmlEventListIterator begin = current_node->begin();
    HtmlEventListIterator end = current_node->end();

    if (current_ == end && IsInEventWindow(begin) && IsInEventWindow(end) &&
        IsInEventWindow(move_to) &&
        !IsDescendantOf(move_to_node, current_node)) {
      ++end;  // splice is non-inclusive for the 'end' iterator.

      // Manipulate current_ so that when Flush() iterates it lands
      // you on object after current_'s original position, rather
      // than re-iterating over the new_parent's EndElement event.
      current_ = end;
      // NOTE: This will do Very Bad Things if move_to is between begin and end.
      // The IsDescendantOf check above should guard against this if the DOM
      // structure is preserved.
      queue_.splice(move_to, queue_, begin, end);
      --current_;

      // TODO(jmarantz): According to
      // http://www.cplusplus.com/reference/stl/list/splice/
      // the moved iterators are no longer valid, and we
      // are retaining them in the HtmlNode, so we need to fix them.
      //
      // However, in practice they appear to remain valid.  And
      // I can't think of a reason they should be invalidated,
      // as the iterator is a pointer to a node structure with
      // next/prev pointers.  splice can mutate the next/prev pointers
      // in place.
      //
      // See http://stackoverflow.com/questions/143156

      need_sanity_check_ = true;
      need_coalesce_characters_ = true;
      ret = true;
    }
  }

  return ret;
}

// TODO(sligocki): Make a member function of HtmlNode so that it reads
// more grammatically correct?
bool HtmlParse::IsDescendantOf(const HtmlNode* possible_child,
                               const HtmlNode* possible_parent) {
  // node walks up the DOM starting from possible_child.
  const HtmlNode* node = possible_child;
  while (node != NULL) {
    if (node == possible_parent) {
      // If possible_parent is a parent-of-parent-of-...
      return true;
    }
    // Walk up further.
    // Note: this walk ends at top level where parent() == NULL.
    node = node->parent();
  }
  return false;
}

bool HtmlParse::DeleteNode(HtmlNode* node) {
  bool deleted = false;
  if (IsRewritable(node)) {
    bool done = false;
    // If node is an HtmlLeafNode, then begin() and end() might be the same.
    for (HtmlEventListIterator p = node->begin(); !done; ) {
      // We want to include end, so once p == end we still have to do one more
      // iteration.
      done = (p == node->end());

      // Clean up any nested elements/leaves as we get to their 'end' event.
      HtmlEvent* event = *p;

      // Check if we're about to delete the current event.
      if (!skip_increment_ && (p == current_)) {
        skip_increment_ = true;
        current_ = node->end();
        ++current_;
      }
      p = queue_.erase(p);

      HtmlNode* nested_node = event->GetElementIfEndEvent();
      if (nested_node == NULL) {
        nested_node = event->GetLeafNode();
      }
      if (nested_node != NULL) {
        message_handler_->Check(nested_node->live(), "!nested_node->live()");
        nested_node->MarkAsDead(queue_.end());
      }

      delete event;
    }

    // Our iteration should have covered the passed-in element as well.
    message_handler_->Check(!node->live(), "node->live()");
    deleted = true;
    need_sanity_check_ = true;
    need_coalesce_characters_ = true;
  } else if (IsRewritableIgnoringEnd(node) && (current_ != queue_.end())) {
    // If current_ is the StartElement of the requested node, then we
    // can delete it even if the end-element has not been seen yet due
    // to a flush window, by simply deferring it.  We just want to
    // keep track of which deferred nodes we don't expect to restore.
    HtmlEvent* event = *current_;
    if ((event->GetNode() == node) &&
        (event->GetElementIfEndEvent() == NULL)) {  // leaf or StartElement OK
      DeferCurrentNode();
      deferred_deleted_nodes_.insert(node);
      deleted = true;
    }
  }
  return deleted;
}

bool HtmlParse::DeleteSavingChildren(HtmlElement* element) {
  bool deleted = false;
  if (IsRewritable(element)) {
    HtmlElement* new_parent = element->parent();
    HtmlEventListIterator first = element->begin();
    ++first;
    HtmlEventListIterator last = element->end();
    if (first != last) {
      --last;
      FixParents(first, last, new_parent);
      // Ensure the event queue is modified in a sensible way. If we are
      // deleting from the start tag, the child events should go after this
      // node (so we can parse them) and if we are deleting from the end,
      // they should go before (to avoid double parsing). If we are deleting
      // from anywhere else, it doesn't matter if we put them before or after
      // as we won't be moving events relative to the current_ pointer.
      bool at_end = (current_ == queue_.end());
      HtmlEvent* cur_event = *current_;
      if (!at_end && cur_event->GetElementIfStartEvent() == element) {
        queue_.splice(++element->end(), queue_, first, element->end());
      } else {
        queue_.splice(element->begin(), queue_, first, element->end());
      }
      need_sanity_check_ = true;
      need_coalesce_characters_ = true;
    }
    deleted = DeleteNode(element);
  }
  return deleted;
}

bool HtmlParse::MakeElementInvisible(HtmlElement* element) {
  bool ret = false;
  // We consider it an error to make an element invisible whose Start element
  // has been flushed, though it's fine to make an element invisible whose
  // End element is yet parsed.
  if (IsRewritableIgnoringEnd(element)) {
    element->set_style(HtmlElement::INVISIBLE);
    ret = true;
  }
  return ret;
}

bool HtmlParse::HasChildrenInFlushWindow(HtmlElement* element) {
  bool has_children = false;
  if (IsRewritable(element)) {
    HtmlEventListIterator first = element->begin();
    if (first != queue_.end()) {
      ++first;
      has_children = (first != element->end());
    }
  }
  return has_children;
}

bool HtmlParse::ReplaceNode(HtmlNode* existing_node, HtmlNode* new_node) {
  bool replaced = false;
  if (IsRewritable(existing_node)) {
    InsertNodeBeforeNode(existing_node, new_node);
    replaced = DeleteNode(existing_node);
    message_handler_->Check(replaced, "!replaced");
  }
  return replaced;
}

HtmlElement* HtmlParse::CloneElement(HtmlElement* in_element) {
  HtmlElement* out_element = NewElement(NULL, in_element->name());
  out_element->set_style(in_element->style());

  const HtmlElement::AttributeList& attrs = in_element->attributes();
  for (HtmlElement::AttributeConstIterator i(attrs.begin());
       i != attrs.end(); ++i) {
    out_element->AddAttribute(*i);
  }
  return out_element;
}

bool HtmlParse::IsRewritableIgnoringDeferral(const HtmlNode* node) const {
  return (node->live() &&  // Avoid dereferencing NULL data for closed elements.
          IsInEventWindow(node->begin()) &&
          IsInEventWindow(node->end()));
}

bool HtmlParse::IsRewritableIgnoringEnd(const HtmlNode* node) const {
  return (node->live() &&  // Avoid dereferencing NULL data for closed elements.
          (deferred_nodes_.find(node) == deferred_nodes_.end()) &&
          IsInEventWindow(node->begin()));
}

bool HtmlParse::IsRewritable(const HtmlNode* node) const {
  return (IsRewritableIgnoringDeferral(node) &&
          (deferred_nodes_.find(node) == deferred_nodes_.end()));
}

bool HtmlParse::CanAppendChild(const HtmlNode* node) const {
  return (node->live() &&  // Avoid dereferencing NULL data for closed elements.
          (deferred_nodes_.find(node) == deferred_nodes_.end()) &&
          IsInEventWindow(node->end()));
}

bool HtmlParse::IsInEventWindow(const HtmlEventListIterator& iter) const {
  return iter != queue_.end();
}

void HtmlParse::ClearElements() {
  ClearDeferredNodes();
  nodes_.DestroyObjects();
  DCHECK(!running_filters_);
}

void HtmlParse::EmitQueue(MessageHandler* handler) {
  for (HtmlEventList::iterator p = queue_.begin(), e = queue_.end();
       p != e; ++p) {
    HtmlEvent* event = *p;
    handler->Message(kInfo, "%c %s (%p)\n",
                     p == current_ ? '*' : ' ',
                     event->ToString().c_str(),
                     event->GetNode());
  }
}

void HtmlParse::DebugLogQueue() {
  EmitQueue(message_handler_);
}

void HtmlParse::DebugPrintQueue() {
  PrintMessageHandler handler;
  EmitQueue(&handler);
}

bool HtmlParse::IsImplicitlyClosedTag(HtmlName::Keyword keyword) const {
  return lexer_->IsImplicitlyClosedTag(keyword);
}

bool HtmlParse::IsLiteralTag(HtmlName::Keyword keyword) {
  return HtmlLexer::IsLiteralTag(keyword);
}

bool HtmlParse::IsSometimesLiteralTag(HtmlName::Keyword keyword) {
  return HtmlLexer::IsSometimesLiteralTag(keyword);
}

bool HtmlParse::IsOptionallyClosedTag(HtmlName::Keyword keyword) const {
  return HtmlKeywords::IsOptionallyClosedTag(keyword);
}

bool HtmlParse::TagAllowsBriefTermination(HtmlName::Keyword keyword) const {
  return lexer_->TagAllowsBriefTermination(keyword);
}

const DocType& HtmlParse::doctype() const {
  return lexer_->doctype();
}

void HtmlParse::InfoV(
    const char* file, int line, const char *msg, va_list args) {
  message_handler_->InfoV(file, line, msg, args);
}

void HtmlParse::WarningV(
    const char* file, int line, const char *msg, va_list args) {
  message_handler_->WarningV(file, line, msg, args);
}

void HtmlParse::ErrorV(
    const char* file, int line, const char *msg, va_list args) {
  message_handler_->ErrorV(file, line, msg, args);
}

void HtmlParse::FatalErrorV(
    const char* file, int line, const char* msg, va_list args) {
  message_handler_->FatalErrorV(file, line, msg, args);
}

void HtmlParse::Info(const char* file, int line, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  InfoV(file, line, msg, args);
  va_end(args);
}

void HtmlParse::Warning(const char* file, int line, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  WarningV(file, line, msg, args);
  va_end(args);
}

void HtmlParse::Error(const char* file, int line, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  ErrorV(file, line, msg, args);
  va_end(args);
}

void HtmlParse::FatalError(const char* file, int line, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  FatalErrorV(file, line, msg, args);
  va_end(args);
}

void HtmlParse::InfoHere(const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  InfoHereV(msg, args);
  va_end(args);
}

void HtmlParse::WarningHere(const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  WarningHereV(msg, args);
  va_end(args);
}

void HtmlParse::ErrorHere(const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  ErrorHereV(msg, args);
  va_end(args);
}

void HtmlParse::FatalErrorHere(const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  FatalErrorHereV(msg, args);
  va_end(args);
}

void HtmlParse::CloseElement(
    HtmlElement* element, HtmlElement::Style style, int line_number) {
  if (delayed_start_literal_.get() != NULL) {
    HtmlElement* element = delayed_start_literal_->GetElementIfStartEvent();
    DCHECK(element != NULL);
    bool insert_at_begin = true;
    if (!queue_.empty()) {
      // We have been holding back "<script>" until the lexer tells us the
      // tag is closed here.  But we want to insert the <script> tag *before*
      // the previous characters block, if any.
      //
      // Most of the time we just need queue_.push_front (bool insert_at_begin
      // above) but when InsertComment is called from Flush, which happens
      // in the debug filter, we must put the <script> after that, so
      // walk back from current, past the Character block, if any.  We
      // don't expect anything other than a Character block here.
      HtmlEventListIterator p = queue_.end();
      --p;
      HtmlEvent* event = *p;
      HtmlCharactersNode* node = event->GetCharactersNode();
      if (node != NULL) {
        if (p != queue_.begin()) {
          --p;
          element->set_begin(
              queue_.insert(p, delayed_start_literal_.release()));
          insert_at_begin = false;
        }
      } else {
        // I don't think it's possible to get here, but let's find out if
        // we do.  Note that we use an 'info' log as this is not actionable
        // from a site owner's perspective, and I think we are doing the right
        // thing anyway, but I'd like to hit this with a unit test if we can
        // find a case.
        GoogleString buf = event->ToString();
        InfoHere("Deferred literal tag, expected a characters node : %s",
                 buf.c_str());
        LOG(DFATAL) << "Deferred literal tag, expected a characters node: "
                    << buf;
      }
    }
    if (insert_at_begin) {
      queue_.push_front(delayed_start_literal_.release());
      element->set_begin(queue_.begin());
    }
    DCHECK(delayed_start_literal_.get() == NULL);
  }

  HtmlEndElementEvent* end_event =
      new HtmlEndElementEvent(element, line_number);
  if (element->style() != HtmlElement::INVISIBLE) {
    element->set_style(style);
  }
  AddEvent(end_event);
  element->set_end(Last());
  element->set_end_line_number(line_number);
}

HtmlName HtmlParse::MakeName(HtmlName::Keyword keyword) {
  const StringPiece* str = HtmlKeywords::KeywordToString(keyword);
  return HtmlName(keyword, str);
}

HtmlName HtmlParse::MakeName(const StringPiece& str_piece) {
  HtmlName::Keyword keyword = HtmlName::Lookup(str_piece);
  const StringPiece* str = HtmlKeywords::KeywordToString(keyword);

  // If the passed-in string is not in its canonical form, or is not a
  // recognized keyword, then we must make a permanent copy in our
  // string table.  Note that we are comparing the bytes of the
  // keyword from the table, not the pointer.
  if ((str == NULL) || (str_piece != *str)) {
    Atom atom = string_table_.Intern(str_piece);
    str = atom.Rep();
  }
  return HtmlName(keyword, str);
}

void HtmlParse::add_event_listener(HtmlFilter* listener) {
  event_listeners_.push_back(listener);
}

void HtmlParse::set_size_limit(int64 x) {
  lexer_->set_size_limit(x);
}

bool HtmlParse::size_limit_exceeded() const {
  return lexer_->size_limit_exceeded();
}

bool HtmlParse::InsertComment(StringPiece unescaped) {
  HtmlElement* parent = NULL;

  GoogleString escaped;
  HtmlKeywords::Escape(unescaped, &escaped);

  if (queue_.begin() != queue_.end()) {
    HtmlEventListIterator pos = current_;
    HtmlEvent* event;

    // Get the last event.
    if (pos == queue_.end()) {
      --pos;
    }
    event = *pos;

    // Be careful where we insert comments into HTML Elements, as some of
    // them cannot tolerate new children -- even comments (e.g. textarea,
    // script).  So if we are looking at a start_element, then insert before,
    // but if we are looking at an end_element, then insert after.
    HtmlElement* start_element = event->GetElementIfStartEvent();
    HtmlElement* end_element = event->GetElementIfEndEvent();
    if (start_element != NULL) {
      parent = start_element->parent();
      InsertNodeBeforeEvent(pos, NewCommentNode(parent, escaped));
    } else if (end_element != NULL) {
      parent = end_element->parent();
      InsertNodeAfterEvent(pos, NewCommentNode(parent, escaped));
    } else {
      // The current node must not be an element, but instead a leaf
      // node such as another Comment, IEDirective, or Characters.
      // Insert the comment immediately after the node if we are at the
      // end, otherwise insert the comment before the current node.
      HtmlNode* node = event->GetNode();
      if (node != NULL) {
        parent = node->parent();
      }
      if (current_ == queue_.end()) {
        InsertNodeAfterEvent(pos, NewCommentNode(parent, escaped));
      } else {
        InsertNodeBeforeEvent(pos, NewCommentNode(parent, escaped));
      }
    }
  } else {
    // Verify that we aren't trying to create a new node inside of a literal
    // block. This can happen if we already flushed the open tag of a literal
    // element, but haven't seen the close tag yet.
    HtmlElement* parent = lexer_->Parent();
    if (parent != NULL && IsLiteralTag(parent->keyword())) {
      return false;
    }
    AddEvent(
        new HtmlCommentEvent(NewCommentNode(lexer_->Parent(), escaped), 0));
  }
  return true;
}

void HtmlParse::DeferCurrentNode() {
  CHECK(current_ != queue_.end());
  HtmlNode* node = (*current_)->GetNode();

#ifndef NDEBUG
  DCHECK(node->live());
  DCHECK(open_deferred_nodes_.find(current_filter_) ==
         open_deferred_nodes_.end());
  DCHECK(deferred_nodes_.find(node) == deferred_nodes_.end());
  DCHECK(node->begin() != queue_.end())
      << "Cannot remove a node whose opening tag is flushed";
#endif

  // There are three cases:
  //   1. We are removing a node totally in the flush window (fine)
  //   2. We are remove an element at its StartElement event, but its
  //      EndElement event is not in the flush window.
  //   3. We are removing an element at its EndElement event, but its
  //      StartElement event is not in the flush window.  We avoid this
  //      case by requiring that callers run DeferCurentNode from the
  //      StartElement event.
  HtmlEventList* node_events = new HtmlEventList;
  deferred_nodes_[node] = node_events;
  HtmlEventListIterator node_last = node->end();
  if (node_last != queue_.end()) {
    // Case 1: node is totally in flush window.
    ++node_last;
  } else {
    // Case 2: we need to keep track of the node-removals that are not closed,
    // so as we lex in new child nodes we can put them onto the correct
    // node-list, rather than the queue_, until it's closed.
    HtmlElement* element = (*node->begin())->GetElementIfStartEvent();
    CHECK(element != NULL) << "Only HtmlElements can cut across flush windows.";
    DCHECK(current_filter_ != NULL);
    open_deferred_nodes_[current_filter_] = DeferredNode(
        node, node_events);
  }

  current_ = node_last;
  skip_increment_ = true;

  node_events->splice(node_events->end(), queue_, node->begin(), node_last);
  need_sanity_check_ = true;

  // We will attempt to coalesce Characters nodes brought together as a
  // result of this 'defer', but this is not guaranteed to work at the end
  // of a flush window, and I don't think that guarantee would be worth
  // the complexity of pushing back the characters to the lexer.
  need_coalesce_characters_ = true;
}

void HtmlParse::RestoreDeferredNode(HtmlNode* deferred_node) {
  // There are two cases:
  //  1. The removed node is complete now.
  //  2. The removed node is incomplete (error).
  // Note: you cannot restore a node on a Flush.
  DCHECK(queue_.end() != current_);
  if (!IsRewritableIgnoringDeferral(deferred_node)) {
    LOG(DFATAL) << "A node cannot be replaced until it is complete";
    return;
  }

  DCHECK(deferred_deleted_nodes_.find(deferred_node) ==
         deferred_deleted_nodes_.end()) << "You cannot restore a deleted node";

  // Remove the previously deferred node from the list of deferred nodes.
  NodeToEventListMap::iterator p = deferred_nodes_.find(deferred_node);
  if (p == deferred_nodes_.end()) {
    LOG(DFATAL) << "Restoring a node that was not deferred";
    return;
  }
  HtmlEventList* event_list = p->second;
  deferred_nodes_.erase(p);

  // Correct the parent-pointer, as the new location for removed_node may be
  // higher or lower in the hierarchy. There is a special case for when we
  // restore on a start element, as the parent will be the current_ element.
  HtmlEvent* event = *current_;
  HtmlElement* new_parent = event->GetNode()->parent();
  if (event->GetElementIfStartEvent() != NULL) {
    new_parent = event->GetElementIfStartEvent();
  }
  deferred_node->set_parent(new_parent);

  NextEvent();
  queue_.splice(current_, *event_list,
                event_list->begin(), event_list->end());
  delete event_list;
  current_ = deferred_node->begin();
  DCHECK(!skip_increment_) << "Always false coming out of NextEvent()";
  need_sanity_check_ = true;

  // Like Defer above, we can attempt to coalesce characters here, but there
  // is no guarantee at flush-window boundaries, and I don't think it's worth
  // the complexity to provide one.
  need_coalesce_characters_ = true;
}

void HtmlParse::ClearDeferredNodes() {
  for (NodeToEventListMap::iterator p = deferred_nodes_.begin(),
           e = deferred_nodes_.end(); p != e; ++p) {
    const HtmlNode* node = p->first;
    HtmlEventList* events = p->second;
    if (deferred_deleted_nodes_.find(node) == deferred_deleted_nodes_.end()) {
      message_handler_->Message(
          kWarning, "Removed node %s never replaced", node->ToString().c_str());
    }
    STLDeleteElements(events);
    delete events;
  }
  deferred_nodes_.clear();
  deferred_deleted_nodes_.clear();
  open_deferred_nodes_.clear();
}

}  // namespace net_instaweb
