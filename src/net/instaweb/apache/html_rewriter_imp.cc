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


#include "net/instaweb/apache/html_rewriter_imp.h"

#include <string>

#include "base/logging.h"
#include "net/instaweb/apache/apache_rewrite_driver_factory.h"
#include "net/instaweb/apache/html_rewriter_config.h"
#include "net/instaweb/apache/pagespeed_server_context.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/gzip_inflater.h"

namespace {
const int kBufSize = 1024;
}  // namespace

namespace html_rewriter {

HtmlRewriterImp::HtmlRewriterImp(PageSpeedServerContext* context,
                                 ContentEncoding encoding,
                                 const std::string& base_url,
                                 const std::string& url, std::string* output)
    : context_(context),
      url_(url),
      rewrite_driver_(context_->rewrite_driver_factory()->GetRewriteDriver()),
      string_writer_(output),
      inflater_(NULL) {
  if (encoding == GZIP) {
    inflater_ = new net_instaweb::GzipInflater();
    inflater_->Init();
  }
  rewrite_driver_->SetBaseUrl(base_url);
  // TODO(lsong): Bypass the string buffer, writer data directly to the next
  // apache bucket.
  rewrite_driver_->SetWriter(&string_writer_);
  rewrite_driver_->html_parse()->StartParse(url_.c_str());
}

HtmlRewriterImp::~HtmlRewriterImp() {
  context_->rewrite_driver_factory()->ReleaseRewriteDriver(rewrite_driver_);
  delete inflater_;
}

void HtmlRewriterImp::Finish() {
  if (inflater_) {
    inflater_->ShutDown();
  }
  rewrite_driver_->html_parse()->FinishParse();
}

void HtmlRewriterImp::Flush() {
  rewrite_driver_->html_parse()->Flush();
}

void HtmlRewriterImp::Rewrite(const char* input, int size) {
  if (inflater_) {
    char buf[kBufSize];
    inflater_->SetInput(input, size);
    while (inflater_->HasUnconsumedInput()) {
      int num_inflated_bytes = inflater_->InflateBytes(buf, kBufSize);
      rewrite_driver_->html_parse()->ParseText(buf, num_inflated_bytes);
    }
  } else {
    rewrite_driver_->html_parse()->ParseText(input, size);
  }
}

}  // namespace html_rewriter
