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

#include <cstddef>
#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

// names for Statistics variables.
const char kUrlTrims[] = "url_trims";
const char kUrlTrimSavedBytes[] = "url_trim_saved_bytes";

}  // namespace

namespace net_instaweb {

UrlLeftTrimFilter::UrlLeftTrimFilter(RewriteDriver* rewrite_driver,
                                     Statistics *stats)
    : CommonFilter(rewrite_driver),
      tag_scanner_(rewrite_driver),
      trim_count_(stats->GetVariable(kUrlTrims)),
      trim_saved_bytes_(stats->GetVariable(kUrlTrimSavedBytes)) {
  tag_scanner_.set_find_a_tags(true);
}

void UrlLeftTrimFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kUrlTrims);
  statistics->AddVariable(kUrlTrimSavedBytes);
}

// Do not rewrite the base tag.
void UrlLeftTrimFilter::StartElementImpl(HtmlElement* element) {
  if (element->keyword() != HtmlName::kBase &&
      BaseUrlIsValid()) {
    TrimAttribute(tag_scanner_.ScanElement(element));
  }
}

// Resolve the url we want to trim, and then remove the scheme, origin
// and/or path as appropriate.
bool UrlLeftTrimFilter::Trim(const GoogleUrl& base_url,
                             const StringPiece& url_to_trim,
                             GoogleString* trimmed_url,
                             MessageHandler* handler) {
  if (!base_url.is_valid() || !base_url.is_standard() || url_to_trim.empty()) {
    return false;
  }

  GoogleUrl long_url(base_url, url_to_trim);
  //  Don't try to rework an invalid url
  if (!long_url.is_valid() || !long_url.is_standard()) {
    return false;
  }

  StringPiece long_url_buffer = long_url.Spec();
  size_t to_trim = 0;

  // If we can strip the whole origin (http://www.google.com/) do it,
  // then see if we can strip the prefix of the path.
  StringPiece origin = base_url.Origin();
  if (origin.length() < long_url_buffer.length() &&
      long_url.Origin() == origin) {
    to_trim = origin.length();
    StringPiece path = base_url.PathSansLeaf();

    // If the path still starts with a "//", we can't trim the origin.
    // "//" is not actually the same as a single /, though most
    // servers will do the same thing with it.
    // E.g. on http://example.com/foo.html, don't trim
    // http://example.com//bar.html to //bar or /bar.
    if (long_url_buffer.data()[to_trim + 1] == '/' &&
        long_url_buffer.data()[to_trim] == '/') {
      to_trim = 0;
    } else if (to_trim + path.length() < long_url_buffer.length() &&
               long_url.PathSansLeaf().find(path) == 0) {
      // Don't trim the path off queries in the form http://foo.com/?a=b
      // Instead resolve to /?a=b (not ?a=b, which resolves to
      // index.html?a=b on http://foo.com/index.html).
      if (!long_url.has_query() || long_url.LeafSansQuery().length() > 0) {
        to_trim += path.length();

        // If the path now starts with "//", we need to undo the trim.
        // E.g. on http://example.com/foo/bar/index.html, don't trim
        // http://example.com/foo/bar//baz/other.html to //baz/other.html
        // or to /baz/other.html.
        if (long_url_buffer.data()[to_trim] == '/') {
          to_trim -= path.length();
        }
      }
    }
  }

  // If we can't strip the whole origin, see if we can strip off the scheme.
  StringPiece scheme = base_url.Scheme();
  if (to_trim == 0 && scheme.length() + 1 < long_url_buffer.length() &&
      long_url.SchemeIs(scheme)) {
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
    GoogleUrl resolved_newurl(base_url, trimmed_url_piece);
    DCHECK(resolved_newurl == long_url);
    if (!resolved_newurl.is_valid() || resolved_newurl != long_url) {
      handler->Message(kError, "Left trimming of %s referring to %s was %s, "
                       "which instead refers to %s.",
                       url_to_trim.as_string().c_str(),
                       long_url_buffer.as_string().c_str(),
                       trimmed_url_piece.as_string().c_str(),
                       resolved_newurl.spec_c_str());
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
    GoogleString trimmed_val;
    size_t orig_size = val.size();
    if (Trim(driver_->base_url(), val, &trimmed_val,
             driver_->message_handler())) {
      attr->SetValue(trimmed_val);
      trim_count_->Add(1);
      trim_saved_bytes_->Add(orig_size - trimmed_val.size());
    }
  }
}

}  // namespace net_instaweb
