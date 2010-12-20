// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: mdsteele@google.com (Matthew D. Steele)

#include "net/instaweb/rewriter/public/js_inline_filter.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/google_url.h"

namespace net_instaweb {

JsInlineFilter::JsInlineFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      html_parse_(driver->html_parse()),
      script_atom_(html_parse_->Intern("script")),
      src_atom_(html_parse_->Intern("src")),
      size_threshold_bytes_(driver->options()->js_inline_max_bytes()),
      script_tag_scanner_(html_parse_),
      should_inline_(false) {}

JsInlineFilter::~JsInlineFilter() {}

void JsInlineFilter::StartDocumentImpl() {
  // TODO(sligocki): This should go in the domain lawyer, right?
  domain_ = html_parse_->gurl().host();
  should_inline_ = false;
}

void JsInlineFilter::EndDocument() {
  domain_.clear();
}

void JsInlineFilter::StartElementImpl(HtmlElement* element) {
  DCHECK(!should_inline_);

  HtmlElement::Attribute* src;
  if (script_tag_scanner_.ParseScriptElement(element, &src) ==
      ScriptTagScanner::kJavaScript) {
    should_inline_ = (src != NULL) && (src->value() != NULL);
  }
}

void JsInlineFilter::EndElementImpl(HtmlElement* element) {
  if (should_inline_ && html_parse_->IsRewritable(element)) {
    DCHECK(element->tag() == script_atom_);
    const char* src = element->AttributeValue(src_atom_);
    DCHECK(src != NULL);
    should_inline_ = false;

    // TODO(morlovich): Consider async/defer here; it may not be a good
    // idea to inline async scripts in particular

    scoped_ptr<Resource> resource(CreateInputResourceAndReadIfCached(src));
    // TODO(jmaessen): Is the domain lawyer policy the appropriate one here?
    // Or do we still have to check for strict domain equivalence?
    // If so, add an inline-in-page policy to domainlawyer in some form,
    // as we make a similar policy decision in css_inline_filter.
    if (resource != NULL && resource->ContentsValid()) {
      StringPiece contents = resource->contents();
      // Only inline if it's small enough, and if it doesn't contain
      // "</script>" anywhere.  If we inline an external script containing
      // "</script>", the <script> tag will be ended early.
      // See http://code.google.com/p/modpagespeed/issues/detail?id=106
      // TODO(mdsteele): We should consider rewriting "</script>" to
      //   "<\/script>" instead of just bailing.  But we can't blindly search
      //   and replace because that would break legal (if contrived) code such
      //   as "if(x</script>/){...}", which is comparing x to a regex literal.
      if (contents.size() <= size_threshold_bytes_ &&
          contents.find("</script>") == StringPiece::npos) {
        // If we're in XHTML, we should wrap the script in a <!CDATA[...]]>
        // block to ensure that we don't break well-formedness.  Since XHTML is
        // sometimes interpreted as HTML (which will ignore CDATA delimiters),
        // we have to hide the CDATA delimiters behind Javascript comments.
        // See http://lachy.id.au/log/2006/11/xhtml-script
        // and http://code.google.com/p/modpagespeed/issues/detail?id=125
        if (html_parse_->doctype().IsXhtml()) {
          // CDATA sections cannot be nested because they end with the first
          // occurance of "]]>", so if the script contains that string
          // anywhere (and we're in XHTML) we can't inline.
          // TODO(mdsteele): Again, we should consider escaping somehow.
          if (contents.find("]]>") == StringPiece::npos) {
            HtmlCharactersNode* node =
                html_parse_->NewCharactersNode(element, "//<![CDATA[\n");
            node->Append(contents);
            node->Append("\n//]]>");
            html_parse_->InsertElementBeforeCurrent(node);
            element->DeleteAttribute(src_atom_);
          }
        }
        // If we're not in XHTML, we can simply paste in the external script
        // verbatim.
        else {
          html_parse_->InsertElementBeforeCurrent(
              html_parse_->NewCharactersNode(element, contents));
          element->DeleteAttribute(src_atom_);
        }
      }
    }
  }
}

void JsInlineFilter::Characters(HtmlCharactersNode* characters) {
  if (should_inline_) {
    DCHECK(characters->parent() != NULL);
    DCHECK(characters->parent()->tag() == script_atom_);
    if (OnlyWhitespace(characters->contents())) {
      // If it's just whitespace inside the script tag, it's (probably) safe to
      // just remove it.
      html_parse_->DeleteElement(characters);
    } else {
      // This script tag isn't empty, despite having a src field.  The contents
      // won't be executed by the browser, but will still be in the DOM; some
      // external scripts like to use this as a place to store data.  So, we'd
      // better not try to inline in this case.
      should_inline_ = false;
    }
  }
}

}  // namespace net_instaweb
