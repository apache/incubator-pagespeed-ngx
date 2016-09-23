/*
 * Copyright 2015 Google Inc.
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

// Author: kspoelstra@we-amp.com (Kees Spoelstra)

#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/empty_html_filter.h"

namespace {

const char kHtmlDomain[] = "http://test.com/";
const char kOtherDomain[] = "http://other.test.com/";
const char kFrom1Domain[] = "http://from1.test.com/";
const char kFrom2Domain[] = "http://from2.test.com/";
const char kTo1Domain[] = "http://to1.test.com/";
const char kTo2Domain[] = "http://to2.test.com/";
const char kTo2ADomain[] = "http://to2a.test.com/";
const char kTo2BDomain[] = "http://to2b.test.com/";

}  // namespace

namespace net_instaweb {

class CanModifyUrlsFilter : public EmptyHtmlFilter {
 public:
  CanModifyUrlsFilter():
    EmptyHtmlFilter(),
    can_modify_urls_(false) {
  }
  void set_can_modify_urls(bool value) { can_modify_urls_ = value; }
  bool CanModifyUrls() override { return can_modify_urls_; }
  const char* Name() const override { return "CMURLS"; }

 protected:
  bool can_modify_urls_;
};

class StripSubresourceHintsFilterTestBase : public RewriteTestBase {
 protected:
  void SetUp() override {
    RewriteTestBase::SetUp();
    options()->Disallow("*dontdropme*");
    DomainLawyer* lawyer = options()->WriteableDomainLawyer();
    lawyer->AddRewriteDomainMapping(kTo1Domain, kFrom1Domain,
                                    &message_handler_);
    lawyer->AddRewriteDomainMapping(kTo2Domain, kFrom2Domain,
                                    &message_handler_);
    lawyer->AddShard(kTo2Domain, StrCat(kTo2ADomain, ",", kTo2BDomain),
                     &message_handler_);
    can_modify_urls_filter_.reset(new CanModifyUrlsFilter());
    rewrite_driver()->AddFilter(can_modify_urls_filter_.get());
    CustomSetup();
    rewrite_driver()->AddFilters();
  }

  void ValidateStripSubresourceHint(const char* source, const char* rewritten) {
    can_modify_urls_filter_->set_can_modify_urls(true);
    ValidateExpected("validaterewrite_can_modify_urls_true", source, rewritten);

    can_modify_urls_filter_->set_can_modify_urls(false);
    ValidateExpected("validaterewrite_can_modify_urls_false", source, source);
  }

  virtual void CustomSetup() = 0;

  scoped_ptr<CanModifyUrlsFilter> can_modify_urls_filter_;
};

class StripSubresourceHintsFilterTest :
  public StripSubresourceHintsFilterTestBase {
 protected:
  void CustomSetup() override {}
};

class StripSubresourceHintsFilterTestPreserveStyle :
      public StripSubresourceHintsFilterTestBase {
 protected:
  void CustomSetup() override {
    options()->set_css_preserve_urls(true);
    options()->set_js_preserve_urls(false);
    options()->set_image_preserve_urls(false);
  }
};

class StripSubresourceHintsFilterTestPreserveScript :
      public StripSubresourceHintsFilterTestBase {
 protected:
  void CustomSetup() override {
    options()->set_css_preserve_urls(false);
    options()->set_js_preserve_urls(true);
    options()->set_image_preserve_urls(false);
  }
};

class StripSubresourceHintsFilterTestPreserveImage :
      public StripSubresourceHintsFilterTestBase {
 protected:
  void CustomSetup() override {
    options()->set_css_preserve_urls(false);
    options()->set_js_preserve_urls(false);
    options()->set_image_preserve_urls(true);
  }
};

class StripSubresourceHintsFilterTestFullPreserve : public
  StripSubresourceHintsFilterTestBase {
 protected:
  void CustomSetup() override {
    options()->set_css_preserve_urls(true);
    options()->set_js_preserve_urls(true);
    options()->set_image_preserve_urls(true);
  }
};

class StripSubresourceHintsFilterTestDisabled :
  public StripSubresourceHintsFilterTestBase {
 protected:
  void CustomSetup() override {
    options()->set_preserve_subresource_hints(true);
  }
};

class StripSubresourceHintsFilterTestRewriteLevelPassthrough :
  public StripSubresourceHintsFilterTestBase {
 protected:
  void CustomSetup() override {
    options()->SetRewriteLevel(RewriteOptions::kPassThrough);
  }
};

class StripSubresourceHintsFilterTestRewriteLevelCoreFilters :
  public StripSubresourceHintsFilterTestBase {
 protected:
  void CustomSetup() override {
    options()->SetRewriteLevel(RewriteOptions::kCoreFilters);
  }
};

TEST_F(StripSubresourceHintsFilterTest, PreserveSubResourceHintsIsFalse) {
  EXPECT_FALSE(options()->preserve_subresource_hints());
}

TEST_F(StripSubresourceHintsFilterTest, SingleResourceNoLink) {
  static const char source[] =
    "<head><link rel=\"subresource\"/></head>"
    "<body><img src=\"1.jpg\"/></body>";
  static const char rewritten[] =
    "<head></head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTest, SingleResourceSrcLink) {
  static const char source[] =
    "<head><link rel=\"subresource\" src=\"/test.gif\"/></head>"
    "<body><img src=\"1.jpg\"/></body>";
  static const char rewritten[] =
    "<head></head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTest, SingleResourceValidLink) {
  static const char source[] =
    "<head><link rel=\"subresource\" href=\"/test.gif\"/></head>"
    "<body><img src=\"1.jpg\"/></body>";
  static const char rewritten[] =
    "<head></head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTest, SingleResourceValidPreloadLink) {
  static const char source[] =
    "<head><link rel=\"preload\" href=\"/test.gif\" as=\"image\"/></head>"
    "<body><img src=\"1.jpg\"/></body>";
  static const char rewritten[] =
    "<head></head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTest, SingleResourceExternalLink) {
  static const char source[] =
    "<head>"
    "<link rel=\"subresource\" href=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, source);
}

TEST_F(StripSubresourceHintsFilterTest, MultiResourceMixedLinks) {
  static const char source[] =
    "<head>"
    "<link rel=\"subresource\" href=\"/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  static const char rewritten[] =
    "<head>"
    "<link rel=\"subresource\" href=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTest, SingleResourceRewriteDomain) {
  static const char source[] =
    "<head><link rel=\"subresource\" href=\"http://from1.test.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  static const char rewritten[] =
    "<head></head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTest, SingleResourceDisallow) {
  static const char source[] =
    "<head><link rel=\"subresource\" href=\"/dontdropme/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  static const char rewritten[] =
    "<head><link rel=\"subresource\" href=\"/dontdropme/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

// Even if you turn on preserve images, we still strip all rel=subresource hints
// because we don't know which ones are images.
TEST_F(StripSubresourceHintsFilterTestPreserveImage,
       MultiSubresourcePreserveImages) {
  static const char source[] =
    "<head>"
    "<link rel=\"subresource\" href=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" href=\"/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://from1.test.com/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  static const char rewritten[] =
    "<head>"
    "<link rel=\"subresource\" href=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" href=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTestFullPreserve,
       MultiSubresourcePreserveAll) {
  static const char source[] =
    "<head>"
    "<link rel=\"subresource\" href=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" href=\"/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://from1.test.com/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  static const char rewritten[] =
    "<head>"
    "<link rel=\"subresource\" href=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" href=\"/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://from1.test.com/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

// With rel=preload, if you have set preserve for a type we don't strip it.
TEST_F(StripSubresourceHintsFilterTestPreserveImage, ImagesPreserved) {
  static const char source[] =
      "<link rel=preload as=image href=a.jpg>"
      "<link rel=preload as=script href=a.js>"
      "<link rel=preload as=style href=a.css>";
  static const char rewritten[] =
      "<link rel=preload as=image href=a.jpg>";
  ValidateStripSubresourceHint(source, rewritten);
}
TEST_F(StripSubresourceHintsFilterTestPreserveScript, ScriptsPreserved) {
  static const char source[] =
      "<link rel=preload as=image href=a.jpg>"
      "<link rel=preload as=script href=a.js>"
      "<link rel=preload as=style href=a.css>";
  static const char rewritten[] =
      "<link rel=preload as=script href=a.js>";
  ValidateStripSubresourceHint(source, rewritten);
}
TEST_F(StripSubresourceHintsFilterTestPreserveStyle, StylesPreserved) {
  static const char source[] =
      "<link rel=preload as=image href=a.jpg>"
      "<link rel=preload as=script href=a.js>"
      "<link rel=preload as=style href=a.css>";
  static const char rewritten[] =
      "<link rel=preload as=style href=a.css>";
  ValidateStripSubresourceHint(source, rewritten);
}

// With rel=preload we don't strip unknown types.
TEST_F(StripSubresourceHintsFilterTest, DontStripUnknownTypes) {
  static const char source[] = "<link rel=preload as=font href=a.woff>";
  ValidateStripSubresourceHint(source, source);
}

TEST_F(StripSubresourceHintsFilterTestDisabled,
       PreserveSubResourceHintsIsTrue) {
  can_modify_urls_filter_->set_can_modify_urls(true);
  EXPECT_TRUE(options()->preserve_subresource_hints());
}

TEST_F(StripSubresourceHintsFilterTestDisabled, MultiResourcePreserveAll) {
  can_modify_urls_filter_->set_can_modify_urls(true);
  static const char source[] =
    "<head>"
    "<link rel=\"subresource\" href=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" href=\"/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://from1.test.com/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, source);
}

TEST_F(StripSubresourceHintsFilterTestRewriteLevelPassthrough, MultiResource) {
  static const char source[] =
    "<head>"
    "<link rel=\"subresource\" href=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" href=\"/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://from1.test.com/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body></body>";
  static const char rewritten[] =
    "<head>"
    "<link rel=\"subresource\" href=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" href=\"/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://from1.test.com/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body></body>";
  ValidateExpected("multi_resource", source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTestRewriteLevelCoreFilters, MultiResource) {
  static const char source[] =
    "<head>"
    "<link rel=\"subresource\" href=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" href=\"/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://from1.test.com/test.gif\"/>"
    "<link rel=\"subresource\" href=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body></body>";
  static const char rewritten[] =
    "<head>"
    "<link rel=\"subresource\" href=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" href=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body></body>";
  ValidateExpected("multi_resource", source, rewritten);
}

}  // namespace net_instaweb
