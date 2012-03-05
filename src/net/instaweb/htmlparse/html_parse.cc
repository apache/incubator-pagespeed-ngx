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

#include "net/instaweb/htmlparse/public/html_parse.h"

#include <list>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/htmlparse/html_event.h"
#include "net/instaweb/htmlparse/html_lexer.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_keywords.h"
#include "net/instaweb/htmlparse/public/html_filter.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"
#include "net/instaweb/util/public/arena.h"
#include "net/instaweb/util/public/atom.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/print_message_handler.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/symbol_table.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {
class DocType;

HtmlParse::HtmlParse(MessageHandler* message_handler)
    : lexer_(NULL),  // Can't initialize here, since "this" should not be used
                     // in the initializer list (it generates an error in
                     // Visual Studio builds).
      sequence_(0),
      current_(queue_.end()),
      message_handler_(message_handler),
      line_number_(1),
      deleted_current_(false),
      need_sanity_check_(false),
      coalesce_characters_(true),
      need_coalesce_characters_(false),
      url_valid_(false),
      log_rewrite_timing_(false),
      running_filters_(false),
      parse_start_time_us_(0),
      timer_(NULL) {
  lexer_ = new HtmlLexer(this);
  HtmlKeywords::Init();
}

HtmlParse::~HtmlParse() {
  delete lexer_;
  STLDeleteElements(&queue_);
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
  }
}

// Testing helper method
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
  HtmlFilter* listener = event_listener_.get();
  if (listener != NULL) {
    running_filters_ = true;
    event->Run(listener);
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
  element->set_sequence(sequence_++);
  if (IsOptionallyClosedTag(name.keyword())) {
    // When we programmatically insert HTML nodes we should default to
    // including an explicit close-tag if they are optionally closed
    // such as <html>, <body>, and <p>.
    element->set_close_style(HtmlElement::EXPLICIT_CLOSE);
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
  url.CopyToString(&url_);
  GoogleUrl gurl(url);
  url_valid_ = gurl.is_valid();
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
    AddEvent(new HtmlEndDocumentEvent(line_number_));
  }
}

void HtmlParse::EndFinishParse() {
  if (url_valid_) {
    ClearElements();
    ShowProgress("FinishParse");
  }
}

void HtmlParse::ParseText(const char* text, int size) {
  DCHECK(url_valid_) << "Invalid to call ParseText with invalid url";
  if (url_valid_) {
    lexer_->Parse(text, size);
  }
}

