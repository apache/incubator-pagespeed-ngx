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
// Author: mdsteele@google.com (Matthew D. Steele)

#include "pagespeed/kernel/html/doctype.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

class DocTypeTest : public testing::Test {
 protected:
  void TestParse(const StringPiece& directive,
                 const ContentType& content_type,
                 const DocType expected_doctype) {
    DocType actual_doctype;
    EXPECT_TRUE(actual_doctype.Parse(TrimDirective(directive), content_type));
    EXPECT_EQ(expected_doctype, actual_doctype);
  }

  void TestParseFailure(const StringPiece& directive,
                        const ContentType& content_type) {
    DocType doctype;
    EXPECT_FALSE(doctype.Parse(TrimDirective(directive), content_type));
  }

  bool IsXhtml(const StringPiece& directive,
               const ContentType& content_type) {
    DocType doctype;
    EXPECT_TRUE(doctype.Parse(TrimDirective(directive), content_type));
    return doctype.IsXhtml();
  }

 private:
  // Trim the "<!" and ">" off of a "<!...>" string.
  StringPiece TrimDirective(const StringPiece& directive) {
    EXPECT_TRUE(directive.starts_with("<!"));
    EXPECT_TRUE(directive.ends_with(">"));
    return directive.substr(2, directive.size() - 3);
  }
};

TEST_F(DocTypeTest, NonDoctypeDirective) {
  TestParseFailure("<!foobar>", kContentTypeHtml);
}

TEST_F(DocTypeTest, UnknownDoctype) {
  TestParse("<!doctype foo bar baz>", kContentTypeHtml, DocType::kUnknown);
}

TEST_F(DocTypeTest, DetectHtml5) {
  TestParse("<!doctype html>", kContentTypeHtml, DocType::kHTML5);
  TestParse("<!doctype HTML>", kContentTypeHtml, DocType::kHTML5);
  TestParse("<!dOcTyPe HtMl>", kContentTypeHtml, DocType::kHTML5);
}

TEST_F(DocTypeTest, DetectXhtml5) {
  TestParse("<!DOCTYPE html>", kContentTypeXhtml, DocType::kXHTML5);
  TestParse("<!DOCTYPE html>", kContentTypeXml, DocType::kXHTML5);
}

TEST_F(DocTypeTest, DetectHtml4) {
  TestParse("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" "
            "\"http://www.w3.org/TR/html4/strict.dtd\">",
            kContentTypeHtml, DocType::kHTML4Strict);
  TestParse(
      "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" "
      "\"http://www.w3.org/TR/html4/loose.dtd\">",
      kContentTypeHtml, DocType::kHTML4Transitional);
}

TEST_F(DocTypeTest, DetectXhtml11) {
  TestParse("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" "
            "\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">",
            kContentTypeXhtml, DocType::kXHTML11);
}

TEST_F(DocTypeTest, DetectXhtml10) {
  TestParse("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
            "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">",
            kContentTypeXhtml, DocType::kXHTML10Strict);
  TestParse(
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" "
      "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">",
      kContentTypeXhtml, DocType::kXHTML10Transitional);
}

TEST_F(DocTypeTest, DetectVariousXhtmlTypes) {
  // Some of these are listed here:
  //   http://www.w3.org/QA/2002/04/valid-dtd-list.html
  EXPECT_TRUE(IsXhtml(
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" "
      "\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">",
      kContentTypeXhtml));
  EXPECT_TRUE(IsXhtml(
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Frameset//EN\" "
      "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd\">",
      kContentTypeHtml));
  EXPECT_TRUE(IsXhtml(
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML+RDFa 1.0//EN\" "
      "\"http://www.w3.org/MarkUp/DTD/xhtml-rdfa-1.dtd\">",
      kContentTypeXhtml));
  EXPECT_TRUE(IsXhtml(
      "<!DOCTYPE html PUBLIC "
      "\"-//W3C//DTD XHTML 1.1 plus MathML 2.0 plus SVG 1.1//EN\" "
      "\"http://www.w3.org/2002/04/xhtml-math-svg/xhtml-math-svg.dtd\">",
      kContentTypeXhtml));
  EXPECT_TRUE(IsXhtml(
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML Basic 1.1//EN\" "
      "\"http://www.w3.org/TR/xhtml-basic/xhtml-basic11.dtd\">",
      kContentTypeXhtml));

  EXPECT_FALSE(IsXhtml(
      "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" "
      "\"http://www.w3.org/TR/html4/strict.dtd\">",
      kContentTypeHtml));
}

}  // namespace net_instaweb
