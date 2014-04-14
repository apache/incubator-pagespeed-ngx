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
#include <memory>
#include <vector>

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace net_instaweb {

namespace {

typedef std::vector<semantic_type::Category> CategoryVector;

// Helper class to collect all external resources.
class ResourceCollector : public EmptyHtmlFilter {
 public:
  explicit ResourceCollector(StringVector* resources,
                             CategoryVector* resource_category,
                             RewriteDriver* driver)
      : resources_(resources),
        resource_category_(resource_category),
        driver_(driver) {}

  virtual void StartDocument() {
    resources_->clear();
    resource_category_->clear();
  }

  virtual void StartElement(HtmlElement* element) {
    resource_tag_scanner::UrlCategoryVector attributes;
    resource_tag_scanner::ScanElement(element, driver_->options(), &attributes);
    for (int i = 0, n = attributes.size(); i < n; ++i) {
      resources_->push_back(attributes[i].url->DecodedValueOrNull());
      resource_category_->push_back(attributes[i].category);
    }
  }

  virtual const char* Name() const { return "ResourceCollector"; }

 private:
  StringVector* resources_;
  CategoryVector* resource_category_;
  RewriteDriver* driver_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCollector);
};

class ResourceTagScannerTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    collector_.reset(
        new ResourceCollector(
            &resources_, &resource_category_, rewrite_driver()));
    rewrite_driver()->AddFilter(collector_.get());
  }

  virtual bool AddBody() const { return true; }

  StringVector resources_;
  CategoryVector resource_category_;
  scoped_ptr<ResourceCollector> collector_;
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
      "<input type=IMAGE src=find-image.jpg formaction=do-find-formaction>");
  ASSERT_EQ(static_cast<size_t>(2), resources_.size());
  ASSERT_EQ(static_cast<size_t>(2), resource_category_.size());
  EXPECT_STREQ("find-image.jpg", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
  EXPECT_STREQ("do-find-formaction", resources_[1]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[1]);
}

TEST_F(ResourceTagScannerTest, DoFindInputFormaction) {
  ValidateNoChanges(
      "DoFindFormaction",
      "<input formaction=find-formaction>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("find-formaction", resources_[0]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, DoFindButtonFormaction) {
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

TEST_F(ResourceTagScannerTest, DoFindBodyCitation) {
  ValidateNoChanges(
      "NoBodyCitation",
      "<body cite=do-find-body-citation></body>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("do-find-body-citation", resources_[0]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[0]);
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

TEST_F(ResourceTagScannerTest, ImageAndLongdesc) {
  ValidateNoChanges(
      "ImageAndLongdesc",
      "<img src=find-image longdesc=do-find-longdesc>");
  ASSERT_EQ(static_cast<size_t>(2), resources_.size());
  ASSERT_EQ(static_cast<size_t>(2), resource_category_.size());
  EXPECT_STREQ("find-image", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
  EXPECT_STREQ("do-find-longdesc", resources_[1]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[1]);
}

TEST_F(ResourceTagScannerTest, ImageUrlValuedAttribute) {
  options()->ClearSignatureForTesting();
  options()->AddUrlValuedAttribute("img", "data-src", semantic_type::kImage);
  options()->ComputeSignature();

  // Image tag with both src and data-src.  All attributes get returned.
  ValidateNoChanges(
      "ImageAndDataAndLongdesc",
      "<img src=find-image data-src=img2 longdesc=do-find-longdesc>");
  ASSERT_EQ(static_cast<size_t>(3), resources_.size());
  ASSERT_EQ(static_cast<size_t>(3), resource_category_.size());
  EXPECT_STREQ("find-image", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
  EXPECT_STREQ("img2", resources_[1]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[1]);
  EXPECT_STREQ("do-find-longdesc", resources_[2]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[2]);

  // Image tag without src, but with a data-src.  Both data-src and longdesc
  // attributes get returned.
  ValidateNoChanges(
      "ImageDataAndLongdesc",
      "<img data-src=img2 longdesc=do-find-longdesc>");
  ASSERT_EQ(static_cast<size_t>(2), resources_.size());
  ASSERT_EQ(static_cast<size_t>(2), resource_category_.size());
  EXPECT_STREQ("img2", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
  EXPECT_STREQ("do-find-longdesc", resources_[1]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[1]);
}

TEST_F(ResourceTagScannerTest, ImageUrlValuedAttributeOverride) {
  options()->ClearSignatureForTesting();
  options()->AddUrlValuedAttribute("a", "href", semantic_type::kImage);
  options()->ComputeSignature();

  // Detect the href of this a tag is an image..
  ValidateNoChanges(
      "HrefImage",
      "<a href=find-image>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("find-image", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, DoFindLongdesc) {
  ValidateNoChanges(
      "DoFindLongdesc",
      "<img longdesc=do-find-longdesc>");
  ASSERT_EQ(static_cast<size_t>(1), resources_.size());
  ASSERT_EQ(static_cast<size_t>(1), resource_category_.size());
  EXPECT_STREQ("do-find-longdesc", resources_[0]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[0]);
}

TEST_F(ResourceTagScannerTest, FrameSrcAndLongdesc) {
  ValidateNoChanges(
      "FrameSrcAndLongdesc",
      "<frame src=find-frame-src longdesc=do-find-longdesc></frame>");
  ASSERT_EQ(static_cast<size_t>(2), resources_.size());
  ASSERT_EQ(static_cast<size_t>(2), resource_category_.size());
  EXPECT_STREQ("find-frame-src", resources_[0]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category_[0]);
  EXPECT_STREQ("do-find-longdesc", resources_[1]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[1]);
}

TEST_F(ResourceTagScannerTest, IFrameSrcNotLongdesc) {
  ValidateNoChanges(
    "IFrameSrcNotLongdesc",
    "<iframe src=find-iframe-src longdesc=do-find-longdesc></iframe>");
  ASSERT_EQ(static_cast<size_t>(2), resources_.size());
  ASSERT_EQ(static_cast<size_t>(2), resource_category_.size());
  EXPECT_STREQ("find-iframe-src", resources_[0]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category_[0]);
  EXPECT_STREQ("do-find-longdesc", resources_[1]);
  EXPECT_EQ(semantic_type::kHyperlink, resource_category_[1]);
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
      "<video poster=do-find-poster src=find-video-src></video>");
  ASSERT_EQ(static_cast<size_t>(2), resources_.size());
  ASSERT_EQ(static_cast<size_t>(2), resource_category_.size());
  EXPECT_STREQ("do-find-poster", resources_[0]);
  EXPECT_EQ(semantic_type::kImage, resource_category_[0]);
  EXPECT_STREQ("find-video-src", resources_[1]);
  EXPECT_EQ(semantic_type::kOtherResource, resource_category_[1]);
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

}  // namespace

}  // namespace net_instaweb
