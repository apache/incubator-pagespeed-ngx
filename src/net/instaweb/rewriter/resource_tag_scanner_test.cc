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
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
typedef std::vector<semantic_type::Category> CategoryVector;
class ResourceTagScannerTest : public HtmlParseTestBase {
 protected:
  ResourceTagScannerTest() : collector_(this) { }

  virtual void SetUp() {
    html_parse_.AddFilter(&collector_);
  }

  virtual bool AddBody() const { return true; }

  // Helper class to collect all external resources.
  class ResourceCollector : public EmptyHtmlFilter {
   public:
    explicit ResourceCollector(ResourceTagScannerTest* test) : test_(test) { }

    virtual void StartElement(HtmlElement* element) {
      semantic_type::Category resource_category;
      HtmlElement::Attribute* src = resource_tag_scanner::ScanElement(
          element, NULL /* driver */, &resource_category);
      if (src != NULL) {
        test_->resources_.push_back(src->DecodedValueOrNull());
        test_->resource_category_.push_back(resource_category);
      }
    }

    virtual const char* Name() const { return "ResourceCollector"; }

   private:
    ResourceTagScannerTest* test_;

    DISALLOW_COPY_AND_ASSIGN(ResourceCollector);
  };

  StringVector resources_;
  CategoryVector resource_category_;
  ResourceCollector collector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceTagScannerTest);
};

