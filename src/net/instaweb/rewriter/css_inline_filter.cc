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

#include "net/instaweb/rewriter/public/css_inline_filter.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

CssInlineFilter::CssInlineFilter(HtmlParse* html_parse,
                                 ResourceManager* resource_manager,
                                 size_t size_threshold_bytes)
    : CommonFilter(html_parse),
      html_parse_(html_parse),
      resource_manager_(resource_manager),
      href_atom_(html_parse_->Intern("href")),
      link_atom_(html_parse_->Intern("link")),
      media_atom_(html_parse_->Intern("media")),
      rel_atom_(html_parse_->Intern("rel")),
      style_atom_(html_parse_->Intern("style")),
      size_threshold_bytes_(size_threshold_bytes) {}

void CssInlineFilter::StartDocumentImpl() {
  // TODO(sligocki): Domain lawyerify.
  domain_ = html_parse_->gurl().host();
}

void CssInlineFilter::EndDocument() {
  domain_.clear();
}

void CssInlineFilter::EndElementImpl(HtmlElement* element) {
  if (element->tag() == link_atom_) {
    const char* rel = element->AttributeValue(rel_atom_);
    if (rel == NULL || strcmp(rel, "stylesheet")) {
      return;
    }

    // If the link tag has a media attribute whose value isn't "all", don't
    // inline.  (Note that "all" is equivalent to having no media attribute;
    // see http://www.w3.org/TR/html5/semantics.html#the-style-element)
    const char* media = element->AttributeValue(media_atom_);
    if (media != NULL && strcmp(media, "all") != 0) {
      return;
    }

    // Get the URL where the external script is stored
    const char* href = element->AttributeValue(href_atom_);
    if (href == NULL) {
      return;  // We obviously can't inline if the URL isn't there.
    }

    // Make sure we're not moving across domains -- CSS can potentially contain
    // Javascript expressions.
    // TODO(jmaessen): Is the domain lawyer policy the appropriate one here?
    // Or do we still have to check for strict domain equivalence?
    // If so, add an inline-in-page policy to domainlawyer in some form,
    // as we make a similar policy decision in js_inline_filter.
    MessageHandler* message_handler = html_parse_->message_handler();
    scoped_ptr<Resource> resource(
        resource_manager_->CreateInputResourceAndReadIfCached(
            base_gurl(), href, message_handler));
    if (resource == NULL  || !resource->ContentsValid()) {
      return;
    }

    // Check that the file is small enough to inline.
    StringPiece contents = resource->contents();
    if (contents.size() > size_threshold_bytes_) {
      return;
    }

    // Absolutify the URLs in the CSS -- relative URLs will break otherwise.
    std::string rewritten;
    StringWriter writer(&rewritten);
    if (!CssTagScanner::AbsolutifyUrls(contents, resource->url(), &writer,
                                       message_handler)) {
      return;
    }

    // Inline the CSS.
    HtmlElement* style_element =
        html_parse_->NewElement(element->parent(), style_atom_);
    if (html_parse_->ReplaceNode(element, style_element)) {
      html_parse_->AppendChild(
          style_element, html_parse_->NewCharactersNode(element, rewritten));
    }
  }
}

}  // namespace net_instaweb
