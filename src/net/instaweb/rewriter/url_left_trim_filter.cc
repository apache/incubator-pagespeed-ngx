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
  SetBaseUrl(html_parse_->url());
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

void UrlLeftTrimFilter::ClearBaseUrl() {
  base_url_ = GURL();
  scheme_.clear();
  origin_.clear();
  path_.clear();
}

void UrlLeftTrimFilter::SetBaseUrl(const StringPiece& base) {
  ClearBaseUrl();
  base_url_ = GURL(base.data());

  //  Don't try to set a base url for an invalid path.
  if (!base_url_.is_valid() || !base_url_.IsStandard()) {
    return;
  }
  scheme_ = std::string(base_url_.scheme());

  origin_ = std::string(GoogleUrl::Origin(base_url_));

  path_ = GoogleUrl::PathSansLeaf(base_url_);
}

// Resolve the url we want to trim, and then remove the scheme, origin
// and/or path as appropriate.
// StringPiece supports left and right trimming in place (the only
// mutation it permits).
// Check lengths so that we never completely
// remove a url, leaving it empty.
bool UrlLeftTrimFilter::Trim(const StringPiece& url,
                             std::string *trimmed_url) {
  if (url.empty()) {
    return false;
  }

  GURL long_url = GoogleUrl::Resolve(base_url_, url);
  //  Don't try to rework an invalid url
  if (!long_url.is_valid() || !long_url.IsStandard()) {
    return false;
  }

  std::string long_url_buffer(GoogleUrl::Spec(long_url));
  StringPiece long_url_str(long_url_buffer);
  size_t to_trim = 0;

  // If we can strip the whole origin (http://www.google.com/) do it,
  // then see if we can strip the prefix of the path.
  if (origin_.length() < long_url_str.length() &&
      GoogleUrl::Origin(long_url) == origin_) {
    to_trim = origin_.length();
    if (to_trim + path_.length() < long_url_str.length() &&
        GoogleUrl::PathSansLeaf(long_url).find(path_) == 0) {
      to_trim += path_.length();
    }
  }

  // If we can't strip the whole origin, see if we can strip off the scheme.
  if (to_trim == 0 && scheme_.length() + 1 < long_url_str.length() &&
      long_url.SchemeIs(scheme_.c_str())) {
    // +1 for : (not included in scheme)
    to_trim = scheme_.length() + 1;
  }

  long_url_str.remove_prefix(to_trim);

  if (long_url_str.length() < url.length()) {
    // If we have a colon before the first slash there are two options:
    // option 1 - we still have our scheme, in which case we're not shortening
    // anything, and can just abort.
    // option 2 - the original url had some nasty scheme-looking stuff in the
    // middle of the url, and now it's at the front.  This causes Badness,
    // revert to the original.
    size_t colon_pos = long_url_str.find(':');
    if (colon_pos != long_url_str.npos) {
      if (long_url_str.rfind('/', colon_pos) == long_url_str.npos) {
        return false;
      }
    }
    GURL resolved_newurl(GoogleUrl::Resolve(base_url_, long_url_str));
    DCHECK(resolved_newurl == long_url);
    if (resolved_newurl != long_url) {
      html_parse_->ErrorHere("Left trimming of %s referring to %s was %s, "
                             "which instead refers to %s.",
                             url.as_string().c_str(), long_url_buffer.c_str(),
                             long_url_str.as_string().c_str(),
                             GoogleUrl::Spec(resolved_newurl).c_str());
      return false;
    }
    *trimmed_url = long_url_str.as_string();
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
    if (Trim(val, &trimmed_val)) {
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
