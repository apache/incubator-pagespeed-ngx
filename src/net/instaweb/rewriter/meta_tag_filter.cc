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

// Author: nforman@google.com (Naomi Forman)
//
// Implements the convert_meta_tags filter, which creates a
// response header for http-equiv meta tags.

#include "net/instaweb/rewriter/public/meta_tag_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kConvertedMetaTags[] = "converted_meta_tags";

}  // namespace

namespace net_instaweb {

MetaTagFilter::MetaTagFilter(RewriteDriver* rewrite_driver)
    : CommonFilter(rewrite_driver) {
  Statistics* stats = driver_->statistics();
  converted_meta_tag_count_ = stats->GetVariable(kConvertedMetaTags);
}

void MetaTagFilter::Initialize(Statistics* stats) {
  if (stats != NULL) {
    stats->AddVariable(kConvertedMetaTags);
  }
}

MetaTagFilter::~MetaTagFilter() {}

void MetaTagFilter::EndElementImpl(HtmlElement* element) {
  // If headers are null, they got reset due to a flush, so don't
  // try to convert any tags into headers (which were already finalized).
  ResponseHeaders* headers = driver_->response_headers_ptr();
  if (headers != NULL) {
    // Figure out if this is a meta tag.  If it is, move to headers.
    if (element->keyword() == HtmlName::kMeta) {
      // We want only meta tags with http header equivalents.
      HtmlElement::Attribute* equiv = element->FindAttribute(
          HtmlName::kHttpEquiv);
      if (equiv != NULL) {
        HtmlElement::Attribute* value = element->FindAttribute(
            HtmlName::kContent);
        // Check to see if we have this value already.  If we do,
        // there's no need to add it in again.
        ConstStringStarVector values;
        headers->Lookup(equiv->value(), &values);
        for (int i = 0, n = values.size(); i < n; ++i) {
          StringPiece val(*values[i]);
          if (StringCaseCompare(val, value->value()) == 0) {
            return;
          }
        }
        headers->Add(equiv->value(), value->value());
        if (converted_meta_tag_count_ != NULL) {
          converted_meta_tag_count_->Add(1);
        }
      }
    }
  }
}

void MetaTagFilter::Flush() {
  driver_->set_response_headers_ptr(NULL);
}

}  // namespace net_instaweb
