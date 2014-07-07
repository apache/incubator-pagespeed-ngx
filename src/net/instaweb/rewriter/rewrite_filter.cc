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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/rewrite_filter.h"

#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/charset_util.h"
#include "net/instaweb/util/public/string_util.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

class RewriteContext;

RewriteFilter::~RewriteFilter() {
}

void RewriteFilter::DetermineEnabled(GoogleString* disabled_reason) {
  set_is_enabled(true);
  if (UsesPropertyCacheDomCohort()) {
    driver()->set_write_property_cache_dom_cohort(true);
  }
}

const UrlSegmentEncoder* RewriteFilter::encoder() const {
  return driver()->default_encoder();
}

bool RewriteFilter::ComputeOnTheFly() const {
  return false;
}

RewriteContext* RewriteFilter::MakeRewriteContext() {
  return NULL;
}

RewriteContext* RewriteFilter::MakeNestedRewriteContext(
    RewriteContext* parent, const ResourceSlotPtr& slot) {
  return NULL;
}

StringPiece RewriteFilter::GetCharsetForScript(
    const Resource* script,
    const StringPiece attribute_charset,
    const StringPiece enclosing_charset) {
  // 1. If the script has a Content-Type with a charset, use that.
  if (!script->charset().empty()) {
    return script->charset();
  }

  // 2. If the element has a charset attribute, use that.
  if (!attribute_charset.empty()) {
    return attribute_charset;
  }

  // 3. If the script has a BOM, use that.
  StringPiece bom_charset = GetCharsetForBom(script->contents());
  if (!bom_charset.empty()) {
    return bom_charset;
  }

  // 4. Use the charset of the enclosing page, if any.
  if (!enclosing_charset.empty()) {
    return enclosing_charset;
  }

  // Well, we really have no idea.
  return StringPiece(NULL);
}

GoogleString RewriteFilter::GetCharsetForStylesheet(
    const Resource* stylesheet,
    const StringPiece attribute_charset,
    const StringPiece enclosing_charset) {
  // 1. If the stylesheet has a Content-Type with a charset, use that, else
  if (!stylesheet->charset().empty()) {
    return stylesheet->charset().as_string();
  }

  // 2. If the stylesheet has an initial @charset, use that.
  StringPiece css(stylesheet->contents());
  StripUtf8Bom(&css);
  Css::Parser parser(css);
  UnicodeText css_charset = parser.ExtractCharset();
  if (parser.errors_seen_mask() == 0) {
    GoogleString at_charset = UnicodeTextToUTF8(css_charset);
    if (!at_charset.empty()) {
      return at_charset;
    }
  }

  // 3. If the stylesheet has a BOM, use that.
  StringPiece bom_charset = GetCharsetForBom(stylesheet->contents());
  if (!bom_charset.empty()) {
    return bom_charset.as_string();
  }

  // 4. If the element has a charset attribute, use that.
  if (!attribute_charset.empty()) {
    return attribute_charset.as_string();
  }

  // 5. Use the charset of the enclosing page, if any.
  if (!enclosing_charset.empty()) {
    return enclosing_charset.as_string();
  }

  // Well, we really have no idea.
  return GoogleString();
}

const RewriteOptions::Filter* RewriteFilter::RelatedFilters(
    int* num_filters) const {
  *num_filters = 0;
  return NULL;
}

}  // namespace net_instaweb
