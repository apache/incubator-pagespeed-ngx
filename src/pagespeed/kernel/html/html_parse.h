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

#ifndef PAGESPEED_KERNEL_HTML_HTML_PARSE_H_
#define PAGESPEED_KERNEL_HTML_HTML_PARSE_H_

#include <cstdarg>
#include <cstddef>
#include <list>
#include <set>
#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/arena.h"
#include "pagespeed/kernel/base/printf_format.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/symbol_table.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

class DocType;
class HtmlEvent;
class HtmlFilter;
class HtmlLexer;
class MessageHandler;
class Timer;

typedef std::set <const HtmlEvent*> ConstHtmlEventSet;

// Streaming Html Parser API.  Callbacks defined in HtmlFilter are
// called on each parser token.
//
// Any number of filters can be added to the Html Parser; they are
// organized in a chain.  Each filter processes a stream of SAX events
// (HtmlEvent), interspersed by Flushes.  The filter operates on the
// sequence of events between flushes (a flush-window), and the system
// passes the (possibly mutated) event-stream to the next filter.
//
// An HTML Event is a lexical token provided by the parser, including:
//     begin document
//     end document
//     begin element
//     end element
//     whitespace
//     characters
//     cdata
//     comment
//
// The parser retains the sequence of events as a data structure:
// list<HtmlEvent>.  HtmlEvents are sent to filters (HtmlFilter), as follows:
//   foreach filter in filter-chain
//     foreach event in flush-window
//       apply filter to event
//
// Filters may mutate the event streams as they are being processed,
// and these mutations be seen by downstream filters.  The filters can
// mutate any event that has not been flushed.  Supported mutations include:
//   - Removing an HTML element whose begin/end tags are both within
//     the flush window.  This will also remove any nested elements.
//   - Removing other HTML events
//   - Inserting new elements (automatically inserts begin/end events)
//     before or after "current" event
//   - Inserting new events, before or after "current" event
class HtmlParse {
 public:
  explicit HtmlParse(MessageHandler* message_handler);
  virtual ~HtmlParse();

  // Application methods for parsing functions and adding filters

  // Add a new html filter to the filter-chain, without taking ownership
  // of it.
  void AddFilter(HtmlFilter* filter);

  // Initiate a chunked parsing session.  Finish with FinishParse.  The
  // url is only used to resolve relative URLs; the contents are not
  // directly fetched.  The caller must supply the text and call ParseText.
  //
  // Returns whether the URL is valid.
  bool StartParse(const StringPiece& url) {
    return StartParseWithType(url, kContentTypeHtml);
  }
  bool StartParseWithType(const StringPiece& url,
                          const ContentType& content_type) {
    return StartParseId(url, url, content_type);
  }

  // Returns whether the google_url() URL is valid.
  bool is_url_valid() const { return url_valid_; }

  // Mostly useful for file-based rewriters so that messages can reference
  // the HTML file and produce navigable errors.
  //
  // Returns whether the URL is valid.
  virtual bool StartParseId(const StringPiece& url, const StringPiece& id,
                            const ContentType& content_type);

  // Sets url() for test purposes. Normally this is done by StartParseId,
  // but sometimes tests need to set it without worrying about parse
  // state.
  void SetUrlForTesting(const StringPiece& url);

  // Parses an arbitrary block of an html file, queuing up the events.  Call
  // Flush to send the events through the Filter.
  //
  // To parse an entire file, first call StartParse(), then call
  // ParseText on the file contents (in whatever size chunks are convenient),
  // then call FinishParse().
  //
  // It is invalid to call ParseText when the StartParse* routines returned
  // false.
  void ParseText(const char* content, int size) {
    ParseTextInternal(content, size);
  }
  void ParseText(const StringPiece& sp) {
    ParseTextInternal(sp.data(), sp.size());
  }

  // Flush the currently queued events through the filters.  It is desirable
  // for large web pages, particularly dynamically generated ones, to start
  // getting delivered to the browser as soon as they are ready.  On the
  // other hand, rewriting is more powerful when more of the content can
  // be considered for image/css/js spriting.  This method should be called
  // when the controlling network process wants to induce a new chunk of
  // output.  The less you call this function the better the rewriting will
  // be.
  //
  // It is invalid to call Flush when the StartParse* routines returned
  // false.
  //
  // If this is called from a Filter, the request will be deferred until after
  // currently active filters are completed.
  virtual void Flush();

