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

#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/charset_util.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_segment_encoder.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

class RewriteContext;

RewriteFilter::~RewriteFilter() {
}

ResourcePtr RewriteFilter::CreateInputResourceFromOutputResource(
    OutputResource* output_resource) {
  ResourcePtr input_resource;
  StringVector urls;
  ResourceContext data;
  if (encoder()->Decode(output_resource->name(), &urls, &data,
                        driver_->message_handler()) &&
      (urls.size() == 1)) {
    GoogleUrl base_gurl(output_resource->decoded_base());
    GoogleUrl resource_url(base_gurl, urls[0]);
    StringPiece output_base = output_resource->resolved_base();
    if (output_base == driver_->base_url().AllExceptLeaf() ||
        output_base == GoogleUrl(driver_->decoded_base()).AllExceptLeaf()) {
      input_resource = driver_->CreateInputResource(resource_url);
    } else if (driver_->MayRewriteUrl(base_gurl, resource_url)) {
      input_resource = driver_->CreateInputResource(resource_url);
    }
  }
  return input_resource;
}

const UrlSegmentEncoder* RewriteFilter::encoder() const {
  return driver_->default_encoder();
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

}  // namespace net_instaweb
