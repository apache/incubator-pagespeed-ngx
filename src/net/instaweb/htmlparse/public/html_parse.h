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

#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_PARSE_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_PARSE_H_

#include <stdarg.h>
#include <set>
#include <vector>
#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/doctype.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"
#include "net/instaweb/util/public/arena.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/printf_format.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/symbol_table.h"

namespace net_instaweb {

class Timer;

// TODO(jmarantz): rename HtmlParse to HtmlContext.  The actual
// parsing occurs in HtmlLexer, and this class is dominated by methods
// to manipulate DOM as it streams through.
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
  bool StartParse(const StringPiece& url) {
    return StartParseWithType(url, kContentTypeHtml);
  }
  bool StartParseWithType(const StringPiece& url,
                          const ContentType& content_type) {
    return StartParseId(url, url, content_type);
  }

  // Returns whether the gurl() URL is valid.
  bool is_url_valid() const { return url_valid_; }

  // Use an error message id that is distinct from the url.
  // Mostly useful for file-based rewriters so that messages can reference
  // the HTML file and produce navigable errors.
  virtual bool StartParseId(const StringPiece& url, const StringPiece& id,
                            const ContentType& content_type);

  // Parses an arbitrary block of an html file, queuing up the events.  Call
  // Flush to send the events through the Filter.
  //
  // To parse an entire file, first call StartParse(), then call
  // ParseText on the file contents (in whatever size chunks are convenient),
  // then call FinishParse().
  //
  // It is invalid to call ParseText when the StartParse* routines returned
  // false.
  void ParseText(const char* content, int size);
  void ParseText(const StringPiece& sp) { ParseText(sp.data(), sp.size()); }

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
  void Flush();

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

  // TODO(mdsteele): Rename these methods to e.g. InsertNodeBeforeNode.
  // This and downstream filters will then see inserted elements but upstream
  // filters will not.
  // Note: In Javascript the first is called insertBefore and takes the arg
  // in the oposite order.
  void InsertElementBeforeElement(const HtmlNode* existing_node,
                                  HtmlNode* new_node);
  void InsertElementAfterElement(const HtmlNode* existing_node,
                                 HtmlNode* new_node);

  // Add child element at the begining or end of existing_parent's children.
  // Named after Javascript's appendChild method.
  void PrependChild(const HtmlElement* existing_parent, HtmlNode* new_child);
  void AppendChild(const HtmlElement* existing_parent, HtmlNode* new_child);

  // Insert element before the current one.  current_ remains unchanged.
  void InsertElementBeforeCurrent(HtmlNode* node);

  // Insert element after the current one, moving current_ to the new
  // element.  In a Filter, the flush-loop will advance past this on
  // the next iteration.
  void InsertElementAfterCurrent(HtmlNode* node);

  // Enclose element around two elements in a sequence.  The first
  // element must be the same as, or precede the last element in the
  // event-stream, and this is not checked, but the two elements do
  // not need to be adjacent.  They must have the same parent to start
  // with.
  //
  // This differs from MoveSequenceToParent in that the new parent is
  // not yet in the DOM tree, and will be inserted around the
  // elements.
  bool AddParentToSequence(HtmlNode* first, HtmlNode* last,
                           HtmlElement* new_parent);

  // Moves a node-sequence to an already-existing parent, where they
  // will be placed as the last elements in that parent.  Returns false
  // if the operation could not be performed because either the node
  // or its parent was partially or wholy flushed.
  //
  // This differs from AddParentToSequence in that the parent is already
  // in the DOM-tree.
  bool MoveCurrentInto(HtmlElement* new_parent);

  // If the given node is rewritable, delete it and all of its children (if
  // any) and return true; otherwise, do nothing and return false.
  // Note: Javascript appears to use removeChild for this.
  bool DeleteElement(HtmlNode* node);

  // Delete a parent element, retaining any children and moving them to
  // reside under the parent's parent.
  bool DeleteSavingChildren(HtmlElement* element);

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
    return element->AddAttribute(MakeName(keyword), value, "\"");
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

  void ClearElements();

  void DebugPrintQueue();  // Print queue (for debugging)

  // Implementation helper with detailed knowledge of html parsing libraries
  friend class HtmlLexer;

  // Determines whether a tag should be terminated in HTML.
  bool IsImplicitlyClosedTag(HtmlName::Keyword keyword) const;

  // Determines whether a tag allows brief termination in HTML, e.g. <tag/>
  bool TagAllowsBriefTermination(HtmlName::Keyword keyword) const;

  MessageHandler* message_handler() const { return message_handler_; }
  // Gets the current location information; typically to help with error
  // messages.
  const char* url() const { return url_.c_str(); }
  // Gets a parsed GURL& corresponding to url().
  const GURL& gurl() const { return google_url_.gurl(); }
  const GoogleUrl& google_url() const { return google_url_; }
  const char* id() const { return id_.c_str(); }
  int line_number() const { return line_number_; }
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

  // Run a filter on the current queue of parse nodes.  This is visible
  // for testing.
  void ApplyFilter(HtmlFilter* filter);

  // Provide timer to helping to report timing of each filter.  You must also
  // set_log_rewrite_timing(true) to turn on this reporting.
  void set_timer(Timer* timer) { timer_ = timer; }
  void set_log_rewrite_timing(bool x) { log_rewrite_timing_ = x; }

 private:
  HtmlEventListIterator Last();  // Last element in queue
  bool IsInEventWindow(const HtmlEventListIterator& iter) const;
  void InsertElementBeforeEvent(const HtmlEventListIterator& event,
                                HtmlNode* new_node);
  void InsertElementAfterEvent(const HtmlEventListIterator& event,
                               HtmlNode* new_node);
  void SanityCheck();
  void CheckEventParent(HtmlEvent* event, HtmlElement* expect,
                        HtmlElement* actual);
  void CheckParentFromAddEvent(HtmlEvent* event);
  void FixParents(const HtmlEventListIterator& begin,
                  const HtmlEventListIterator& end_inclusive,
                  HtmlElement* new_parent);
  void CoalesceAdjacentCharactersNodes();
  void ShowProgress(const char* message);

  // Visible for testing only, via HtmlTestingPeer
  friend class HtmlTestingPeer;
  void AddEvent(HtmlEvent* event);
  void SetCurrent(HtmlNode* node);
  void set_coalesce_characters(bool x) { coalesce_characters_ = x; }
  size_t symbol_table_size() const {
    return string_table_.string_bytes_allocated();
  }

  SymbolTableSensitive string_table_;
  std::vector<HtmlFilter*> filters_;
  HtmlLexer* lexer_;
  int sequence_;
  Arena<HtmlNode> nodes_;
  HtmlEventList queue_;
  HtmlEventListIterator current_;
  // Have we deleted current? Then we shouldn't do certain manipulations to it.
  MessageHandler* message_handler_;
  std::string url_;
  GoogleUrl google_url_;
  std::string id_;  // Per-request identifier string used in error messages.
  int line_number_;
  bool deleted_current_;
  bool need_sanity_check_;
  bool coalesce_characters_;
  bool need_coalesce_characters_;
  bool url_valid_;
  bool log_rewrite_timing_;  // Should we time the speed of parsing?
  int64 parse_start_time_us_;
  Timer* timer_;

  DISALLOW_COPY_AND_ASSIGN(HtmlParse);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_PARSE_H_
