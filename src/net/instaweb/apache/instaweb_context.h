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
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"

// The httpd header must be after the
// apache_rewrite_driver_factory.h. Otherwise, the compiler will
// complain "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "httpd.h"

namespace net_instaweb {

class GzipInflater;
class RewriteOptions;

// We use the following structure to keep the instaweb module context. The
// rewriter will put the rewritten content into the output string when flushed
// or finished. We call Flush when we see the FLUSH bucket, and call Finish when
// we see the EOS bucket.
class InstawebContext {
 public:
  enum ContentEncoding {kNone, kGzip, kDeflate, kOther};
  enum ContentDetectionState {kStart, kHtml, kNotHtml};

  InstawebContext(request_rec* request,
                  net_instaweb::ApacheRewriteDriverFactory* factory,
                  const std::string& base_url,
                  bool use_custom_options,
                  const RewriteOptions& custom_options);
  ~InstawebContext();

  void Rewrite(const char* input, int size);
  void Flush() {
    if (content_detection_state_ == kHtml) {
      rewrite_driver_->html_parse()->Flush();
    }
  }
  void Finish() {
    if (content_detection_state_ == kHtml) {
      rewrite_driver_->html_parse()->FinishParse();
    }
  }
  bool empty() const { return output_.empty(); }
  apr_bucket_brigade* bucket_brigade() const { return bucket_brigade_; }
  const std::string& output() { return output_; }
  void clear() { output_.clear(); }  // TODO(jmarantz): needed?
  ContentEncoding content_encoding() const { return  content_encoding_; }

  // Looks up the factory from the server rec.
  // TODO(jmarantz): Is there a better place to put this?  It needs to
  // be used by both mod_instaweb.cc and instaweb_handler.cc.
  static ApacheRewriteDriverFactory* Factory(server_rec* server);

 private:
  void ComputeContentEncoding(request_rec* request);
  void ProcessBytes(const char* input, int size);
  static apr_status_t Cleanup(void* object);

  std::string output_;  // content after instaweb rewritten.
  apr_bucket_brigade* bucket_brigade_;
  ContentEncoding content_encoding_;

  net_instaweb::ApacheRewriteDriverFactory* factory_;
  net_instaweb::RewriteDriver* rewrite_driver_;
  net_instaweb::StringWriter string_writer_;
  scoped_ptr<GzipInflater> inflater_;
  scoped_ptr<RewriteDriver> custom_rewriter_;
  std::string buffer_;
  SimpleMetaData response_headers_;
  ContentDetectionState content_detection_state_;
  std::string absolute_url_;

  DISALLOW_COPY_AND_ASSIGN(InstawebContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_INSTAWEB_CONTEXT_H_
