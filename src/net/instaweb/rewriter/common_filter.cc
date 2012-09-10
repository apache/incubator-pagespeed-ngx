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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/common_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

CommonFilter::CommonFilter(RewriteDriver* driver)
    : driver_(driver),
      server_context_(driver->server_context()),
      rewrite_options_(driver->options()),
      seen_base_(false) {
}

CommonFilter::~CommonFilter() {}

void CommonFilter::StartDocument() {
  // Base URL starts as document URL.
  noscript_element_ = NULL;
  // Reset whether or not we've seen the base tag yet, because we're starting
  // back at the top of the document.
  seen_base_ = false;
  // Run the actual filter's StartDocumentImpl.
  StartDocumentImpl();
}

void CommonFilter::StartElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kNoscript) {
    if (noscript_element_ == NULL) {
      noscript_element_ = element;  // Record top-level <noscript>
    }
  }
  // If this is a base tag with an href attribute, then we've seen the base, and
  // any url references after this point are relative to that base.
  if (element->keyword() == HtmlName::kBase &&
      element->FindAttribute(HtmlName::kHref) != NULL) {
    seen_base_ = true;
  }

  // Run actual filter's StartElementImpl
  StartElementImpl(element);
}

void CommonFilter::EndElement(HtmlElement* element) {
  if (element == noscript_element_) {
    noscript_element_ = NULL;  // We are exitting the top-level <noscript>
  }

  // Run actual filter's EndElementImpl
  EndElementImpl(element);
}

// Returns whether or not we can resolve against the base tag.  References
// that occur before the base tag can not be resolved against it.
// Different browsers deal with such refs differently, but we shouldn't
// change their behavior.
bool CommonFilter::BaseUrlIsValid() const {
  // If there are no href or src attributes before the base, it's
  // always valid.
  if (!driver_->refs_before_base()) {
    return true;
  }
  // If the filter has already seen the base url, then it's now valid
  // even if there were urls before it.
  return seen_base_;
}

ResourcePtr CommonFilter::CreateInputResource(const StringPiece& input_url) {
  ResourcePtr resource;
  if (!input_url.empty()) {
    if (!BaseUrlIsValid()) {
      const GoogleUrl resource_url(input_url);
      if (resource_url.is_valid() && resource_url.is_standard()) {
        resource = driver_->CreateInputResource(resource_url);
      }
    } else if (base_url().is_valid()) {
      const GoogleUrl resource_url(base_url(), input_url);
      if (resource_url.is_valid() && resource_url.is_standard()) {
        resource = driver_->CreateInputResource(resource_url);
      }
    }
  }
  return resource;
}

const GoogleUrl& CommonFilter::base_url() const {
  return driver_->base_url();
}

const GoogleUrl& CommonFilter::decoded_base_url() const {
  return driver_->decoded_base_url();
}

bool CommonFilter::ExtractMetaTagDetails(const HtmlElement& element,
                                         const ResponseHeaders* headers,
                                         GoogleString* content,
                                         GoogleString* mime_type,
                                         GoogleString* charset) {
  // The charset can be specified in an http-equiv or a charset attribute.
  const HtmlElement::Attribute* equiv;
  const HtmlElement::Attribute* value;
  const HtmlElement::Attribute* cs_attr;

  bool result = false;

  // HTTP-EQUIV case.
  if ((equiv = element.FindAttribute(HtmlName::kHttpEquiv)) != NULL &&
      (value = element.FindAttribute(HtmlName::kContent)) != NULL) {
    StringPiece attribute = equiv->DecodedValueOrNull();
    StringPiece value_str = value->DecodedValueOrNull();
    if (!value_str.empty() && !attribute.empty()) {
      value_str.CopyToString(content);
      TrimWhitespace(&attribute);

      // http-equiv must equal "Content-Type" and content mustn't be blank.
      if (StringCaseEqual(attribute, HttpAttributes::kContentType) &&
          !content->empty()) {
        // Per http://webdesign.about.com/od/metatags/qt/meta-charset.htm we
        // need to handle this:
        //   <meta http-equiv=Content-Type content=text/html; charset=UTF-8>
        // The approach here is to first parse the content string, then if it
        // doesn't have charset, look for a charset attribute and if the
        // content ends with ';' append the 'content=charset' text. Note that
        // we have to parse first because we need the -final- content for
        // checking the headers. If the initial parsing fails then there's no
        // point in proceeding because even if we add the content= then it
        // won't parse and we'll return false.
        bool have_parsed = true;  // Controls the second parse below.
        GoogleString local_charset;
        result = ParseContentType(*content, mime_type, &local_charset);
        if (result) {
          // No charset, see if we have a charset attribute to append.
          if (local_charset.empty() && *(content->rbegin()) == ';' &&
              ((cs_attr = element.FindAttribute(HtmlName::kCharset)) != NULL) &&
              (cs_attr->DecodedValueOrNull() != NULL)) {
            StrAppend(content, " charset=", cs_attr->DecodedValueOrNull());
            have_parsed = false;
          }
          // If requested, check to see if we have this value already.
          if (headers != NULL && headers->HasValue(attribute, *content)) {
            result = false;
          } else if (!have_parsed) {
            result = ParseContentType(*content, mime_type, &local_charset);
          }
          if (result) {
            *charset = local_charset;
          }
        }
      }
    }
  // charset case.
  } else if (((cs_attr = element.FindAttribute(HtmlName::kCharset)) != NULL) &&
             (cs_attr->DecodedValueOrNull() != NULL)) {
    *mime_type = "";
    *charset = cs_attr->DecodedValueOrNull();
    result = true;
  }

  return result;
}

void CommonFilter::LogFilterModifiedContent() {
  if (driver()->log_record() != NULL) {
    driver()->log_record()->LogAppliedRewriter(Name());
  }
}

}  // namespace net_instaweb