  // Finish a chunked parsing session.  This also induces a Flush.
  //
  // It is invalid to call FinishParse when the StartParse* routines returned
  // false.
  virtual void FinishParse();


  // Utility methods for implementing filters

  HtmlCdataNode* NewCdataNode(HtmlElement* parent,
                              const StringPiece& contents);
  HtmlCharactersNode* NewCharactersNode(HtmlElement* parent,
                                        const StringPiece& literal);
  HtmlCommentNode* NewCommentNode(HtmlElement* parent,
                                  const StringPiece& contents);
  HtmlDirectiveNode* NewDirectiveNode(HtmlElement* parent,
                                      const StringPiece& contents);
  HtmlIEDirectiveNode* NewIEDirectiveNode(HtmlElement* parent,
                                          const StringPiece& contents);

  // DOM-manipulation methods.
  // TODO(sligocki): Find Javascript equivalents and list them or even change
  // our names to be consistent.

  // This and downstream filters will then see inserted elements but upstream
  // filters will not.

  // Note: In Javascript the first is called insertBefore and takes the arg
  // in the opposite order.
  // Note: new_node must not already be in the DOM.
  void InsertNodeBeforeNode(const HtmlNode* existing_node, HtmlNode* new_node);
  void InsertNodeAfterNode(const HtmlNode* existing_node, HtmlNode* new_node);

  // These are a backwards-compatibility wrapper for use by Pagespeed Insights.
  // TODO(morlovich): Remove them after PSI is synced.
  void InsertElementBeforeElement(const HtmlNode* existing_element,
                                  HtmlNode* new_element) {
    InsertNodeBeforeNode(existing_element, new_element);
  }

  void InsertElementAfterElement(const HtmlNode* existing_element,
                                 HtmlNode* new_element) {
    InsertNodeAfterNode(existing_element, new_element);
  }

  // Add a new child element at the beginning or end of existing_parent's
  // children. Named after Javascript's appendChild method.
  // Note: new_child must not already be in the DOM.
  void PrependChild(const HtmlElement* existing_parent, HtmlNode* new_child);
  void AppendChild(const HtmlElement* existing_parent, HtmlNode* new_child);

  // Insert a new element before the current one.  current_ remains unchanged.
  // Note: new_node must not already be in the DOM.
  void InsertNodeBeforeCurrent(HtmlNode* new_node);

  // Insert a new element after the current one, moving current_ to the new
  // element.  In a Filter, the flush-loop will advance past this on
  // the next iteration.
  // Note: new_node must not already be in the DOM.
  void InsertNodeAfterCurrent(HtmlNode* new_node);

  // Enclose element around two elements in a sequence.  The first
  // element must be the same as, or precede the last element in the
  // event-stream, and this is not checked, but the two elements do
  // not need to be adjacent.  They must have the same parent to start
  // with.
  bool AddParentToSequence(HtmlNode* first, HtmlNode* last,
                           HtmlElement* new_parent);

  // Moves current node (and all children) to an already-existing parent,
  // where they will be placed as the last elements in that parent.
  // Returns false if the operation could not be performed because either
  // the node or its parent was partially or wholly flushed.
  // Note: Will not work if called from StartElement() event.
  //
  // This differs from AppendChild() because it moves the current node,
  // which is already in the DOM, rather than adding a new node.
  bool MoveCurrentInto(HtmlElement* new_parent);

  // Moves current node (and all children) directly before existing_node.
  // Note: Will not work if called from StartElement() event.
  //
  // This differs from InsertNodeBeforeNode() because it moves the
  // current node, which is already in the DOM, rather than adding a new node.
  bool MoveCurrentBefore(HtmlNode* existing_node);

  // If the given node is rewritable, delete it and all of its children (if
  // any) and return true; otherwise, do nothing and return false.
  // Note: Javascript appears to use removeChild for this.
  bool DeleteNode(HtmlNode* node);

  // Delete a parent element, retaining any children and moving them to
  // reside under the parent's parent.
  bool DeleteSavingChildren(HtmlElement* element);

