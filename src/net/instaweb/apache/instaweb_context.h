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

#ifndef NET_INSTAWEB_APACHE_INSTAWEB_CONTEXT_H_
#define NET_INSTAWEB_APACHE_INSTAWEB_CONTEXT_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/apache/apache_rewrite_driver_factory.h"
#include "net/instaweb/automatic/public/html_detector.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_writer.h"

// The httpd header must be after the
// apache_rewrite_driver_factory.h. Otherwise, the compiler will
// complain "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "httpd.h"

namespace net_instaweb {

class ApacheResourceManager;
class GzipInflater;
class RewriteOptions;

const char kPagespeedOriginalUrl[] = "mod_pagespeed_original_url";

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

  InstawebContext(request_rec* request,
                  const ContentType& content_type,
                  ApacheResourceManager* manager,
                  const GoogleString& base_url,
                  bool use_custom_options,
                  const RewriteOptions& custom_options);
  ~InstawebContext();

  void Rewrite(const char* input, int size);
  void Flush();
  void Finish();

  apr_bucket_brigade* bucket_brigade() const { return bucket_brigade_; }
  ContentEncoding content_encoding() const { return  content_encoding_; }
  ApacheResourceManager* manager() { return resource_manager_; }
  const GoogleString& output() { return output_; }
  bool empty() const { return output_.empty(); }
  void clear() { output_.clear(); }  // TODO(jmarantz): needed?

  ResponseHeaders* response_headers() {
    return &response_headers_;
  }

  bool sent_headers() { return sent_headers_; }
  void set_sent_headers(bool sent) { sent_headers_ = sent; }

  // Populated response_headers_ with the request's headers_out table.
  void PopulateHeaders(request_rec* request);

  // Looks up the manager from the server rec.
  // TODO(jmarantz): Is there a better place to put this?  It needs to
  // be used by both mod_instaweb.cc and instaweb_handler.cc.
  static ApacheResourceManager* ManagerFromServerRec(server_rec* server);

  // Returns a fetchable URI from a request, using the request pool.
  static const char* MakeRequestUrl(request_rec* request);

 private:
  void ComputeContentEncoding(request_rec* request);
  void ProcessBytes(const char* input, int size);

  // Checks to see if there was a Furious cookie sent with the request.
  // If there was not, set one, and add a Set-Cookie header to the
  // response headers.
  // If there was one, make sure to set the options state appropriately.
  void SetFuriousStateAndCookie(request_rec* request, RewriteOptions* options);

  static apr_status_t Cleanup(void* object);

  GoogleString output_;  // content after instaweb rewritten.
  apr_bucket_brigade* bucket_brigade_;
  ContentEncoding content_encoding_;
  const ContentType content_type_;

  ApacheResourceManager* resource_manager_;
  RewriteDriver* rewrite_driver_;
  StringWriter string_writer_;
  scoped_ptr<GzipInflater> inflater_;
  HtmlDetector html_detector_;
  GoogleString absolute_url_;
  ResponseHeaders response_headers_;
  bool started_parse_;
  bool sent_headers_;
  bool populated_headers_;

  DISALLOW_COPY_AND_ASSIGN(InstawebContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_INSTAWEB_CONTEXT_H_
