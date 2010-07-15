/**
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
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"
#include "net/instaweb/util/public/printf_format.h"
#include <string>
#include "net/instaweb/util/public/symbol_table.h"

namespace net_instaweb {

class HtmlParse {
 public:
  explicit HtmlParse(MessageHandler* message_handler);
  ~HtmlParse();

  // Application methods for parsing functions and adding filters

  // Add a new html filter to the filter-chain
  void AddFilter(HtmlFilter* filter);

  // Initiate a chunked parsing session.  Finish with FinishParse.  The
  // url_or_filename is only used for error messages; the contents are not
  // directly fetched.  The caller must supply the text and call ParseText.
  void StartParse(const char* url_or_filename);

  // Parses an arbitrary block of an html file, queuing up the events.  Call
  // Flush to send the events through the Filter.
  //
  // To parse an entire file, first call StartParse(filename), then call
  // ParseText on the file contents (in whatever size chunks are convenient),
  // then call FinishParse().
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
  void Flush();

  // Finish a chunked parsing session.  This also induces a Flush.
  void FinishParse();


  // Utility methods for implementing filters

  HtmlCdataNode* NewCdataNode(HtmlElement* parent,
                              const std::string& contents);
  HtmlCharactersNode* NewCharactersNode(HtmlElement* parent,
                                        const std::string& literal);
  HtmlCommentNode* NewCommentNode(HtmlElement* parent,
                                  const std::string& contents);
  HtmlDirectiveNode* NewDirectiveNode(HtmlElement* parent,
                                      const std::string& contents);

  // TODO(mdsteele): Rename these methods to e.g. InsertNodeBeforeNode.
  // This and downstream filters will then see inserted elements but upstream
  // filters will not.
  bool InsertElementBeforeElement(const HtmlNode* existing_node,
                                  HtmlNode* new_node);
  bool InsertElementAfterElement(const HtmlNode* existing_node,
                                 HtmlNode* new_node);
  // Insert element before current event.
  bool InsertElementBeforeCurrent(HtmlNode* node);


  // Enclose element around two elements in a sequence.  The first
  // element must precede the second element in the event-stream, and
  // this is not checked, but the two elements do not need to be adjacent.
  // They must have the same parent to start with.
  bool AddParentToSequence(HtmlNode* first, HtmlNode* last,
                           HtmlElement* new_parent);

  HtmlElement* NewElement(HtmlElement* parent, Atom tag);

  // If the given node is rewritable, delete it and all of its children (if
  // any) and return true; otherwise, do nothing and return false.
  bool DeleteElement(HtmlNode* node);

  // If possible, replace the existing node with the new node and return true;
  // otherwise, do nothing and return false.
  bool ReplaceNode(HtmlNode* existing_node, HtmlNode* new_node);

  bool IsRewritable(const HtmlNode* node) const;

  void ClearElements();

  void DebugPrintQueue();  // Print queue (for debugging)

  Atom Intern(const std::string& name) {
    return string_table_.Intern(name);
  }
  Atom Intern(const char* name) {
    return string_table_.Intern(name);
  }

  // Implementation helper with detailed knowledge of html parsing libraries
  friend class HtmlLexer;

  // Determines whether a tag should be terminated in HTML.
  bool IsImplicitlyClosedTag(Atom tag) const;

  // Determines whether a tag allows brief termination in HTML, e.g. <tag/>
  bool TagAllowsBriefTermination(Atom tag) const;

  MessageHandler* message_handler() const { return message_handler_; }
  // Gets the current location information; typically to help with error
  // messages.
  const char* filename() const { return filename_.c_str(); }
  int line_number() const { return line_number_; }

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
    InfoV(filename_.c_str(), line_number_, msg, args);
  }
  void WarningHereV(const char *msg, va_list args) {
    WarningV(filename_.c_str(), line_number_, msg, args);
  }
  void ErrorHereV(const char *msg, va_list args) {
    ErrorV(filename_.c_str(), line_number_, msg, args);
  }
  void FatalErrorHereV(const char* msg, va_list args) {
    FatalErrorV(filename_.c_str(), line_number_, msg, args);
  }

  void CloseElement(HtmlElement* element, HtmlElement::CloseStyle close_style,
                    int line_number);

  // Run a filter on the current queue of parse nodes.  This is visible
  // for testing.
  void ApplyFilter(HtmlFilter* filter);

  // Visible for testing only.
  // TODO(jmarantz): Consider using a Peer class that is declared as a friend,
  // and instantiating that from the tests.
  void AddEvent(HtmlEvent* event);

 private:
  void AddElement(HtmlElement* element, int line_number);
    HtmlEventListIterator Last();  // Last element in queue
  bool IsInEventWindow(const HtmlEventListIterator& iter) const;
  bool InsertElementBeforeEvent(const HtmlEventListIterator& event,
                                HtmlNode* new_node);
  void SanityCheck();
  void CheckEventParent(HtmlEvent* event, HtmlElement* expect,
                        HtmlElement* actual);
  void CheckParentFromAddEvent(HtmlEvent* event);

  SymbolTableInsensitive string_table_;
  std::vector<HtmlFilter*> filters_;
  HtmlLexer* lexer_;
  int sequence_;
  std::set<HtmlNode*> nodes_;
  HtmlEventList queue_;
  HtmlEventListIterator current_;
  // Have we deleted current? Then we shouldn't do certain manipulations to it.
  bool deleted_current_;
  MessageHandler* message_handler_;
  std::string filename_;
  int line_number_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_PARSE_H_