  // Determines whether the element, in the context of its flush
  // window, has children.  If the element is not rewritable, or
  // has not been closed yet, or inserted into the DOM event stream,
  // then 'false' is returned.
  //
  // Note that the concept of the Flush Window is important because the
  // knowledge of an element's children is not limited to the current
  // event being presented to a Filter.  A Filter can call this method
  // in the StartElement of an event to see if any children are going
  // to be coming.  Of course, if the StartElement is at the end of a
  // Flush window, then we won't know about the children, but IsRewritable
  // will also be false.
  bool HasChildrenInFlushWindow(HtmlElement* element);

  // If possible, replace the existing node with the new node and return true;
  // otherwise, do nothing and return false.
  bool ReplaceNode(HtmlNode* existing_node, HtmlNode* new_node);

  // Creates an another element with the same name and attributes as in_element.
  // Does not duplicate the children or insert it anywhere.
  HtmlElement* CloneElement(HtmlElement* in_element);

  HtmlElement* NewElement(HtmlElement* parent, const StringPiece& str) {
    return NewElement(parent, MakeName(str));
  }
  HtmlElement* NewElement(HtmlElement* parent, HtmlName::Keyword keyword) {
    return NewElement(parent, MakeName(keyword));
  }
  HtmlElement* NewElement(HtmlElement* parent, const HtmlName& name);

  void AddAttribute(HtmlElement* element, HtmlName::Keyword keyword,
                    const StringPiece& value) {
    return element->AddAttribute(MakeName(keyword), value,
                                 HtmlElement::DOUBLE_QUOTE);
  }
  void AddAttribute(HtmlElement* element, StringPiece name,
                    const StringPiece& value) {
    return element->AddAttribute(MakeName(name), value,
                                 HtmlElement::DOUBLE_QUOTE);
  }
  void AddEscapedAttribute(HtmlElement* element, HtmlName::Keyword keyword,
                    const StringPiece& escaped_value) {
    return element->AddEscapedAttribute(MakeName(keyword), escaped_value,
                                        HtmlElement::DOUBLE_QUOTE);
  }
  void AddAttribute(HtmlElement* element, HtmlName::Keyword keyword,
                    int value) {
    return AddAttribute(element, keyword, IntegerToString(value));
  }
  void SetAttributeName(HtmlElement::Attribute* attribute,
                        HtmlName::Keyword keyword) {
    attribute->set_name(MakeName(keyword));
  }

  HtmlName MakeName(const StringPiece& str);
  HtmlName MakeName(HtmlName::Keyword keyword);

  bool IsRewritable(const HtmlNode* node) const;
  // IsRewritable will return false for a node if either the open or close tag
  // has been flushed, but this is too conservative if we only want to call
  // AppendChild on that node, since we can append even if the open tag has
  // already been flushed.
  bool CanAppendChild(const HtmlNode* node) const;

  void ClearElements();

  // Log the HtmlEvent queue_ to the message_handler_ for debugging.
  void DebugLogQueue();

  // Print the HtmlEvent queue_ to stdout for debugging.
  void DebugPrintQueue();

  // Implementation helper with detailed knowledge of html parsing libraries
  friend class HtmlLexer;

  // Determines whether a tag should be terminated in HTML, e.g. <meta ..>.
  // We do not expect to see a close-tag for meta and should never insert one.
  bool IsImplicitlyClosedTag(HtmlName::Keyword keyword) const;

  // Determines whether a tag should be interpreted as a 'literal'
  // tag. That is, a tag whose contents are not parsed until a
  // corresponding matching end tag is encountered.
  static bool IsLiteralTag(HtmlName::Keyword keyword);

  // Determines whether a tag is interpreted as a 'literal' tag in
  // some user agents. Since some user agents will interpret the
  // contents of these tags, our parser never treats them as literal
  // tags. However, a filter that wants to insert new tags that should
  // be processed by all user agents should not insert those tags into
  // a tag that is sometimes parsed as a literal tag. Those filters
  // can use this method to determine if they are within such a tag.
  static bool IsSometimesLiteralTag(HtmlName::Keyword keyword);

  // An optionally closed tag ranges from <p>, which is typically not closed,
  // but we infer the closing from context.  Also consider <html>, which usually
  // is closed but not always.  E.g. www.google.com does not close its html tag.
  bool IsOptionallyClosedTag(HtmlName::Keyword keyword) const;

