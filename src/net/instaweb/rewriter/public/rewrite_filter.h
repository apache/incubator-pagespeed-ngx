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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class OutputResource;
class RewriteContext;
class RewriteDriver;
class UrlSegmentEncoder;

class RewriteFilter : public CommonFilter {
 public:
  // The Byte-Order-Mark (BOM) for the various UTF encodings.
  static const char kUtf8Bom[];
  static const char kUtf16BigEndianBom[];
  static const char kUtf16LittleEndianBom[];
  static const char kUtf32BigEndianBom[];
  static const char kUtf32LittleEndianBom[];

  // The charset equivalent for each of the above BOMs.
  static const char kUtf8Charset[];
  static const char kUtf16BigEndianCharset[];
  static const char kUtf16LittleEndianCharset[];
  static const char kUtf32BigEndianCharset[];
  static const char kUtf32LittleEndianCharset[];

  explicit RewriteFilter(RewriteDriver* driver)
      : CommonFilter(driver) {
  }
  virtual ~RewriteFilter();

  virtual const char* id() const = 0;

  // Create an input resource by decoding output_resource using the
  // filter's. Assures legality by explicitly permission-checking the result.
  ResourcePtr CreateInputResourceFromOutputResource(
      OutputResource* output_resource);

  // All RewriteFilters define how they encode URLs and other
  // associated information needed for a rewrite into a URL.
  // The default implementation handles a single URL with
  // no extra data.  The filter owns the encoder.
  virtual const UrlSegmentEncoder* encoder() const;

  // If this method returns true, the data output of this filter will not be
  // cached, and will instead be recomputed on the fly every time it is needed.
  // (However, the transformed URL and similar metadata in CachedResult will be
  //  kept in cache).
  //
  // The default implementation returns false.
  virtual bool ComputeOnTheFly() const;

  // Generates a RewriteContext appropriate for this filter.
  // Default implementation returns NULL.  This must be overridden by
  // filters.  This is used to implement Fetch.
  virtual RewriteContext* MakeRewriteContext();

  // Generates a nested RewriteContext appropriate for this filter.
  // Default implementation returns NULL.
  // This is used to implement ajax rewriting.
  virtual RewriteContext* MakeNestedRewriteContext(
      RewriteContext* parent, const ResourceSlotPtr& slot);

  // Strips any initial UTF-8 BOM (Byte Order Mark) from the given contents.
  // Returns true if a BOM was stripped, false if not.
  //
  // In addition to specifying the encoding in the ContentType header, one
  // can also specify it at the beginning of the file using a Byte Order Mark.
  //
  // Bytes        Encoding Form
  // 00 00 FE FF  UTF-32, big-endian
  // FF FE 00 00  UTF-32, little-endian
  // FE FF        UTF-16, big-endian
  // FF FE        UTF-16, little-endian
  // EF BB BF     UTF-8
  // See: http://www.unicode.org/faq/utf_bom.html
  //
  // TODO(nforman): Possibly handle stripping BOMs from non-utf-8 files.
  // We currently handle only utf-8 BOM because we assume the resources
  // we get are not in utf-16 or utf-32 when we read and parse them, anyway.
  static bool StripUTF8BOM(StringPiece* contents);

  // Return the charset string for the given contents' BOM if any. If the
  // contents start with one of the BOMs defined above then the corresponding
  // charset string is returned, otherwise NULL.
  static const char* GetCharsetForBOM(const StringPiece contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(RewriteFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_FILTER_H_
