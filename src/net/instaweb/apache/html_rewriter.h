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

#ifndef NET_INSTAWEB_APACHE_HTML_REWRITER_H_
#define NET_INSTAWEB_APACHE_HTML_REWRITER_H_

#include <string>
#include "base/basictypes.h"

struct request_rec;

namespace html_rewriter {

// Forward declaration.
class HtmlRewriterImp;
class PageSpeedServerContext;

enum ContentEncoding {NONE, GZIP, DEFLATE, OTHER};

class HtmlRewriter {
 public:
  // base_url is used by RewriteDriver to resolve relative URLs. For example, in
  // the document, there may be a relative URL foo.css. With the base_url as
  // http://mysite.com/bar/index.html, the relative URL foo.css can be correctly
  // resolved. The string output is where the rewriter will write into.
  //
  // Note (lsong): because the rewriter performs better with more input data, so
  // it wants the flush as late as possible. Therefore, the output won't be
  // avaible util Flush() or Finish().
  HtmlRewriter(PageSpeedServerContext* context,
               ContentEncoding encoding,
               const std::string& base_url,
               const std::string& url, std::string* output);
  ~HtmlRewriter();
  // Rewrite input using internal StringWriter.
  void Rewrite(const char* input, int size);
  void Rewrite(const std::string& input) {
    Rewrite(input.data(), input.size());
  }

  // Flush the re-written content to output.
  void Flush();
  // Flush and finish the re-write.
  void Finish();

  const std::string& get_url() const;
  const std::string& set_url(const std::string& url);
 private:
  HtmlRewriterImp* html_rewriter_imp_;

  DISALLOW_COPY_AND_ASSIGN(HtmlRewriter);
};

}  // namespace html_rewriter

#endif  // NET_INSTAWEB_APACHE_HTML_REWRITER_H_