  // Determines whether a tag allows brief termination in HTML, e.g. <tag/>
  bool TagAllowsBriefTermination(HtmlName::Keyword keyword) const;

  MessageHandler* message_handler() const { return message_handler_; }
  // Gets the current location information; typically to help with error
  // messages.
  const char* url() const { return url_.c_str(); }
  // Gets a parsed GoogleUrl& corresponding to url().
  const GoogleUrl& google_url() const { return google_url_; }
  const char* id() const { return id_.c_str(); }
  int line_number() const { return line_number_; }
  // Returns URL (or id) and line number as a string, to be used in messages.
  GoogleString UrlLine() const {
    return StringPrintf("%s:%d", id(), line_number());
  }

  // Return the current assumed doctype of the document (based on the content
  // type and any HTML directives encountered so far).
  const DocType& doctype() const;

  // Interface for any caller to report an error message via the message handler
  void Info(const char* filename, int line, const char* msg, ...)
      INSTAWEB_PRINTF_FORMAT(4, 5);
  void Warning(const char* filename, int line, const char* msg, ...)
      INSTAWEB_PRINTF_FORMAT(4, 5);
  void Error(const char* filename, int line, const char* msg, ...)
      INSTAWEB_PRINTF_FORMAT(4, 5);
  void FatalError(const char* filename, int line, const char* msg, ...)
      INSTAWEB_PRINTF_FORMAT(4, 5);

  void InfoV(const char* file, int line, const char *msg, va_list args);
  void WarningV(const char* file, int line, const char *msg, va_list args);
  void ErrorV(const char* file, int line, const char *msg, va_list args);
  void FatalErrorV(const char* file, int line, const char* msg, va_list args);

  // Report error message with current parsing filename and linenumber.
  void InfoHere(const char* msg, ...) INSTAWEB_PRINTF_FORMAT(2, 3);
  void WarningHere(const char* msg, ...) INSTAWEB_PRINTF_FORMAT(2, 3);
  void ErrorHere(const char* msg, ...) INSTAWEB_PRINTF_FORMAT(2, 3);
  void FatalErrorHere(const char* msg, ...) INSTAWEB_PRINTF_FORMAT(2, 3);

  // If set_log_rewrite_timing(true) has been called, logs the given message
  // at info level with a timeset offset from the parsing start time,
  void ShowProgress(const char* message);

  void InfoHereV(const char *msg, va_list args) {
    InfoV(id_.c_str(), line_number_, msg, args);
  }
  void WarningHereV(const char *msg, va_list args) {
    WarningV(id_.c_str(), line_number_, msg, args);
  }
  void ErrorHereV(const char *msg, va_list args) {
    ErrorV(id_.c_str(), line_number_, msg, args);
  }
  void FatalErrorHereV(const char* msg, va_list args) {
    FatalErrorV(id_.c_str(), line_number_, msg, args);
  }

  void AddElement(HtmlElement* element, int line_number);
  void CloseElement(HtmlElement* element, HtmlElement::CloseStyle close_style,
                    int line_number);

  // Run a filter on the current queue of parse nodes.
  void ApplyFilter(HtmlFilter* filter);

  // Provide timer to helping to report timing of each filter.  You must also
  // set_log_rewrite_timing(true) to turn on this reporting.
  void set_timer(Timer* timer) { timer_ = timer; }
  Timer* timer() const { return timer_; }
  void set_log_rewrite_timing(bool x) { log_rewrite_timing_ = x; }

  // Adds a filter to be called during parsing as new events are added.
  // Takes ownership of the HtmlFilter passed in.
  void add_event_listener(HtmlFilter* listener);

  // Inserts a comment before or after the current node.  The function tries to
  // pick an intelligent place depending on the document structure and
  // whether the current node is a start-element, end-element, or a leaf.
  // Returns true if it successfully added the comment, and false if it was not
  // safe for the comment to be inserted. This can happen when a comment is
  // inserted in a literal element (script or style) after the opening tag has
  // been flushed, but the closing tag has not been seen yet. In this case, the
  // caller can buffer the messages until EndElement is reached and call
  // InsertComment at that point.
  bool InsertComment(StringPiece sp);

