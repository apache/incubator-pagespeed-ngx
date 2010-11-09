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
#include "net/instaweb/util/public/gzip_inflater.h"
#include "net/instaweb/util/stack_buffer.h"
#include "http_config.h"

extern "C" {
extern module AP_MODULE_DECLARE_DATA pagespeed_module;
}

namespace net_instaweb {

InstawebContext::InstawebContext(request_rec* request,
                                 ApacheRewriteDriverFactory* factory,
                                 const std::string& absolute_url,
                                 bool use_custom_options,
                                 const RewriteOptions& custom_options)
    : content_encoding_(kNone),
      factory_(factory),
      string_writer_(&output_),
      inflater_(NULL) {
  if (use_custom_options) {
    custom_rewriter_.reset(factory->NewCustomRewriteDriver(custom_options));
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
  rewrite_driver_->html_parse()->StartParse(absolute_url);
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
      rewrite_driver_->html_parse()->ParseText(buf, num_inflated_bytes);
    }
  } else {
    rewrite_driver_->html_parse()->ParseText(input, size);
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
    if (strcasecmp(encoding, HttpAttributes::kGzip) == 0) {
      content_encoding_ = kGzip;
    } else if (strcasecmp(encoding, HttpAttributes::kDeflate) == 0) {
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

}  // namespace net_instaweb
