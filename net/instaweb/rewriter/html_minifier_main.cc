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
// Author: bmcquade@google.com (Bryan McQuade)
//
// Simple HTML minifier, based on pagespeed's minify_html.cc

#include <cstdio>
#include <fstream>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/html/collapse_whitespace_filter.h"
#include "pagespeed/kernel/html/elide_attributes_filter.h"
#include "pagespeed/kernel/html/html_attribute_quote_removal.h"
#include "pagespeed/kernel/html/html_parse.h"
#include "pagespeed/kernel/html/html_writer_filter.h"
#include "pagespeed/kernel/html/remove_comments_filter.h"

namespace {

using net_instaweb::StrCat;

void ReadFileToStringOrDie(const char* filename,
                           GoogleString* dest) {
  std::ifstream file_stream;
  file_stream.open(filename, std::ifstream::in | std::ifstream::binary);
  CHECK(!file_stream.fail());
  dest->assign(std::istreambuf_iterator<char>(file_stream),
               std::istreambuf_iterator<char>());
  file_stream.close();
  CHECK(!file_stream.fail());
}

void WriteStringToFileOrDie(const GoogleString& src,
                            const char* filename) {
  std::ofstream file_stream;
  file_stream.open(filename, std::ifstream::out | std::ifstream::binary);
  CHECK(!file_stream.fail());
  file_stream.write(src.c_str(), src.size());
  file_stream.close();
  CHECK(!file_stream.fail());
}

class HtmlMinifier {
 public:
  explicit HtmlMinifier();
  ~HtmlMinifier();

  // Return true if successful, false on error.
  bool MinifyHtml(const GoogleString& input_name,
                  const GoogleString& input,
                  GoogleString* output);

 private:
  net_instaweb::FileMessageHandler message_handler_;
  net_instaweb::HtmlParse html_parse_;
  net_instaweb::RemoveCommentsFilter remove_comments_filter_;
  net_instaweb::ElideAttributesFilter elide_attributes_filter_;
  net_instaweb::HtmlAttributeQuoteRemoval quote_removal_filter_;
  net_instaweb::CollapseWhitespaceFilter collapse_whitespace_filter_;
  net_instaweb::HtmlWriterFilter html_writer_filter_;

  DISALLOW_COPY_AND_ASSIGN(HtmlMinifier);
};

HtmlMinifier::HtmlMinifier()
    : message_handler_(stderr),
      html_parse_(&message_handler_),
      remove_comments_filter_(&html_parse_),
      elide_attributes_filter_(&html_parse_),
      quote_removal_filter_(&html_parse_),
      collapse_whitespace_filter_(&html_parse_),
      html_writer_filter_(&html_parse_) {
  html_parse_.AddFilter(&remove_comments_filter_);
  html_parse_.AddFilter(&elide_attributes_filter_);
  html_parse_.AddFilter(&quote_removal_filter_);
  html_parse_.AddFilter(&collapse_whitespace_filter_);
  html_parse_.AddFilter(&html_writer_filter_);
}

HtmlMinifier::~HtmlMinifier() {}

bool HtmlMinifier::MinifyHtml(const GoogleString& input_name,
                              const GoogleString& input,
                              GoogleString* output) {
  net_instaweb::StringWriter string_writer(output);
  html_writer_filter_.set_writer(&string_writer);

  GoogleString url = StrCat("http://html_minifier.com/", input_name, ".html");
  html_parse_.StartParse(url);
  html_parse_.ParseText(input.data(), input.size());
  html_parse_.FinishParse();

  html_writer_filter_.set_writer(NULL);

  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: minify_html <input> <output>\n");
    return 1;
  }

  GoogleString original;
  ReadFileToStringOrDie(argv[1], &original);

  GoogleString minified;
  HtmlMinifier html_minifier;
  html_minifier.MinifyHtml(argv[1], original, &minified);

  WriteStringToFileOrDie(minified, argv[2]);
  return 0;
}
