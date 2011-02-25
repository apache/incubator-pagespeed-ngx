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

// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/url_left_trim_filter.h"

#include <vector>
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

// names for Statistics variables.
const char kUrlTrims[] = "url_trims";
const char kUrlTrimSavedBytes[] = "url_trim_saved_bytes";

} // namespace

namespace net_instaweb {

UrlLeftTrimFilter::UrlLeftTrimFilter(HtmlParse* html_parse,
                                     Statistics* stats)
    : html_parse_(html_parse),
      trim_count_((stats == NULL) ? NULL : stats->GetVariable(kUrlTrims)),
      trim_saved_bytes_(
          (stats == NULL) ? NULL : stats->GetVariable(kUrlTrimSavedBytes)) {
}

void UrlLeftTrimFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kUrlTrims);
  statistics->AddVariable(kUrlTrimSavedBytes);
}

void UrlLeftTrimFilter::StartDocument() {
  base_url_ = html_parse_->gurl();
}

// If the element is a base tag, set the base url to be the href value.
// Do not rewrite the base tag.
void UrlLeftTrimFilter::StartElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kBase) {
    HtmlElement::Attribute *base_href = element->FindAttribute(HtmlName::kHref);
    if (base_href != NULL) {
      SetBaseUrl(base_href->value());
    }
  } else {
    TrimAttribute(element->FindAttribute(HtmlName::kHref));
    TrimAttribute(element->FindAttribute(HtmlName::kSrc));
  }
}

void UrlLeftTrimFilter::SetBaseUrl(const StringPiece& base) {
  if(base_url_.is_empty()) {
    base_url_ = GoogleUrl::Create(base);
  } else {
    base_url_ = GoogleUrl::Resolve(html_parse_->gurl(), base);
  }
}

// Resolve the url we want to trim, and then remove the scheme, origin
// and/or path as appropriate.
bool UrlLeftTrimFilter::Trim(const GURL& base_url,
                             const StringPiece& url_to_trim,
                             std::string* trimmed_url,
                             MessageHandler* handler) {
  if (!base_url.is_valid() || !base_url.IsStandard() || url_to_trim.empty()) {
    return false;
  }

  GURL long_url = GoogleUrl::Resolve(base_url, url_to_trim);
  //  Don't try to rework an invalid url
  if (!long_url.is_valid() || !long_url.IsStandard()) {
    return false;
  }

  std::string long_url_buffer(GoogleUrl::Spec(long_url));
  size_t to_trim = 0;

  // If we can strip the whole origin (http://www.google.com/) do it,
  // then see if we can strip the prefix of the path.
  std::string origin = GoogleUrl::Origin(base_url);
  if (origin.length() < long_url_buffer.length() &&
      GoogleUrl::Origin(long_url) == origin) {
    to_trim = origin.length();
    std::string path = GoogleUrl::PathSansLeaf(base_url);
    if (to_trim + path.length() < long_url_buffer.length() &&
        GoogleUrl::PathSansLeaf(long_url).find(path) == 0) {
      to_trim += path.length();
    }
  }

  // If we can't strip the whole origin, see if we can strip off the scheme.
  std::string scheme = base_url.scheme();
  if (to_trim == 0 && scheme.length() + 1 < long_url_buffer.length() &&
      long_url.SchemeIs(scheme.c_str())) {
    // +1 for : (not included in scheme)
    to_trim = scheme.length() + 1;
  }

  // Candidate trimmed URL.
  StringPiece trimmed_url_piece(long_url_buffer);
  trimmed_url_piece.remove_prefix(to_trim);

  if (trimmed_url_piece.length() < url_to_trim.length()) {
    // If we have a colon before the first slash there are two options:
    // option 1 - we still have our scheme, in which case we're not shortening
    // anything, and can just abort.
    // option 2 - the original url had some nasty scheme-looking stuff in the
    // middle of the url, and now it's at the front.  This causes Badness,
    // revert to the original.
    size_t colon_pos = trimmed_url_piece.find(':');
    if (colon_pos != trimmed_url_piece.npos) {
      if (trimmed_url_piece.rfind('/', colon_pos) == trimmed_url_piece.npos) {
        return false;
      }
    }
    GURL resolved_newurl(GoogleUrl::Resolve(base_url, trimmed_url_piece));
    DCHECK(resolved_newurl == long_url);
    if (resolved_newurl != long_url) {
      handler->Message(kError, "Left trimming of %s referring to %s was %s, "
                       "which instead refers to %s.",
                       url_to_trim.as_string().c_str(),
                       long_url_buffer.c_str(),
                       trimmed_url_piece.as_string().c_str(),
                       GoogleUrl::Spec(resolved_newurl).c_str());
      return false;
    }
    *trimmed_url = trimmed_url_piece.as_string();
    return true;
  }
  return false;
}

// Trim the value of the given attribute, if the attribute is non-NULL.
void UrlLeftTrimFilter::TrimAttribute(HtmlElement::Attribute* attr) {
  if (attr != NULL) {
    StringPiece val(attr->value());
    std::string trimmed_val;
    size_t orig_size = val.size();
    if (Trim(base_url_, val, &trimmed_val, html_parse_->message_handler())) {
      size_t saved = orig_size - trimmed_val.size();
      const char* q = attr->quote();
      html_parse_->InfoHere(
          "trimmed %u %s=%s%s%s to %s%s%s.", static_cast<unsigned>(saved),
          attr->name_str(), q, attr->value(), q,
          q, trimmed_val.c_str(), q);
      attr->SetValue(trimmed_val);
      if (trim_count_ != NULL) {
        trim_count_->Add(1);
        trim_saved_bytes_->Add(orig_size - trimmed_val.size());
      }
    }
  }
}

}  // namespace net_instaweb
