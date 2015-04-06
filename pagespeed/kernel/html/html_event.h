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

#ifndef PAGESPEED_KERNEL_HTML_HTML_EVENT_H_
#define PAGESPEED_KERNEL_HTML_HTML_EVENT_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"
#include "pagespeed/kernel/html/html_node.h"

namespace net_instaweb {

class HtmlEvent {
 public:
  explicit HtmlEvent(int line_number) : line_number_(line_number) {
  }
  virtual ~HtmlEvent();
  virtual void Run(HtmlFilter* filter) = 0;
  virtual GoogleString ToString() const = 0;

  // If this is a StartElement event, returns the HtmlElement that is being
  // started.  Otherwise returns NULL.
  virtual HtmlElement* GetElementIfStartEvent() { return NULL; }

  // If this is an EndElement event, returns the HtmlElement that is being
  // ended.  Otherwise returns NULL.
  virtual HtmlElement* GetElementIfEndEvent() { return NULL; }

  virtual HtmlLeafNode* GetLeafNode() { return NULL; }
  virtual HtmlNode* GetNode() { return NULL; }
  virtual HtmlCharactersNode* GetCharactersNode() { return NULL; }
  void DebugPrint();

  int line_number() const { return line_number_; }

 private:
  int line_number_;

  DISALLOW_COPY_AND_ASSIGN(HtmlEvent);
};

class HtmlStartDocumentEvent: public HtmlEvent {
 public:
  explicit HtmlStartDocumentEvent(int line_number) : HtmlEvent(line_number) {}
  virtual void Run(HtmlFilter* filter) { filter->StartDocument(); }
  virtual GoogleString ToString() const { return "StartDocument"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(HtmlStartDocumentEvent);
};

class HtmlEndDocumentEvent: public HtmlEvent {
 public:
  explicit HtmlEndDocumentEvent(int line_number) : HtmlEvent(line_number) {}
  virtual void Run(HtmlFilter* filter) { filter->EndDocument(); }
  virtual GoogleString ToString() const { return "EndDocument"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(HtmlEndDocumentEvent);
};

class HtmlStartElementEvent: public HtmlEvent {
 public:
  HtmlStartElementEvent(HtmlElement* element, int line_number)
      : HtmlEvent(line_number),
        element_(element) {
  }
  virtual void Run(HtmlFilter* filter) { filter->StartElement(element_); }
  virtual GoogleString ToString() const {
    return StrCat("StartElement ", element_->ToString());
  }
  virtual HtmlElement* GetElementIfStartEvent() { return element_; }
  virtual HtmlElement* GetNode() { return element_; }
 private:
  HtmlElement* element_;

  DISALLOW_COPY_AND_ASSIGN(HtmlStartElementEvent);
};

class HtmlEndElementEvent: public HtmlEvent {
 public:
  HtmlEndElementEvent(HtmlElement* element, int line_number)
      : HtmlEvent(line_number),
        element_(element) {
  }
  virtual void Run(HtmlFilter* filter) { filter->EndElement(element_); }
  virtual GoogleString ToString() const {
    return StrCat("EndElement ", element_->ToString());
  }
  virtual HtmlElement* GetElementIfEndEvent() { return element_; }
  virtual HtmlElement* GetNode() { return element_; }
 private:
  HtmlElement* element_;

  DISALLOW_COPY_AND_ASSIGN(HtmlEndElementEvent);
};

class HtmlLeafNodeEvent: public HtmlEvent {
 public:
  explicit HtmlLeafNodeEvent(int line_number) : HtmlEvent(line_number) { }
  virtual HtmlNode* GetNode() { return GetLeafNode(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(HtmlLeafNodeEvent);
};

class HtmlIEDirectiveEvent: public HtmlLeafNodeEvent {
 public:
  HtmlIEDirectiveEvent(HtmlIEDirectiveNode* directive, int line_number)
      : HtmlLeafNodeEvent(line_number),
        directive_(directive) {
  }
  virtual void Run(HtmlFilter* filter) { filter->IEDirective(directive_); }
  virtual GoogleString ToString() const {
    return StrCat("IEDirective ", directive_->contents());
  }
  virtual HtmlLeafNode* GetLeafNode() { return directive_; }
 private:
  HtmlIEDirectiveNode* directive_;

  DISALLOW_COPY_AND_ASSIGN(HtmlIEDirectiveEvent);
};

class HtmlCdataEvent: public HtmlLeafNodeEvent {
 public:
  HtmlCdataEvent(HtmlCdataNode* cdata, int line_number)
      : HtmlLeafNodeEvent(line_number),
        cdata_(cdata) {
  }
  virtual void Run(HtmlFilter* filter) { filter->Cdata(cdata_); }
  virtual GoogleString ToString() const {
    return StrCat("Cdata ", cdata_->contents());
  }
  virtual HtmlLeafNode* GetLeafNode() { return cdata_; }
 private:
  HtmlCdataNode* cdata_;

  DISALLOW_COPY_AND_ASSIGN(HtmlCdataEvent);
};

class HtmlCommentEvent: public HtmlLeafNodeEvent {
 public:
  HtmlCommentEvent(HtmlCommentNode* comment, int line_number)
      : HtmlLeafNodeEvent(line_number),
        comment_(comment) {
  }
  virtual void Run(HtmlFilter* filter) { filter->Comment(comment_); }
  virtual GoogleString ToString() const {
    return StrCat("Comment ", comment_->contents());
  }
  virtual HtmlLeafNode* GetLeafNode() { return comment_; }

 private:
  HtmlCommentNode* comment_;

  DISALLOW_COPY_AND_ASSIGN(HtmlCommentEvent);
};

class HtmlCharactersEvent: public HtmlLeafNodeEvent {
 public:
  HtmlCharactersEvent(HtmlCharactersNode* characters, int line_number)
      : HtmlLeafNodeEvent(line_number),
        characters_(characters) {
  }
  virtual void Run(HtmlFilter* filter) { filter->Characters(characters_); }
  virtual GoogleString ToString() const {
    return StrCat("Characters ", characters_->contents());
  }
  virtual HtmlLeafNode* GetLeafNode() { return characters_; }
  virtual HtmlCharactersNode* GetCharactersNode() { return characters_; }
 private:
  HtmlCharactersNode* characters_;

  DISALLOW_COPY_AND_ASSIGN(HtmlCharactersEvent);
};

class HtmlDirectiveEvent: public HtmlLeafNodeEvent {
 public:
  HtmlDirectiveEvent(HtmlDirectiveNode* directive, int line_number)
      : HtmlLeafNodeEvent(line_number),
        directive_(directive) {
  }
  virtual void Run(HtmlFilter* filter) { filter->Directive(directive_); }
  virtual GoogleString ToString() const {
    return StrCat("Directive: ", directive_->contents());
  }
  virtual HtmlLeafNode* GetLeafNode() { return directive_; }
 private:
  HtmlDirectiveNode* directive_;

  DISALLOW_COPY_AND_ASSIGN(HtmlDirectiveEvent);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTML_HTML_EVENT_H_
