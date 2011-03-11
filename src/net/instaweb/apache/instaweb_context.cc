// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jmarantz@google.com (Joshua Marantz)
//         lsong@google.com (Libo Song)

#include "net/instaweb/apache/instaweb_context.h"
#include "net/instaweb/apache/header_util.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/gzip_inflater.h"
#include "net/instaweb/util/stack_buffer.h"
#include "apr_strings.h"
#include "http_config.h"
#include "http_core.h"

extern "C" {
extern module AP_MODULE_DECLARE_DATA pagespeed_module;
}

namespace net_instaweb {

const char kPagespeedOrignalUrl[] = "mod_pagespeed_original_url";

InstawebContext::InstawebContext(request_rec* request,
                                 const ContentType& content_type,
                                 ApacheRewriteDriverFactory* factory,
                                 const std::string& absolute_url,
                                 bool use_custom_options,
                                 const RewriteOptions& custom_options)
    : content_encoding_(kNone),
      content_type_(content_type),
      factory_(factory),
      string_writer_(&output_),
      inflater_(NULL),
      content_detection_state_(kStart),
      absolute_url_(absolute_url) {
  if (use_custom_options) {
    // TODO(jmarantz): this is a temporary hack until we sort out better
    // memory management of RewriteOptions.  This will drag on performance.
    // We need to do this because we are changing RewriteDriver to keep
    // a reference to its options throughout its lifetime to refer to the
    // domain lawyer and other options.
    rewrite_options_.CopyFrom(custom_options);
    custom_rewriter_.reset(factory->NewCustomRewriteDriver(rewrite_options_));
    rewrite_driver_ = custom_rewriter_.get();
  } else {
    rewrite_driver_ = factory->NewRewriteDriver();
  }

  ComputeContentEncoding(request);
  apr_pool_cleanup_register(request->pool, this, Cleanup,
                            apr_pool_cleanup_null);

  bucket_brigade_ = apr_brigade_create(request->pool,
                                       request->connection->bucket_alloc);

  if (content_encoding_ == kGzip || content_encoding_ == kDeflate) {
    // TODO(jmarantz): consider keeping a pool of these if they are expensive
    // to initialize.
    if (content_encoding_ == kGzip) {
      inflater_.reset(new GzipInflater(GzipInflater::kGzip));
    } else {
      inflater_.reset(new GzipInflater(GzipInflater::kDeflate));
    }
    inflater_->Init();
  }


  const char* user_agent = apr_table_get(request->headers_in,
                                         HttpAttributes::kUserAgent);
  rewrite_driver_->SetUserAgent(user_agent);
  // TODO(lsong): Bypass the string buffer, writer data directly to the next
  // apache bucket.
  rewrite_driver_->SetWriter(&string_writer_);
}

InstawebContext::~InstawebContext() {
  if (custom_rewriter_ == NULL) {
    factory_->ReleaseRewriteDriver(rewrite_driver_);
  }
}

void InstawebContext::Rewrite(const char* input, int size) {
  if (inflater_.get() != NULL) {
    char buf[kStackBufferSize];
    inflater_->SetInput(input, size);
    while (inflater_->HasUnconsumedInput()) {
      int num_inflated_bytes = inflater_->InflateBytes(buf, kStackBufferSize);
      ProcessBytes(buf, num_inflated_bytes);
    }
  } else {
    ProcessBytes(input, size);
  }
}

namespace {

// http://en.wikipedia.org/wiki/Byte_order_mark
//
// The byte-order marker sequence will typically appear at the beginning of
// an HTML or XML file.  We probably should be order-sensitive but for now
// we will just treat all such characters as allowable characters preceding
// the HTML.  Note the use of unsigned char here to avoid sign-extending when
// comparing to the int constants.
inline bool IsByteOrderMarkerCharacter(unsigned char c) {
  return ((c == 0xef) || (c == 0xbb) || (c == 0xbf));
}

}  // namespace

void InstawebContext::ProcessBytes(const char* input, int size) {
  // Try to figure out whether this looks like HTML or not, if we haven't
  // figured it out already.  We just scan past whitespace for '<'.
  for (int i = 0; (content_detection_state_ == kStart) && (i < size); ++i) {
    char c = input[i];
    if (c == '<') {
      bool started = rewrite_driver_->StartParseWithType(
          absolute_url_, content_type_);
      if (started) {
        content_detection_state_ = kHtml;
      } else {
        // This is a convenient lie.  The text might be HTML but the
        // URL is invalid, so we will fail to resolve any relative URLs.
        // What we really want is to take mod_pagespeed out of the filter
        // chain, and this construct allows that.
        content_detection_state_ = kNotHtml;
      }
    } else if (!isspace(c) && !IsByteOrderMarkerCharacter(c)) {
      // TODO(jmarantz): figure out whether it's possible to remove our
      // filter from the chain entirely.
      //
      // TODO(jmarantz): look for 'gzip' data.  We do not expect to see
      // this if the Content-Encoding header is set upstream of mod_pagespeed,
      // but we have heard evidence from the field that WordPress plugins and
      // possibly other modules send compressed data through without that
      // header.
      content_detection_state_ = kNotHtml;
    }
  }

  switch (content_detection_state_) {
    case kStart:
      // Handle the corner where the first buffer of text contains
      // only whitespace, which we will retain for the next call.
      buffer_.append(input, size);
      break;

    case kHtml:
      // Looks like HTML: send it through the HTML rewriter.
      if (!buffer_.empty()) {
        rewrite_driver_->ParseText(buffer_);
        buffer_.clear();
      }
      rewrite_driver_->ParseText(input, size);
      break;

    case kNotHtml:
      // Looks like something that's not HTML.  Send it directly to the
      // output buffer.
      output_.append(buffer_.data(), buffer_.size());
      buffer_.clear();
      output_.append(input, size);
      break;
  }
}

apr_status_t InstawebContext::Cleanup(void* object) {
  InstawebContext* ic = static_cast<InstawebContext*>(object);
  delete ic;
  return APR_SUCCESS;
}

void InstawebContext::ComputeContentEncoding(request_rec* request) {
  // Check if the content is gzipped. Steal from mod_deflate.
  const char* encoding = apr_table_get(
      request->headers_out, HttpAttributes::kContentEncoding);
  if (encoding) {
    const char* err_enc = apr_table_get(request->err_headers_out,
                                        HttpAttributes::kContentEncoding);
    if (err_enc) {
      // We don't properly handle stacked encodings now.
      content_encoding_ = kOther;
    }
  } else {
    encoding = apr_table_get(request->err_headers_out,
                             HttpAttributes::kContentEncoding);
  }

  if (encoding) {
    if (StringCaseEqual(encoding, HttpAttributes::kGzip)) {
      content_encoding_ = kGzip;
    } else if (StringCaseEqual(encoding, HttpAttributes::kDeflate)) {
      content_encoding_ = kDeflate;
    } else {
      content_encoding_ = kOther;
    }
  }
}

ApacheRewriteDriverFactory* InstawebContext::Factory(server_rec* server) {
  return static_cast<ApacheRewriteDriverFactory*>
      ap_get_module_config(server->module_config, &pagespeed_module);
}

// This function stores the request uri on the first call, and then
// uses that value for all future calls.  This should prevent the url
// from changing due to changes to the reqeust from other modules.
const char* InstawebContext::MakeRequestUrl(request_rec* request) {
  const char* url = apr_table_get(request->notes, kPagespeedOrignalUrl);
  /*
   * In some contexts we are seeing relative URLs passed
   * into request->unparsed_uri.  But when using mod_slurp, the rewritten
   * HTML contains complete URLs, so this construction yields the host:port
   * prefix twice.
   *
   * TODO(jmarantz): Figure out how to do this correctly at all times.
   * TODO(sligocki): Make this work when URL starts with "https://".
   */
  if (url == NULL) {
    if (strncmp(request->unparsed_uri, "http://", 7) == 0) {
      url = apr_pstrdup(request->pool, request->unparsed_uri);
    } else {
      url = ap_construct_url(request->pool, request->unparsed_uri, request);
    }
    apr_table_setn(request->notes, kPagespeedOrignalUrl, url);
  }
  return url;
}

}  // namespace net_instaweb
