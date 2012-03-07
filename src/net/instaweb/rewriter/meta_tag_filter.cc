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
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

// Statistics variable for number of tags converted to headers.
const char kConvertedMetaTags[] = "converted_meta_tags";

}  // namespace

namespace net_instaweb {

MetaTagFilter::MetaTagFilter(RewriteDriver* rewrite_driver)
    : CommonFilter(rewrite_driver),
      response_headers_(NULL) {
  Statistics* stats = driver_->statistics();
  converted_meta_tag_count_ = stats->GetVariable(kConvertedMetaTags);
}

void MetaTagFilter::Initialize(Statistics* stats) {
  stats->AddVariable(kConvertedMetaTags);
}

MetaTagFilter::~MetaTagFilter() {}

void MetaTagFilter::StartDocumentImpl() {
  // This pointer will be nulled at first Flush to guarantee that we don't
  // write it after that (won't work).
  response_headers_ = driver_->response_headers_ptr();
}


void MetaTagFilter::EndElementImpl(HtmlElement* element) {
  // If response_headers_ are null, they got reset due to a flush, so don't
  // try to convert any tags into response_headers_ (which were already
  // finalized). Also don't add meta tags to response_headers_ if they're
  // inside a noscript tag.
  if (response_headers_ == NULL || noscript_element() != NULL ||
      element->keyword() != HtmlName::kMeta) {
    return;
  }

  // We want meta tags with http header equivalents.
  HtmlElement::Attribute* equiv = element->FindAttribute(
      HtmlName::kHttpEquiv);
  HtmlElement::Attribute* value = element->FindAttribute(
      HtmlName::kContent);

  // HTTP-EQUIV case.
  if (equiv != NULL && value != NULL) {
    StringPiece attribute = equiv->value();
    StringPiece content = value->value();
    // It doesn't make sense to have nothing in HttpEquiv, but that means
    // it is in fact lurking out there.
    TrimWhitespace(&attribute);

    if (!StringCaseEqual(attribute, HttpAttributes::kContentType) ||
        content.empty()) {
      return;
    }
    // Check to see if we have this value already.  If we do,
    // there's no need to add it in again.
    if (response_headers_->HasValue(attribute, content)) {
      return;
    }

    GoogleString mime_type, unused_charset;
    if (ParseContentType(content, &mime_type, &unused_charset) &&
        !mime_type.empty()) {
      const ContentType* type = MimeTypeToContentType(mime_type);
      if (type == NULL || !type->IsHtmlLike()) {
        return;
      }
      if (response_headers_->MergeContentType(content)) {
        converted_meta_tag_count_->Add(1);
        return;
      }
    }
    return;
  } else {
    // Also handle the <meta charset=''> case.
    HtmlElement::Attribute* charset = element->FindAttribute(
        HtmlName::kCharset);
    if (charset != NULL) {
      GoogleString type = StrCat("; charset=", charset->value());
      if (response_headers_->MergeContentType(type)) {
        converted_meta_tag_count_->Add(1);
      }
    }
  }
}

void MetaTagFilter::Flush() {
  response_headers_ = NULL;
}

}  // namespace net_instaweb
