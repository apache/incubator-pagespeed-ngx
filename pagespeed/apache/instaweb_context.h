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

#ifndef PAGESPEED_APACHE_INSTAWEB_CONTEXT_H_
#define PAGESPEED_APACHE_INSTAWEB_CONTEXT_H_

#include "net/instaweb/http/public/request_context.h"
#include "pagespeed/automatic/html_detector.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/http/content_type.h"

// The httpd header must be after the
// apache_rewrite_driver_factory.h. Otherwise, the compiler will
// complain "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "pagespeed/apache/apache_httpd_includes.h"
#include "apr_pools.h"

namespace net_instaweb {

class ApacheServerContext;
class GzipInflater;
class QueryParams;
class RequestHeaders;
class ResponseHeaders;
class RewriteDriver;
class RewriteOptions;

const char kPagespeedOriginalUrl[] = "mod_pagespeed_original_url";

// Generic deleter meant to be used with apr_pool_cleanup_register().
template <class T>
apr_status_t apache_cleanup(void* object) {
  T* resolved = static_cast<T*>(object);
  delete resolved;
  return APR_SUCCESS;
}

// Context for an HTML rewrite.
//
// One is created for responses that appear to be HTML (although there is
// a basic sanity check that the first non-space char is '<').
//
// The rewriter will put the rewritten content into the output string when
// flushed or finished. We call Flush when we see the FLUSH bucket, and
// call Finish when we see the EOS bucket.
//
// TODO(sligocki): Factor out similarities between this and ProxyFetch.
class InstawebContext {
 public:
  enum ContentEncoding { kNone, kGzip, kDeflate, kOther };
  enum ContentDetectionState { kStart, kHtml, kNotHtml };

  // Takes ownership of request_headers.
  InstawebContext(request_rec* request,
                  RequestHeaders* request_headers,
                  const ContentType& content_type,
                  ApacheServerContext* server_context,
                  const GoogleString& base_url,
                  const RequestContextPtr& request_context,
                  const QueryParams& pagespeed_query_params,
                  const QueryParams& pagespeed_option_cookies,
                  bool use_custom_options,
                  const RewriteOptions& options);
  ~InstawebContext();

  void Rewrite(const char* input, int size);
  void Flush();
  void Finish();

  apr_bucket_brigade* bucket_brigade() const { return bucket_brigade_; }
  ContentEncoding content_encoding() const { return  content_encoding_; }
  ApacheServerContext* apache_server_context() { return server_context_; }
  const GoogleString& output() { return output_; }
  bool empty() const { return output_.empty(); }
  void clear() { output_.clear(); }  // TODO(jmarantz): needed?

  ResponseHeaders* response_headers() {
    return response_headers_.get();
  }

  bool sent_headers() { return sent_headers_; }
  void set_sent_headers(bool sent) { sent_headers_ = sent; }

  // Populated response_headers_ with the request's headers_out table.
  void PopulateHeaders(request_rec* request);

  // Looks up the apache server context from the server rec.
  // TODO(jmarantz): Is there a better place to put this?  It needs to
  // be used by both mod_instaweb.cc and instaweb_handler.cc.
  static ApacheServerContext* ServerContextFromServerRec(server_rec* server);

  // Returns a fetchable URI from a request, using the request pool.
  static const char* MakeRequestUrl(const RewriteOptions& global_options,
                                    request_rec* request);

 private:
  void ComputeContentEncoding(request_rec* request);
  void BlockingPropertyCacheLookup();
  void ProcessBytes(const char* input, int size);

  // Checks to see if there was an experiment cookie sent with the request.
  // If there was not, set one, and add a Set-Cookie header to the
  // response headers.
  // If there was one, make sure to set the options state appropriately.
  void SetExperimentStateAndCookie(request_rec* request,
                                   RewriteOptions* options);

  GoogleString output_;  // content after instaweb rewritten.
  apr_bucket_brigade* bucket_brigade_;
  ContentEncoding content_encoding_;
  const ContentType content_type_;

  ApacheServerContext* server_context_;
  RewriteDriver* rewrite_driver_;
  StringWriter string_writer_;
  scoped_ptr<GzipInflater> inflater_;
  HtmlDetector html_detector_;
  GoogleString absolute_url_;
  scoped_ptr<RequestHeaders> request_headers_;
  scoped_ptr<ResponseHeaders> response_headers_;
  bool started_parse_;
  bool sent_headers_;
  bool populated_headers_;

  DISALLOW_COPY_AND_ASSIGN(InstawebContext);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_APACHE_INSTAWEB_CONTEXT_H_
