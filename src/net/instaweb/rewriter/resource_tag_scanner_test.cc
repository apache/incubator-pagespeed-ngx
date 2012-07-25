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

#include "net/instaweb/rewriter/public/resource_tag_scanner.h"

#include <cstddef>
#include <vector>

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
typedef std::vector<semantic_type::Category> CategoryVector;
class ResourceTagScannerTest : public HtmlParseTestBase {
 protected:
  ResourceTagScannerTest() {
  }

  virtual bool AddBody() const { return true; }

  // Helper class to collect all external resources.
  class ResourceCollector : public EmptyHtmlFilter {
   public:
    ResourceCollector(StringVector* resources,
                      CategoryVector* resource_category)
        : resources_(resources),
          resource_category_(resource_category) {
    }

    virtual void StartElement(HtmlElement* element) {
      semantic_type::Category resource_category;
      HtmlElement::Attribute* src = resource_tag_scanner::ScanElement(
          element, NULL /* driver */, &resource_category);
      if (src != NULL) {
        resources_->push_back(src->DecodedValueOrNull());
        resource_category_->push_back(resource_category);
      }
    }

    virtual const char* Name() const { return "ResourceCollector"; }

   private:
    StringVector* resources_;
    CategoryVector* resource_category_;

    DISALLOW_COPY_AND_ASSIGN(ResourceCollector);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceTagScannerTest);
};

