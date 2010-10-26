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
    : html_parse_(html_parse),
      resource_manager_(resource_manager),
      href_atom_(html_parse_->Intern("href")),
      link_atom_(html_parse_->Intern("link")),
      media_atom_(html_parse_->Intern("media")),
      rel_atom_(html_parse_->Intern("rel")),
      style_atom_(html_parse_->Intern("style")),
      size_threshold_bytes_(size_threshold_bytes) {}

void CssInlineFilter::StartDocument() {
  domain_ = html_parse_->gurl().host();
}

void CssInlineFilter::EndDocument() {
  domain_.clear();
}

void CssInlineFilter::EndElement(HtmlElement* element) {
  if (element->tag() == link_atom_) {
    const char* rel = element->AttributeValue(rel_atom_);
    if (rel == NULL || strcmp(rel, "stylesheet")) {
      return;
    }

    // Check if the link tag has a media attribute.  If so, don't inline.
    const char* media = element->AttributeValue(media_atom_);
    if (media != NULL) {
      return;
    }

    // Get the URL where the external script is stored
    const char* href = element->AttributeValue(href_atom_);
    if (href == NULL) {
      return;  // We obviously can't inline if the URL isn't there.
    }

    // Make sure we're not moving across domains -- CSS can potentially contain
    // Javascript expressions.
    GURL url = html_parse_->gurl().Resolve(href);
    if (!url.is_valid() || !url.DomainIs(domain_.data(), domain_.size())) {
      return;
    }

    // Get the text of the CSS file.
    // TODO(mdsteele): I feel like the below code should be a single
    //   convenience method somewhere, but I'm not sure where.
    //   [Agreed; it belongs in ResourceManager and should refactor
    //    all the similar code.  This formula is all over the place.
    //    - jmaessen)]
    MessageHandler* message_handler = html_parse_->message_handler();
    scoped_ptr<Resource> resource(
        resource_manager_->CreateInputResourceGURL(url, message_handler));
    if (resource == NULL ||
        !resource_manager_->ReadIfCached(resource.get(), message_handler) ||
        !resource->ContentsValid()) {
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
    if (!CssTagScanner::AbsolutifyUrls(contents, url.spec(), &writer,
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
