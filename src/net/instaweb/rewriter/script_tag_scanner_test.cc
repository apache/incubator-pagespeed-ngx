/**
 * Copyright 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: mdsteele@google.com (Matthew D. Steele)

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class ScriptTagScannerTest : public HtmlParseTestBase {
 protected:
  ScriptTagScannerTest() {
  }

  virtual bool AddBody() const { return true; }

  // Helper class to collect script srcs.
  class ScriptCollector : public EmptyHtmlFilter {
   public:
    ScriptCollector(HtmlParse* html_parse, StringVector* script_srcs)
        : script_srcs_(script_srcs),
          script_tag_scanner_(html_parse) {
    }

    virtual void StartElement(HtmlElement* element) {
      HtmlElement::Attribute* src =
          script_tag_scanner_.ParseScriptElement(element);
      if (src != NULL) {
        script_srcs_->push_back(src->value());
      }
    }
    virtual const char* Name() const { return "ScriptCollector"; }

   private:
    StringVector* script_srcs_;
    ScriptTagScanner script_tag_scanner_;

    DISALLOW_COPY_AND_ASSIGN(ScriptCollector);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(ScriptTagScannerTest);
};

TEST_F(ScriptTagScannerTest, FindScriptTag) {
  StringVector scripts;
  ScriptCollector collector(&html_parse_, &scripts);
  html_parse_.AddFilter(&collector);
  ValidateNoChanges("simple_script", "<script src=\"myscript.js\"></script>");
  ASSERT_EQ(static_cast<size_t>(1), scripts.size());
  EXPECT_EQ(std::string("myscript.js"), scripts[0]);
}

}  // namespace net_instaweb
