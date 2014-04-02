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
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/critical_images_beacon_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char CommonFilter::kCreateResourceFailedDebugMsg[] =
    "Cannot create resource: either its domain is unauthorized and "
    "InlineUnauthorizedResources is not enabled, or it cannot be fetched "
    "(check the server logs)";

CommonFilter::CommonFilter(RewriteDriver* driver)
    : driver_(driver),
      server_context_(driver->server_context()),
      rewrite_options_(driver->options()),
      noscript_element_(NULL),
      end_body_point_(NULL),
      seen_base_(false) {
}

CommonFilter::~CommonFilter() {}

void CommonFilter::InsertNodeAtBodyEnd(HtmlNode* data) {
  if (end_body_point_ != NULL && driver_->CanAppendChild(end_body_point_)) {
    driver_->AppendChild(end_body_point_, data);
  } else {
    driver_->InsertNodeBeforeCurrent(data);
  }
}

void CommonFilter::StartDocument() {
  // Base URL starts as document URL.
  noscript_element_ = NULL;
  end_body_point_ = NULL;
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

  // If end_body_point_ was set (if we've already seen a </body> for instance),
  // and we encounter a new open element, clear end_body_point_ as it's no
  // longer the end of the body.
  end_body_point_ = NULL;

  // Run actual filter's StartElementImpl.
  StartElementImpl(element);
}

void CommonFilter::EndElement(HtmlElement* element) {
  switch (element->keyword()) {
    case HtmlName::kNoscript:
      if (element == noscript_element_) {
        noscript_element_ = NULL;  // We are exiting the top-level <noscript>
      }
      end_body_point_ = NULL;
      break;
    case HtmlName::kBody:
      // Preferred injection location
      end_body_point_ = element;
      break;
    case HtmlName::kHtml:
      if ((end_body_point_ == NULL ||
           !driver()->CanAppendChild(end_body_point_)) &&
          driver()->CanAppendChild(element)) {
        // Try to inject before </html> if before </body> won't work.
        end_body_point_ = element;
      }
      break;
    default:
      // There were (possibly implicit) close tags after </body> or </html>, so
      // throw that point away.
      end_body_point_ = NULL;
      break;
  }

  // Run actual filter's EndElementImpl.
  EndElementImpl(element);
}

void CommonFilter::Characters(net_instaweb::HtmlCharactersNode* characters) {
  // If we have a character node after the closing body or html tag, then we
  // can't safely insert something depending on being at the end of the document
  // there. This can happen due to a faulty filter, or malformed HTML.
  if (end_body_point_ != NULL && !OnlyWhitespace(characters->contents())) {
    end_body_point_ = NULL;
  }
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

void CommonFilter::ResolveUrl(StringPiece input_url, GoogleUrl* out_url) {
  out_url->Clear();
  if (!input_url.empty()) {
    if (!BaseUrlIsValid()) {
      out_url->Reset(input_url);
    } else if (base_url().IsWebValid()) {
      out_url->Reset(base_url(), input_url);
    }
  }
}

ResourcePtr CommonFilter::CreateInputResource(const StringPiece& input_url) {
  ResourcePtr resource;
  GoogleUrl resource_url;
  ResolveUrl(input_url, &resource_url);
  if (resource_url.IsWebValid()) {
    resource = driver_->CreateInputResource(
        resource_url,
        AllowUnauthorizedDomain(),
        IntendedForInlining() ?
            RewriteDriver::kIntendedForInlining :
            RewriteDriver::kIntendedForGeneral);
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

bool CommonFilter::CanAddPagespeedOnloadToImage(const HtmlElement& element) {
  const HtmlElement::Attribute* onload_attribute =
      element.FindAttribute(HtmlName::kOnload);
  return (noscript_element() == NULL &&
          (onload_attribute == NULL ||
           (onload_attribute->DecodedValueOrNull() != NULL &&
            strcmp(onload_attribute->DecodedValueOrNull(),
                   CriticalImagesBeaconFilter::kImageOnloadCode) == 0)));
}

void CommonFilter::LogFilterModifiedContent() {
  driver()->log_record()->SetRewriterLoggingStatus(
      LoggingId(), RewriterApplication::APPLIED_OK);
}

}  // namespace net_instaweb
