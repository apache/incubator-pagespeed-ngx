// Copyright 2010 Google Inc. All Rights Reserved.
// Author: bmcquade@google.com (Bryan McQuade)
//
// Simple HTML minifier, based on pagespeed's minify_html.cc

#include <cstdio>

#include <fstream>
#include <string>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/rewriter/public/collapse_whitespace_filter.h"
#include "net/instaweb/rewriter/public/elide_attributes_filter.h"
#include "net/instaweb/rewriter/public/html_attribute_quote_removal.h"
#include "net/instaweb/rewriter/public/remove_comments_filter.h"
#include "net/instaweb/util/public/file_message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

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
