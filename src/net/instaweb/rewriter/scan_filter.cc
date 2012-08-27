/*
 * Copyright 2011 Google Inc.
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

#include "net/instaweb/rewriter/public/scan_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/util/public/charset_util.h"
#include "net/instaweb/util/public/statistics.h"

namespace net_instaweb {

ScanFilter::ScanFilter(RewriteDriver* driver)
    : driver_(driver) {
}

ScanFilter::~ScanFilter() {
}

void ScanFilter::StartDocument() {
  // TODO(jmarantz): consider having rewrite_driver access the url in this
  // class, rather than poking it into rewrite_driver.
  seen_any_nodes_ = false;
  seen_refs_ = false;
  seen_base_ = false;
  seen_meta_tag_charset_ = false;

  // Set the driver's containing charset to whatever the headers set it to; if
  // they don't set it to anything, blank the driver's so we know it's not set.
  const ResponseHeaders* headers = driver_->response_headers();
  driver_->set_containing_charset(headers == NULL ? "" :
                                  headers->DetermineCharset());
}

void ScanFilter::Cdata(HtmlCdataNode* cdata) {
  seen_any_nodes_ = true;
}

void ScanFilter::Comment(HtmlCommentNode* comment) {
  seen_any_nodes_ = true;
}

void ScanFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  seen_any_nodes_ = true;
}

void ScanFilter::Directive(HtmlDirectiveNode* directive) {
  seen_any_nodes_ = true;
}

void ScanFilter::Characters(HtmlCharactersNode* characters) {
  // Check for a BOM at the start of the document. All other event handlers
  // set the flag to false without using it, so if it's true on entry then
  // this must be the first event.
  if (!seen_any_nodes_ && driver_->containing_charset().empty()) {
    StringPiece charset = GetCharsetForBom(characters->contents());
    if (!charset.empty()) {
      driver_->set_containing_charset(charset);
    }
  }
  seen_any_nodes_ = true;  // ignore any subsequent BOMs.
}

void ScanFilter::StartElement(HtmlElement* element) {
  seen_any_nodes_ = true;
  // <base>
  if (element->keyword() == HtmlName::kBase) {
    HtmlElement::Attribute* href = element->FindAttribute(HtmlName::kHref);
    // See http://www.whatwg.org/specs/web-apps/current-work/multipage
    // /semantics.html#the-base-element
    //
    // TODO(jmarantz): If the base is present but cannot be decoded, we should
    // probably not do any resource rewriting at all.
    if ((href != NULL) && (href->DecodedValueOrNull() != NULL)) {
      // TODO(jmarantz): consider having rewrite_driver access the url in this
      // class, rather than poking it into rewrite_driver.
      driver_->SetBaseUrlIfUnset(href->DecodedValueOrNull());
      seen_base_ = true;
      if (seen_refs_) {
        driver_->set_refs_before_base();
      }
    }
    // TODO(jmarantz): handle base targets in addition to hrefs.
  } else {
    semantic_type::Category category;
    HtmlElement::Attribute* href = resource_tag_scanner::ScanElement(
        element, driver_, &category);

    // Don't count <html manifest=...> as a ref for the purpose of determining
    // if there are refs before base.  It's also important not to count <head
    // profile=...> but ScanElement skips that.
    if (!seen_refs_ && !seen_base_ && href != NULL &&
        !(element->keyword() == HtmlName::kHtml &&
          href->keyword() == HtmlName::kManifest)) {
      seen_refs_ = true;
    }
  }

  // Get/set the charset of the containing HTML page.
  // HTTP1.1 says the default charset is ISO-8859-1 but as the W3C says (in
  // http://www.w3.org/International/O-HTTP-charset.en.php) not many browsers
  // actually do this so we default to "" instead so that we can tell if it
  // has been set. The following logic is taken from
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#
  // determining-the-character-encoding:
  // 1. If the UA specifies an encoding, use that (not relevant to us).
  // 2. If the transport layer specifies an encoding, use that.
  //    Implemented by using the charset from any Content-Type header.
  // 3. If there is a BOM at the start of the file, use the relevant encoding.
  // 4. If there is a meta tag in the HTML, use the encoding specified if any.
  // 5. There are various other heuristics listed which are not implemented.
  // 6. Otherwise, use no charset or default to something "sensible".
  if (!seen_meta_tag_charset_ &&
      driver_->containing_charset().empty() &&
      element->keyword() == HtmlName::kMeta) {
    GoogleString content, mime_type, charset;
    if (CommonFilter::ExtractMetaTagDetails(*element, NULL,
                                            &content, &mime_type, &charset)) {
      if (!charset.empty()) {
        driver_->set_containing_charset(charset);
        seen_meta_tag_charset_ = true;
      }
    }
  }
}

void ScanFilter::Flush() {
  driver_->resource_manager()->rewrite_stats()->num_flushes()->Add(1);
}

}  // namespace net_instaweb
