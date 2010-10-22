/**
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

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/base64_util.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace net_instaweb {

class OutputResource;
class UrlAsyncFetcher;
class Writer;

class RewriteFilter : public EmptyHtmlFilter {
 public:
  explicit RewriteFilter(RewriteDriver* driver, StringPiece filter_prefix)
      : filter_prefix_(filter_prefix.data(), filter_prefix.size()),
        driver_(driver) {
  }
  virtual ~RewriteFilter();

  // Fetches a resource written using the filter.  Filters that
  // encode all the data (URLs, meta-data) needed to reconstruct
  // a rewritten resource in a URL component, this method is the
  // mechanism for the filter to serve the rewritten resource.
  //
  // The flow is that a RewriteFilter is instantiated with
  // a path prefix, e.g. a two letter abbreviation of the
  // filter, like "ce" for CacheExtender.  When it rewrites a
  // resource, it replaces the href with a url constructed as
  //   HOST://PREFIX/ce/WEB64_ENCODED_PROTOBUF
  // The WEB64_ENCODED_PROTOBUF can then be decoded.  for
  // CacheExtender, the protobuf contains the content hash plus
  // the original URL.  For "ir" (ImgRewriterFilter) the protobuf
  // might include the original image URL, plus the pixel-dimensions
  // to which the image was resized.
  virtual bool Fetch(OutputResource* output_resource,
                     Writer* writer,
                     const MetaData& request_header,
                     MetaData* response_headers,
                     UrlAsyncFetcher* fetcher,
                     MessageHandler* message_handler,
                     UrlAsyncFetcher::Callback* callback) = 0;

  // Encodes an arbitrary protobuf to a web-safe string, gzipping it first.
  // The protobuf type used is specific to the filter.  E.g. CssCombineFilter
  // needs a protobuf that can store an variable size array of css files.
  template<class Protobuf>
  static void Encode(const Protobuf& protobuf, std::string* url_safe_id) {
    std::string serialized_url;

    // Add extra scope to ensure that StringOutputStream is destructed
    // prior to using the string it has encoded.  See comment in declaration
    // of StringOutputStream in protobuf/io/zero_copy_stream_impl_lite.h,
    // which says:
    //    The string remains property of the caller, but it MUST NOT be
    //    accessed in any way until the stream is destroyed.
    {
      StringOutputStream sstream(&serialized_url);
      GzipOutputStream::Options options;
      options.format = GzipOutputStream::ZLIB;
      GzipOutputStream zostream(&sstream, options);
      options.compression_level = 9;
      protobuf.SerializeToZeroCopyStream(&zostream);
      zostream.Flush();
    }
    Web64Encode(serialized_url, url_safe_id);
  }

  // Decodes an arbitrary web64-encoded & compressed protobuf.
  template<class Protobuf>
  static bool Decode(StringPiece url_safe_id, Protobuf* protobuf) {
    bool ret = false;
    std::string decoded_resource;
    if (Web64Decode(url_safe_id, &decoded_resource)) {
      ArrayInputStream input(decoded_resource.data(), decoded_resource.size());
      GzipInputStream zistream(&input, GzipInputStream::ZLIB);
      ret = protobuf->ParseFromZeroCopyStream(&zistream);
    }
    return ret;
  }

  const std::string& id() const { return filter_prefix_; }
  HtmlParse* html_parse() { return driver_->html_parse(); }
  ResourceManager* resource_manager() { return driver_->resource_manager(); }

 protected:
  std::string filter_prefix_;  // Prefix that should be used in front of all
                                // rewritten URLs
  RewriteDriver* driver_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RewriteFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_FILTER_H_