  // Sets the limit on the maximum number of bytes that should be parsed.
  void set_size_limit(int64 x);
  // Returns whether we have exceeded the size limit.
  bool size_limit_exceeded() const;

 protected:
  typedef std::vector<HtmlFilter*> FilterVector;
  typedef std::list<HtmlFilter*> FilterList;

  // HtmlParse::FinishParse() is equivalent to the sequence of
  // BeginFinishParse(); Flush(); EndFinishParse().
  // Split up to permit asynchronous versions.
  void BeginFinishParse();
  void EndFinishParse();

  // Clears any cached state we have while this object is laying
  // around for recycling.
  void Clear();

  // Returns the number of events on the event queue.
  size_t GetEventQueueSize();

  virtual void ParseTextInternal(const char* content, int size);

  // Calls DetermineEnabledFiltersImpl in an idempotent way.
  void DetermineEnabledFilters() {
    if (!determine_enabled_filters_called_) {
      determine_enabled_filters_called_ = true;
      DetermineEnabledFiltersImpl();
    }
  }

  // Call DetermineEnabled() on each filter. Should be called after
  // the property cache lookup has finished since some filters depend on
  // pcache results in their DetermineEnabled implementation. If a subclass has
  // filters that the base HtmlParse doesn't know about, it should override this
  // function and call DetermineEnabled on each of its filters, along with
  // calling the base DetermineEnabledFiltersImpl.
  virtual void DetermineEnabledFiltersImpl();

 private:
  void ApplyFilterHelper(HtmlFilter* filter);
  HtmlEventListIterator Last();  // Last element in queue
  bool IsInEventWindow(const HtmlEventListIterator& iter) const;
  void InsertNodeBeforeEvent(const HtmlEventListIterator& event,
                             HtmlNode* new_node);
  void InsertNodeAfterEvent(const HtmlEventListIterator& event,
                            HtmlNode* new_node);
  bool MoveCurrentBeforeEvent(const HtmlEventListIterator& move_to);
  bool IsDescendantOf(const HtmlNode* possible_child,
                      const HtmlNode* possible_parent);
  void SanityCheck();
  void CheckEventParent(HtmlEvent* event, HtmlElement* expect,
                        HtmlElement* actual);
  void CheckParentFromAddEvent(HtmlEvent* event);
  void FixParents(const HtmlEventListIterator& begin,
                  const HtmlEventListIterator& end_inclusive,
                  HtmlElement* new_parent);
  void CoalesceAdjacentCharactersNodes();
  void ClearEvents();
  void EmitQueue(MessageHandler* handler);

  // Visible for testing only, via HtmlTestingPeer
  friend class HtmlTestingPeer;
  void AddEvent(HtmlEvent* event);
  void SetCurrent(HtmlNode* node);
  void set_coalesce_characters(bool x) { coalesce_characters_ = x; }
  size_t symbol_table_size() const {
    return string_table_.string_bytes_allocated();
  }

  // If a FLUSH occurs in the middle of a script, style, or other tag
  // whose contents can only be a Characters block, then we will buffer
  // up the start of the script tag and not emit it and the Characters block
  // until after we see the close script tag.  This function enforces that
  // right before calling the Filters.
  void DelayLiteralTag();

  FilterVector event_listeners_;
  SymbolTableSensitive string_table_;
  FilterVector filters_;
  HtmlLexer* lexer_;
  Arena<HtmlNode> nodes_;
  HtmlEventList queue_;
  HtmlEventListIterator current_;
  // Have we deleted current? Then we shouldn't do certain manipulations to it.
  MessageHandler* message_handler_;
  GoogleString url_;
  GoogleUrl google_url_;
  GoogleString id_;  // Per-request identifier string used in error messages.
  int line_number_;
  bool deleted_current_;
  bool skip_increment_;
  bool determine_enabled_filters_called_;
  bool need_sanity_check_;
  bool coalesce_characters_;
  bool need_coalesce_characters_;
  bool url_valid_;
  bool log_rewrite_timing_;  // Should we time the speed of parsing?
  bool running_filters_;
  int64 parse_start_time_us_;
  scoped_ptr<HtmlEvent> delayed_start_literal_;
  Timer* timer_;

  DISALLOW_COPY_AND_ASSIGN(HtmlParse);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTML_HTML_PARSE_H_
