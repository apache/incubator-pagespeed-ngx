/*
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

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class ResourceTagScannerTest : public HtmlParseTestBase {
 protected:
  ResourceTagScannerTest() {
  }

  virtual bool AddBody() const { return true; }

  // Helper class to collect all external resources.
  class ResourceCollector : public EmptyHtmlFilter {
   public:
    ResourceCollector(HtmlParse* html_parse, StringVector* resources)
        : resources_(resources),
          resource_tag_scanner_(html_parse) {
    }

    virtual void StartElement(HtmlElement* element) {
      HtmlElement::Attribute* src =
          resource_tag_scanner_.ScanElement(element);
      if (src != NULL) {
        resources_->push_back(src->value());
      }
    }

    virtual const char* Name() const { return "ResourceCollector"; }

   private:
    StringVector* resources_;
    ResourceTagScanner resource_tag_scanner_;

    DISALLOW_COPY_AND_ASSIGN(ResourceCollector);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceTagScannerTest);
};

TEST_F(ResourceTagScannerTest, FindTags) {
  StringVector resources;
  ResourceCollector collector(&html_parse_, &resources);
  html_parse_.AddFilter(&collector);
  ValidateNoChanges(
      "simple_script",
      "<script src='myscript.js'></script>\n"
      "<script src='action.as' type='application/ecmascript'></script>\n"
      "<img src=\"image.jpg\"/>\n"
      "<link rel=\"prefetch\" href=\"do_not_find_prefetch\">\n"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"nomedia.css\">\n"
      "<link rel=stylesheet type=text/css href=id.css id=id>\n"
      "<link rel=stylesheet href=no_type.style>\n"
      "<link rel=stylesheet type=text/css href=media.css media=print>");
  ASSERT_EQ(static_cast<size_t>(7), resources.size());
  EXPECT_EQ(std::string("myscript.js"), resources[0]);
  EXPECT_EQ(std::string("action.as"), resources[1]);
  EXPECT_EQ(std::string("image.jpg"), resources[2]);
  EXPECT_EQ(std::string("nomedia.css"), resources[3]);
  EXPECT_EQ(std::string("id.css"), resources[4]);
  EXPECT_EQ(std::string("no_type.style"), resources[5]);
  EXPECT_EQ(std::string("media.css"), resources[6]);
}

}  // namespace net_instaweb