TEST_F(ResourceTagScannerTest, SimpleScript) {
  ValidateNoChanges(
      "SimpleScript",
      "<script src='myscript.js'></script>\n");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("myscript.js", resources_[0]);
  EXPECT_EQ(semantic_type::kScript, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, EcmaScript) {
  ValidateNoChanges(
      "EcmaScript",
      "<script src='action.as' type='application/ecmascript'></script>\n");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("action.as", resources_[0]);
  EXPECT_EQ(semantic_type::kScript, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, Image) {
  ValidateNoChanges(
      "Image",
      "<img src=\"image.jpg\"/>\n");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("image.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, Prefetch) {
  ValidateNoChanges(
      "Prefetch",
      "<link rel=\"prefetch\" href=\"do_find_prefetch\">\n");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("do_find_prefetch", resources_[0]);
  EXPECT_EQ(semantic_type::kPrefetch, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, NoMediaCss) {
  ValidateNoChanges(
      "NoMediaCss",
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"nomedia.css\">\n");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("nomedia.css", resources_[0]);
  EXPECT_EQ(semantic_type::kStylesheet, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, IdCss) {
  ValidateNoChanges(
      "IdCss",
      "<link rel=stylesheet type=text/css href=id.css id=id>\n");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("id.css", resources_[0]);
  EXPECT_EQ(semantic_type::kStylesheet, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, NoTypeCss) {
  ValidateNoChanges(
      "NoTypeCss",
      "<link rel=stylesheet href=no_type.style>\n");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("no_type.style", resources_[0]);
  EXPECT_EQ(semantic_type::kStylesheet, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, MediaCss) {
  ValidateNoChanges(
      "MediaCss",
      "<link rel=stylesheet type=text/css href=media.css media=print>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("media.css", resources_[0]);
  EXPECT_EQ(semantic_type::kStylesheet, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, Link) {
  ValidateNoChanges(
      "Link",
      "<a href=\"find_link\"/>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("find_link", resources_[0]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, FormAction) {
  ValidateNoChanges(
      "FormAction",
      "<form action=\"find_form_action\"/>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("find_form_action", resources_[0]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, RelCase) {
  ValidateNoChanges(
      "RelCase",
      "<link rel=StyleSheet href='case.css'>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("case.css", resources_[0]);
  EXPECT_EQ(semantic_type::kStylesheet, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, BodyBackground) {
  ValidateNoChanges(
      "BodyBackground",
      "<body background=background_image.jpg>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("background_image.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, FavIcon) {
  ValidateNoChanges(
      "FavIcon",
      "<link rel=icon href=favicon.ico>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("favicon.ico", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, ShortcutIcon) {
  ValidateNoChanges(
      "ShortcutIcon",
      "<link rel='shortcut icon' href=favicon.ico>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("favicon.ico", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, AppleTouchIcon) {
  ValidateNoChanges(
      "AppleTouchIcon",
      "<link rel=apple-touch-icon href=apple-extension.jpg>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("apple-extension.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, AppleTouchIconPrecomposed) {
  ValidateNoChanges(
      "AppleTouchIconPrecomposed",
      "<link rel=apple-touch-icon-precomposed href=apple-extension2.jpg>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("apple-extension2.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, AppleTouchStartup) {
  ValidateNoChanges(
      "AppleTouchStartup",
      "<link rel=apple-touch-startup-image href=apple-extension3.jpg>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("apple-extension3.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, DontFindImage) {
  ValidateNoChanges(
      "DontFindImage",
      "<input src=dont-find-image.jpg>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, DoFindImage) {
  ValidateNoChanges(
      "DoFindImage",
      "<input type=image src=do-find-image.jpg>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("do-find-image.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, ImageNotAction) {
  ValidateNoChanges(
      "ImageNotAction",
      "<input type=IMAGE src=find-image.jpg formaction=dont-find-formaction>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("find-image.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, DontFindAction) {
  ValidateNoChanges(
      "DontFindAction",
      "<input formaction=still-dont-find-formaction>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, DoFindAction) {
  ValidateNoChanges(
      "DoFindAction",
      "<button formaction=do-find-formaction></button>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("do-find-formaction", resources_[0]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, CommandIcon) {
  ValidateNoChanges(
      "CommandIcon",
      "<command icon=some-icon.jpg></command>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("some-icon.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, DontFindBase) {
  ValidateNoChanges(
      "DontFindBase",
      "<base href=dont-find-base>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, DontFindApplet) {
  ValidateNoChanges(
      "DontFindApplet",
      "<applet codebase=dont-find-applet-codebase></applet>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, DontFindObject) {
  ValidateNoChanges(
      "DontFindObject",
      "<object codebase=dont-find-object-codebase></object>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, Manifest) {
  ValidateNoChanges(
      "Manifest",
      "<html manifest=html-manifest></html>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("html-manifest", resources_[0]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, BlockquoteCitation) {
  ValidateNoChanges(
      "BlockquoteCitation",
      "<blockquote cite=blockquote-citation></blockquote>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("blockquote-citation", resources_[0]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, DontFindBodyCitation) {
  ValidateNoChanges(
      "NoBodyCitation",
      "<body cite=dont-find-body-citation></body>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, QCitation) {
  ValidateNoChanges(
      "QCitation",
      "<q cite=q-citation>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("q-citation", resources_[0]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, InsCitation) {
  ValidateNoChanges(
      "InsCitation",
      "<ins cite=ins-citation></ins>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("ins-citation", resources_[0]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, DelCitation) {
  ValidateNoChanges(
      "DelCitation",
      "<del cite=del-citation></del>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("del-citation", resources_[0]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, AreaLink) {
  ValidateNoChanges(
      "AreaLink",
      "<area href=find-area-link>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("find-area-link", resources_[0]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, ImageNotLongdesc) {
  ValidateNoChanges(
      "ImageNotLongdesc",
      "<img src=find-image longdesc=dont-find-longdesc>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("find-image", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, DontFindLongdesc) {
  ValidateNoChanges(
      "DontFindLongdesc",
      "<img longdesc=still-dont-find-longdesc>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, FrameSrcNotLongdesc) {
  ValidateNoChanges(
      "FrameSrcNotLongdesc",
      "<frame src=find-frame-src longdesc=dont-find-frame-longdesc></frame>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("find-frame-src", resources_[0]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, IFrameSrcNotLongdesc) {
  ValidateNoChanges(
    "IFrameSrcNotLongdesc",
    "<iframe src=find-iframe-src longdesc=dont-find-iframe-longdesc></iframe>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("find-iframe-src", resources_[0]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, DontFindProfile) {
  ValidateNoChanges(
      "DontFindProfile",
      "<head profile=dont-find-profile></head>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, TrackSrc) {
  ValidateNoChanges(
      "TrackSrc",
      "<track src=track-src>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("track-src", resources_[0]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, AudioSrc) {
  ValidateNoChanges(
      "AudioSrc",
      "<audio src=audio-src></audio>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("audio-src", resources_[0]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, VideoSrc) {
  ValidateNoChanges(
      "VideoSrc",
      "<video poster=dont-find-poster src=find-video-src></video>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("find-video-src", resources_[0]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, EmbedSrc) {
  ValidateNoChanges(
      "EmbedSrc",
      "<embed src=embed-src>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("embed-src", resources_[0]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, SourceSrc) {
  ValidateNoChanges(
      "SourceSrc",
      "<source src=source-src>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("source-src", resources_[0]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, DontFindArchive) {
  ValidateNoChanges(
      "DontFindArchive",
      "<applet archive=archive-unsafe-because-of-codebase></applet>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, DontFindCode) {
  ValidateNoChanges(
      "DontFindCode",
      "<applet code=code-unsafe-because-of-codebase></applet>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, DontFindClassid) {
  ValidateNoChanges(
      "DontFindClassid",
      "<object classid=classid-unsafe-because-of-codebase></object>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, DontFindData) {
  ValidateNoChanges(
      "DontFindData",
      "<object data=data-unsafe-because-of-codebase></object>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, DontFindObjectArchive) {
  ValidateNoChanges(
      "DontFindObjectArchive",
      "<object archive=archive-unsafe-because-of-codebase></object>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, DontFindUsemap) {
  ValidateNoChanges(
      "DontFindUsemap",
      "<img usemap=ignore-img-usemap>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, DontFindImageUsemap) {
  ValidateNoChanges(
      "DontFindImageUsemap",
      "<input type=image usemap=ignore-input-usemap>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, DontFindObjectUsemap) {
  ValidateNoChanges(
      "DontFindObjectUsemap",
      "<object usemap=ignore-object-usemap></object>");
  EXPECT_TRUE(resources_.empty());
  EXPECT_TRUE(resource_category_.empty());
}

TEST_F(ResourceTagScannerTest, TdBackgroundImage) {
  ValidateNoChanges(
      "TdBackgroundImage",
      "<td background=td_background_image.jpg></td>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("td_background_image.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, ThBackgroundImage) {
  ValidateNoChanges(
      "ThBackgroundImage",
      "<th background=th_background_image.jpg></th>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("th_background_image.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, TableBackgroundImage) {
  ValidateNoChanges(
      "TableBackgroundImage",
      "<table background=table_background_image.jpg></table>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("table_background_image.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, TBodyBackgroundImage) {
  ValidateNoChanges(
      "TBodyBackgroundImage",
      "<tbody background=tbody_background_image.jpg></tbody>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("tbody_background_image.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, TFootBackgroundImage) {
  ValidateNoChanges(
      "TFootBackgroundImage",
      "<tfoot background=tfoot_background_image.jpg></tfoot>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("tfoot_background_image.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, THeadBackgroundImage) {
  ValidateNoChanges(
      "THeadBackgroundImage",
      "<thead background=thead_background_image.jpg></thead>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("thead_background_image.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

}  // namespace net_instaweb