// This is factored out of Flush() for testing purposes.
void HtmlParse::ApplyFilter(HtmlFilter* filter) {
  if (coalesce_characters_ && need_coalesce_characters_) {
    CoalesceAdjacentCharactersNodes();
    need_coalesce_characters_ = false;
  }

  ShowProgress(StrCat("ApplyFilter:", filter->Name()).c_str());
  for (current_ = queue_.begin(); current_ != queue_.end(); ) {
    HtmlEvent* event = *current_;
    line_number_ = event->line_number();
    event->Run(filter);
    deleted_current_ = false;
    ++current_;
  }
  filter->Flush();

  if (need_sanity_check_) {
    SanityCheck();
    need_sanity_check_ = false;
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

void HtmlParse::CheckEventParent(HtmlEvent* event, HtmlElement* expect,
                                 HtmlElement* actual) {
  if ((expect != NULL) && (actual != expect)) {
    GoogleString actual_buf, expect_buf, event_buf;
    if (actual != NULL) {
      actual->ToString(&actual_buf);
    } else {
      actual_buf = "(null)";
    }
    expect->ToString(&expect_buf);
    event->ToString(&event_buf);
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

  HtmlFilter* listener = event_listener_.get();
  if (listener != NULL) {
    listener->Flush();
  }

  DCHECK(url_valid_) << "Invalid to call FinishParse with invalid url";
  if (url_valid_) {
    ShowProgress("Flush");

    for (int i = 0, n = filters_.size(); i < n; ++i) {
      HtmlFilter* filter = filters_[i];
      ApplyFilter(filter);
    }
    ClearEvents();
  }
}

void HtmlParse::ClearEvents() {
  // Detach all the elements from their events, as we are now invalidating
  // the events, but not the elements.
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
      } else {
        HtmlLeafNode* leaf_node = event->GetLeafNode();
        if (leaf_node != NULL) {
          leaf_node->set_iter(queue_.end());
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

void HtmlParse::AppendEventsToQueue(HtmlEventList* extra_events) {
  queue_.splice(queue_.end(), *extra_events);
}

HtmlEvent* HtmlParse::SplitQueueOnFirstEventInSet(
    const ConstHtmlEventSet& event_set,
    HtmlEventList* tail) {
  for (HtmlEventListIterator it = queue_.begin(); it != queue_.end(); ++it) {
    if (event_set.find(*it) != event_set.end()) {
      tail->splice(tail->end(), queue_, it, queue_.end());
      return *it;
    }
  }
  return NULL;
}

HtmlEvent* HtmlParse::GetEndElementEvent(const HtmlElement* element) {
  DCHECK (element != NULL);
  if (element->end() == queue_.end()) {
    return NULL;
  } else {
    return *(element->end());
  }
}

void HtmlParse::InsertElementBeforeElement(const HtmlNode* existing_node,
                                           HtmlNode* new_node) {
  // begin() == queue_.end() -> this is an invalid element.
  // TODO(sligocki): Rather than checks, we should probably return this as
  // the status.
  message_handler_->Check(existing_node->begin() != queue_.end(),
                          "InsertElementBeforeElement: existing_node invalid");
  new_node->set_parent(existing_node->parent());
  InsertElementBeforeEvent(existing_node->begin(), new_node);
}

void HtmlParse::InsertElementAfterElement(const HtmlNode* existing_node,
                                          HtmlNode* new_node) {
  message_handler_->Check(existing_node->end() != queue_.end(),
                          "InsertElementAfterElement: existing_node invalid");
  new_node->set_parent(existing_node->parent());
  InsertElementAfterEvent(existing_node->end(), new_node);
}

void HtmlParse::PrependChild(const HtmlElement* existing_parent,
                             HtmlNode* new_child) {
  message_handler_->Check(existing_parent->begin() != queue_.end(),
                          "PrependChild: existing_parent invalid");
  new_child->set_parent(const_cast<HtmlElement*>(existing_parent));
  InsertElementAfterEvent(existing_parent->begin(), new_child);
}

void HtmlParse::AppendChild(const HtmlElement* existing_parent,
                            HtmlNode* new_child) {
  message_handler_->Check(existing_parent->end() != queue_.end(),
                          "AppendChild: existing_parent invalid");
  new_child->set_parent(const_cast<HtmlElement*>(existing_parent));
  InsertElementBeforeEvent(existing_parent->end(), new_child);
}

void HtmlParse::InsertElementBeforeCurrent(HtmlNode* new_node) {
  if (deleted_current_) {
    FatalErrorHere("InsertElementBeforeCurrent after current has been "
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
      message_handler_->Check(node != NULL,
                              "Cannot compute parent for new node");
      new_node->set_parent(node->parent());
    }
  }
  InsertElementBeforeEvent(current_, new_node);
}

void HtmlParse::InsertElementBeforeEvent(const HtmlEventListIterator& event,
                                         HtmlNode* new_node) {
  need_sanity_check_ = true;
  need_coalesce_characters_ = true;
  new_node->SynthesizeEvents(event, &queue_);
}

void HtmlParse::InsertElementAfterEvent(const HtmlEventListIterator& event,
                                        HtmlNode* new_node) {
  message_handler_->Check(event != queue_.end(), "event == queue_.end()");
  HtmlEventListIterator next_event = event;
  ++next_event;
  InsertElementBeforeEvent(next_event, new_node);
}


void HtmlParse::InsertElementAfterCurrent(HtmlNode* new_node) {
  if (deleted_current_) {
    FatalErrorHere("InsertElementAfterCurrent after current has been "
                   "deleted.");
  }
  if (current_ == queue_.end()) {
    FatalErrorHere("InsertElementAfterCurrent called with queue at end.");
  }
  ++current_;
  InsertElementBeforeEvent(current_, new_node);

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
    InsertElementBeforeEvent(first->begin(), new_parent);
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
  if (current_ != queue_.end()) {
    HtmlNode* current_node = (*current_)->GetNode();
    if (MoveCurrentBeforeEvent(new_parent->end())) {
      current_node->set_parent(new_parent);
      moved = true;
    }
  } else {
    DebugLogQueue();
    LOG(DFATAL) << "MoveCurrentInto() called at queue_.end()";
  }
  return moved;
}

bool HtmlParse::MoveCurrentBefore(HtmlNode* element) {
  bool moved = false;
  DCHECK(current_ != queue_.end());
  if (current_ != queue_.end()) {
    HtmlNode* current_node = (*current_)->GetNode();
    if (MoveCurrentBeforeEvent(element->begin())) {
      current_node->set_parent(element->parent());
      moved = true;
    }
  } else {
    DebugLogQueue();
    LOG(DFATAL) << "MoveCurrentBefore() called at queue_.end()";
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

bool HtmlParse::DeleteElement(HtmlNode* node) {
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
      HtmlNode* nested_node = event->GetElementIfEndEvent();
      if (nested_node == NULL) {
        nested_node = event->GetLeafNode();
      }
      if (nested_node != NULL) {
        message_handler_->Check(nested_node->live(), "!nested_node->live()");
        nested_node->MarkAsDead(queue_.end());
      }

      // Check if we're about to delete the current event.
      bool move_current = (p == current_);
      p = queue_.erase(p);
      if (move_current) {
        current_ = p;  // p is the event *after* the old current.
        --current_;    // Go to *previous* event so that we don't skip p.
        deleted_current_ = true;
        line_number_ = (*current_)->line_number();
      }
      delete event;
    }

    // Our iteration should have covered the passed-in element as well.
    message_handler_->Check(!node->live(), "node->live()");
    deleted = true;
    need_sanity_check_ = true;
    need_coalesce_characters_ = true;
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
      queue_.splice(element->begin(), queue_, first, element->end());
      need_sanity_check_ = true;
      need_coalesce_characters_ = true;
    }
    deleted = DeleteElement(element);
  }
  return deleted;
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
    InsertElementBeforeElement(existing_node, new_node);
    replaced = DeleteElement(existing_node);
    message_handler_->Check(replaced, "!replaced");
  }
  return replaced;
}

HtmlElement* HtmlParse::CloneElement(HtmlElement* in_element) {
  HtmlElement* out_element = NewElement(NULL, in_element->name());
  out_element->set_close_style(in_element->close_style());
  for (int i = 0; i < in_element->attribute_size(); ++i) {
    out_element->AddAttribute(in_element->attribute(i));
  }
  return out_element;
}

bool HtmlParse::IsRewritable(const HtmlNode* node) const {
  return IsInEventWindow(node->begin()) && IsInEventWindow(node->end());
}

bool HtmlParse::IsInEventWindow(const HtmlEventListIterator& iter) const {
  return iter != queue_.end();
}

void HtmlParse::ClearElements() {
  nodes_.DestroyObjects();
  DCHECK(!running_filters_);
}

void HtmlParse::EmitQueue(MessageHandler* handler) {
  for (HtmlEventList::iterator p = queue_.begin(), e = queue_.end();
       p != e; ++p) {
    GoogleString buf;
    HtmlEvent* event = *p;
    event->ToString(&buf);
    handler->Message(kInfo, "%c %s (%p)\n",
                     p == current_ ? '*' : ' ',
                     buf.c_str(),
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
    HtmlElement* element, HtmlElement::CloseStyle close_style,
    int line_number) {
  HtmlEndElementEvent* end_event =
      new HtmlEndElementEvent(element, line_number);
  element->set_close_style(close_style);
  AddEvent(end_event);
  element->set_end(Last());
  element->set_end_line_number(line_number);
}

HtmlName HtmlParse::MakeName(HtmlName::Keyword keyword) {
  const char* str = HtmlKeywords::KeywordToString(keyword);
  return HtmlName(keyword, str);
}

HtmlName HtmlParse::MakeName(const StringPiece& str_piece) {
  HtmlName::Keyword keyword = HtmlName::Lookup(str_piece);
  const char* str = HtmlKeywords::KeywordToString(keyword);

  // If the passed-in string is not in its canonical form, or is not a
  // recognized keyword, then we must make a permanent copy in our
  // string table.  Note that we are comparing the bytes of the
  // keyword from the table, not the pointer.
  if ((str == NULL) || (str_piece != str)) {
    Atom atom = string_table_.Intern(str_piece);
    str = atom.c_str();
  }
  return HtmlName(keyword, str);
}

void HtmlParse::set_event_listener(HtmlFilter* listener) {
  event_listener_.reset(listener);
}

}  // namespace net_instaweb
