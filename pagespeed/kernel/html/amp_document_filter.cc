/*
 * Copyright 2016 Google Inc.
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

#include "pagespeed/kernel/html/amp_document_filter.h"

#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_parse.h"

namespace net_instaweb {

const char AmpDocumentFilter::kUtf8LightningBolt[] = "\xe2\x9a\xa1";
const char AmpDocumentFilter::kInvalidAmpDirectiveComment[] =
    "Invalid AMP directive -- will be ignored and will probably "
    "be rejected by the AMP validator.";

AmpDocumentFilter::AmpDocumentFilter(HtmlParse* html_parse,
                                     BoolCallback* discovered)
    : html_parse_(html_parse),
      is_known_(false),
      saw_doctype_(false),
      discovered_(discovered) {
}

AmpDocumentFilter::~AmpDocumentFilter() {}

void AmpDocumentFilter::StartDocument() {
  is_known_ = false;
  saw_doctype_ = false;
}

void AmpDocumentFilter::EndDocument() {
  if (!is_known_) {
    discovered_->Run(false);
    is_known_ = true;
  }
}

void AmpDocumentFilter::StartElement(HtmlElement* element) {
  // TODO(jmarantz): See https://github.com/ampproject/amphtml/issues/2380
  // where in response to Cloudflare's concerns about arbitrary buffering,
  // we are discussing a requirement that the amp tag be within the first
  // N bytes.  If that gets resolved, we should enforce that limit in this
  // filter.

  bool declare_not_amp = !is_known_;
  if (element->keyword() == HtmlName::kHtml) {
    // Detect if this document is self-declaring as AMP.
    HtmlElement::Attribute* amp = element->FindAttribute(HtmlName::kAmp);
    if (amp == nullptr) {
      amp = element->FindAttribute(kUtf8LightningBolt);
    }
    if (amp != nullptr) {
      // TODO(jmarantz): should we care about what the value is?
      declare_not_amp = false;
      if (!is_known_) {
        discovered_->Run(true);
        is_known_ = true;
      } else {
        // Some other element after the 'doctype' was seen prior to <html amp>,
        // so we ignore the <html amp> directive and warn the user that his
        // doc is borked.
        html_parse_->InsertComment(kInvalidAmpDirectiveComment);
      }
    }
  } else if (!saw_doctype_ &&
             StringCaseEqual(element->name_str(), "!doctype")) {
    saw_doctype_ = true;
    declare_not_amp = false;
  }
  if (declare_not_amp) {
    discovered_->Run(false);
    is_known_ = true;
  }
}

void AmpDocumentFilter::Characters(HtmlCharactersNode* characters) {
  if (!is_known_) {
    StringPiece contents = characters->contents();
    TrimWhitespace(&contents);
    if (!contents.empty()) {
      discovered_->Run(false);
      is_known_ = true;
    }
  }
}

}  // namespace net_instaweb