TEST_F(ResourceTagScannerTest, FindTags) {
  StringVector resources;
  CategoryVector resource_category;
  ResourceCollector collector(&resources, &resource_category);
  html_parse_.AddFilter(&collector);
  ValidateNoChanges(
      "simple_script",
      "<script src='myscript.js'></script>\n"
      "<script src='action.as' type='application/ecmascript'></script>\n"
      "<img src=\"image.jpg\"/>\n"
      "<link rel=\"prefetch\" href=\"do_find_prefetch\">\n"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"nomedia.css\">\n"
      "<link rel=stylesheet type=text/css href=id.css id=id>\n"
      "<link rel=stylesheet href=no_type.style>\n"
      "<link rel=stylesheet type=text/css href=media.css media=print>"
      "<a href=\"find_link\"/>"
      "<form action=\"find_form_action\"/>"
      "<link rel=StyleSheet href='case.css'>"
      "<body background=background_image.jpg>"
      "<link rel=icon href=favicon.ico>"
      "<link rel=apple-touch-icon href=apple-extension.jpg>"
      "<link rel=apple-touch-icon-precomposed href=apple-extension2.jpg>"
      "<link rel=apple-touch-startup-image href=apple-extension3.jpg>"
      "<input src=dont-find-image.jpg>"
      "<input type=image src=do-find-image.jpg>"
      "<input type=IMAGE src=find-image.jpg formaction=dont-find-formaction>"
      "<input formaction=still-dont-find-formaction>"
      "<button formaction=do-find-formaction></button>"
      "<command icon=some-icon.jpg></command>"
      "<base href=dont-find-base>"
      "<applet codebase=dont-find-applet-codebase></applet>"
      "<object codebase=dont-find-object-codebase></object>"
      "<html manifest=html-manifest></html>"
      "<blockquote cite=blockquote-citation></blockquote>"
      "<body cite=dont-find-body-citation></body>"
      "<q cite=q-citation>"
      "<ins cite=ins-citiation></ins>"
      "<del cite=del-citiation></del>"
      "<area href=find-area-link>"
      "<img src=find-image longdesc=dont-find-longdesc>"
      "<img longdesc=still-dont-find-longdesc>"
      "<frame src=find-frame-src longdesc=dont-find-frame-longdesc></frame>"
      "<iframe src=find-iframe-src longdesc=dont-find-iframe-longdesc></iframe>"
      "<head profile=dont-find-profile></head>"
      "<track src=track-src>"
      "<audio src=audio-src></audio>"
      "<video poster=dont-find-poster src=find-video-src></video>"
      "<embed src=embed-src>"
      "<source src=source-src>"
      "<applet archive=archive-unsafe-because-of-codebase></applet>"
      "<applet code=code-unsafe-because-of-codebase></applet>"
      "<object classid=classid-unsafe-because-of-codebase></object>"
      "<object data=data-unsafe-because-of-codebase></object>"
      "<object archive=archive-unsafe-because-of-codebase></object>"
      "<img usemap=ignore-img-usemap>"
      "<input type=image usemap=ignore-input-usemap>"
      "<object usemap=ignore-object-usemap></object>"
      "<td background=td_background_image.jpg></td>"
      "<th background=th_background_image.jpg></th>"
      "<table background=table_background_image.jpg></table>"
      "<tbody background=tbody_background_image.jpg></tbody>"
      "<tfoot background=tfoot_background_image.jpg></tfoot>"
      "<thead background=thead_background_image.jpg></thead>"
);
  ASSERT_EQ(static_cast<size_t>(40), resources.size());
  EXPECT_EQ(GoogleString("myscript.js"), resources[0]);
  EXPECT_EQ(semantic_type::kScript, resource_category[0]);
  EXPECT_EQ(GoogleString("action.as"), resources[1]);
  EXPECT_EQ(semantic_type::kScript, resource_category[1]);
  EXPECT_EQ(GoogleString("image.jpg"), resources[2]);
  EXPECT_EQ(semantic_type::kImage, resource_category[2]);
  EXPECT_EQ(GoogleString("do_find_prefetch"), resources[3]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category[3]);
  EXPECT_EQ(GoogleString("nomedia.css"), resources[4]);
  EXPECT_EQ(semantic_type::kStylesheet, resource_category[4]);
  EXPECT_EQ(GoogleString("id.css"), resources[5]);
  EXPECT_EQ(semantic_type::kStylesheet, resource_category[5]);
  EXPECT_EQ(GoogleString("no_type.style"), resources[6]);
  EXPECT_EQ(semantic_type::kStylesheet, resource_category[6]);
  EXPECT_EQ(GoogleString("media.css"), resources[7]);
  EXPECT_EQ(semantic_type::kStylesheet, resource_category[7]);
  EXPECT_EQ(GoogleString("find_link"), resources[8]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category[8]);
  EXPECT_EQ(GoogleString("find_form_action"), resources[9]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category[9]);
  EXPECT_EQ(GoogleString("case.css"), resources[10]);
  EXPECT_EQ(semantic_type::kStylesheet, resource_category[10]);
  EXPECT_EQ(GoogleString("background_image.jpg"), resources[11]);
  EXPECT_EQ(semantic_type::kImage, resource_category[11]);
  EXPECT_EQ(GoogleString("favicon.ico"), resources[12]);
  EXPECT_EQ(semantic_type::kImage, resource_category[12]);
  EXPECT_EQ(GoogleString("apple-extension.jpg"), resources[13]);
  EXPECT_EQ(semantic_type::kImage, resource_category[13]);
  EXPECT_EQ(GoogleString("apple-extension2.jpg"), resources[14]);
  EXPECT_EQ(semantic_type::kImage, resource_category[14]);
  EXPECT_EQ(GoogleString("apple-extension3.jpg"), resources[15]);
  EXPECT_EQ(semantic_type::kImage, resource_category[15]);
  EXPECT_EQ(GoogleString("do-find-image.jpg"), resources[16]);
  EXPECT_EQ(semantic_type::kImage, resource_category[16]);
  EXPECT_EQ(GoogleString("find-image.jpg"), resources[17]);
  EXPECT_EQ(semantic_type::kImage, resource_category[17]);
  EXPECT_EQ(GoogleString("do-find-formaction"), resources[18]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category[18]);
  EXPECT_EQ(GoogleString("some-icon.jpg"), resources[19]);
  EXPECT_EQ(semantic_type::kImage, resource_category[19]);
  EXPECT_EQ(GoogleString("html-manifest"), resources[20]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category[20]);
  EXPECT_EQ(GoogleString("blockquote-citation"), resources[21]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category[21]);
  EXPECT_EQ(GoogleString("q-citation"), resources[22]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category[22]);
  EXPECT_EQ(GoogleString("ins-citiation"), resources[23]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category[23]);
  EXPECT_EQ(GoogleString("del-citiation"), resources[24]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category[24]);
  EXPECT_EQ(GoogleString("find-area-link"), resources[25]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category[25]);
  EXPECT_EQ(GoogleString("find-image"), resources[26]);
  EXPECT_EQ(semantic_type::kImage, resource_category[26]);
  EXPECT_EQ(GoogleString("find-frame-src"), resources[27]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category[27]);
  EXPECT_EQ(GoogleString("find-iframe-src"), resources[28]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category[28]);
  EXPECT_EQ(GoogleString("track-src"), resources[29]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category[29]);
  EXPECT_EQ(GoogleString("audio-src"), resources[30]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category[30]);
  EXPECT_EQ(GoogleString("find-video-src"), resources[31]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category[31]);
  EXPECT_EQ(GoogleString("embed-src"), resources[32]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category[32]);
  EXPECT_EQ(GoogleString("source-src"), resources[33]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category[33]);
  EXPECT_EQ(GoogleString("td_background_image.jpg"), resources[34]);
  EXPECT_EQ(semantic_type::kImage, resource_category[34]);
  EXPECT_EQ(GoogleString("th_background_image.jpg"), resources[35]);
  EXPECT_EQ(semantic_type::kImage, resource_category[35]);
  EXPECT_EQ(GoogleString("table_background_image.jpg"), resources[36]);
  EXPECT_EQ(semantic_type::kImage, resource_category[36]);
  EXPECT_EQ(GoogleString("tbody_background_image.jpg"), resources[37]);
  EXPECT_EQ(semantic_type::kImage, resource_category[37]);
  EXPECT_EQ(GoogleString("tfoot_background_image.jpg"), resources[38]);
  EXPECT_EQ(semantic_type::kImage, resource_category[38]);
  EXPECT_EQ(GoogleString("thead_background_image.jpg"), resources[39]);
  EXPECT_EQ(semantic_type::kImage, resource_category[39]);
}
}  // namespace net_instaweb
