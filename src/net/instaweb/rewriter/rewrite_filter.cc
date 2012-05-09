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
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_segment_encoder.h"

namespace net_instaweb {

class RewriteContext;

// All have a final NUL, for example: {0xEF, 0xBB, 0xBF, 0x0}
const char RewriteFilter::kUtf8Bom[]              = "\xEF\xBB\xBF";
const char RewriteFilter::kUtf16BigEndianBom[]    = "\xFE\xFF";
const char RewriteFilter::kUtf16LittleEndianBom[] = "\xFF\xFE";
const char RewriteFilter::kUtf32BigEndianBom[]    = "\x00\x00\xFE\xFF";
const char RewriteFilter::kUtf32LittleEndianBom[] = "\xFF\xFE\x00\x00";

const char RewriteFilter::kUtf8Charset[]              = "utf-8";
const char RewriteFilter::kUtf16BigEndianCharset[]    = "utf-16be";
const char RewriteFilter::kUtf16LittleEndianCharset[] = "utf-16le";
const char RewriteFilter::kUtf32BigEndianCharset[]    = "utf-32be";
const char RewriteFilter::kUtf32LittleEndianCharset[] = "utf-32le";

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

bool RewriteFilter::StripUTF8BOM(StringPiece* contents) {
  bool result = false;
  StringPiece bom;
  bom.set(kUtf8Bom, STATIC_STRLEN(kUtf8Bom));
  if (contents->starts_with(bom)) {
    contents->remove_prefix(bom.length());
    result = true;
  }
  return result;
}

const char* RewriteFilter::GetCharsetForBOM(const StringPiece contents) {
  // Bad/empty data?
  if (contents == NULL || contents.length() == 0) {
    return NULL;
  }
  // If it starts with a printable ASCII character it can't have a BOM, and
  // since that's the most common case and the comparisons below are relatively
  // expensive, test this here for early exit.
  if (contents[0] >= ' ' && contents[0] <= '~') {
    return NULL;
  }

  // Check for the BOMs we know about. Since some BOMs contain NUL(s) we have
  // to use STATIC_STRLEN and manual StringPiece construction.
  StringPiece bom;
  bom.set(kUtf8Bom, STATIC_STRLEN(kUtf8Bom));
  if (contents.starts_with(bom)) {
    return kUtf8Charset;
  }
  bom.set(kUtf16BigEndianBom, STATIC_STRLEN(kUtf16BigEndianBom));
  if (contents.starts_with(bom)) {
    return kUtf16BigEndianCharset;
  }
  bom.set(kUtf16LittleEndianBom, STATIC_STRLEN(kUtf16LittleEndianBom));
  if (contents.starts_with(bom)) {
    return kUtf16LittleEndianCharset;
  }
  bom.set(kUtf32BigEndianBom, STATIC_STRLEN(kUtf32BigEndianBom));
  if (contents.starts_with(bom)) {
    return kUtf32BigEndianCharset;
  }
  bom.set(kUtf32LittleEndianBom, STATIC_STRLEN(kUtf32LittleEndianBom));
  if (contents.starts_with(bom)) {
    return kUtf32LittleEndianCharset;
  }

  return NULL;
}

}  // namespace net_instaweb
