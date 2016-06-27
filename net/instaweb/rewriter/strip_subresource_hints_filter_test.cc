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

class CanModifyUrlsFilter:public EmptyHtmlFilter {
 public:
  CanModifyUrlsFilter():
    EmptyHtmlFilter(),
    can_modify_urls_(false) {
  }
  void set_can_modify_urls(bool value) { can_modify_urls_ = value; }
  virtual bool CanModifyUrls() { return can_modify_urls_; }
  virtual const char* Name() const { return "CMURLS"; }

 protected:
  bool can_modify_urls_;
};

class StripSubresourceHintsFilterTestBase : public RewriteTestBase {
 protected:
  virtual void SetUp() {
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
  void ValidateStripSubresourceHint(const char *source, const char *rewritten) {
    can_modify_urls_filter_->set_can_modify_urls(true);
    ValidateExpected("validaterewrite_can_modify_urls_true", source, rewritten);

    can_modify_urls_filter_->set_can_modify_urls(false);
    ValidateExpected("validaterewrite_can_modify_urls_false", source, source);
  }
  virtual void CustomSetup() { }

  scoped_ptr<CanModifyUrlsFilter> can_modify_urls_filter_;
};

class StripSubresourceHintsFilterTest :
  public StripSubresourceHintsFilterTestBase {
};

class StripSubresourceHintsFilterTestPartialPreserve :
  public StripSubresourceHintsFilterTestBase {
 protected:
  virtual void CustomSetup() {
    options()->set_css_preserve_urls(false);
    options()->set_js_preserve_urls(false);
    options()->set_image_preserve_urls(true);
  }
};

class StripSubresourceHintsFilterTestFullPreserve : public
  StripSubresourceHintsFilterTestBase {
 protected:
  virtual void CustomSetup() {
    options()->set_css_preserve_urls(true);
    options()->set_js_preserve_urls(true);
    options()->set_image_preserve_urls(true);
  }
};

class StripSubresourceHintsFilterTestDisabled :
  public StripSubresourceHintsFilterTestBase {
 protected:
  virtual void CustomSetup() {
    options()->set_preserve_subresource_hints(true);
  }
};

class StripSubresourceHintsFilterTestRewriteLevelPassthrough :
  public StripSubresourceHintsFilterTestBase {
 protected:
  virtual void CustomSetup() {
    options()->SetRewriteLevel(RewriteOptions::kPassThrough);
  }
};

class StripSubresourceHintsFilterTestRewriteLevelCoreFilters :
  public StripSubresourceHintsFilterTestBase {
 protected:
  virtual void CustomSetup() {
    options()->SetRewriteLevel(RewriteOptions::kCoreFilters);
  }
};

TEST_F(StripSubresourceHintsFilterTest, PreserveSubResourceHintsIsFalse) {
  EXPECT_FALSE(options()->preserve_subresource_hints());
}

TEST_F(StripSubresourceHintsFilterTest, SingleResourceNoLink) {
  const char *source =
    "<head><link rel=\"subresource\"/></head>"
    "<body><img src=\"1.jpg\"/></body>";
  const char *rewritten =
    "<head></head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTest, SingleResourceValidLink) {
  const char *source =
    "<head><link rel=\"subresource\" src=\"/test.gif\"/></head>"
    "<body><img src=\"1.jpg\"/></body>";
  const char *rewritten =
    "<head></head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTest, SingleResourceValidPreloadLink) {
  const char *source =
    "<head><link rel=\"preload\" src=\"/test.gif\" as=\"image\"/></head>"
    "<body><img src=\"1.jpg\"/></body>";
  const char *rewritten =
    "<head></head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTest, SingleResourceExternalLink) {
  const char *source =
    "<head>"
    "<link rel=\"subresource\" src=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, source);
}

TEST_F(StripSubresourceHintsFilterTest, MultiResourceMixedLinks) {
  const char *source =
    "<head>"
    "<link rel=\"subresource\" src=\"/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  const char *rewritten =
    "<head>"
    "<link rel=\"subresource\" src=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTest, SingleResourceRewriteDomain) {
  const char *source =
    "<head><link rel=\"subresource\" src=\"http://from1.test.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  const char *rewritten =
    "<head></head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTest, SingleResourceDisallow) {
  const char *source =
    "<head><link rel=\"subresource\" src=\"/dontdropme/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  const char *rewritten =
    "<head><link rel=\"subresource\" src=\"/dontdropme/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTestPartialPreserve,
        MultiResourcePreserveAll) {
  const char *source =
    "<head>"
    "<link rel=\"subresource\" src=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" src=\"/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://from1.test.com/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  const char *rewritten =
    "<head>"
    "<link rel=\"subresource\" src=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" src=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}



TEST_F(StripSubresourceHintsFilterTestFullPreserve,
        MultiResourcePreserveAll) {
  const char *source =
    "<head>"
    "<link rel=\"subresource\" src=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" src=\"/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://from1.test.com/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  const char *rewritten =
    "<head>"
    "<link rel=\"subresource\" src=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" src=\"/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://from1.test.com/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTestDisabled,
        PreserveSubResourceHintsIsTrue) {
  can_modify_urls_filter_->set_can_modify_urls(true);
  EXPECT_TRUE(options()->preserve_subresource_hints());
}

TEST_F(StripSubresourceHintsFilterTestDisabled,
        MultiResourcePreserveAll) {
  can_modify_urls_filter_->set_can_modify_urls(true);
  const char *source =
    "<head>"
    "<link rel=\"subresource\" src=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" src=\"/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://from1.test.com/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body><img src=\"1.jpg\"/></body>";
  ValidateStripSubresourceHint(source, source);
}

TEST_F(StripSubresourceHintsFilterTestRewriteLevelPassthrough,
        MultiResource) {
  const char *source =
    "<head>"
    "<link rel=\"subresource\" src=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" src=\"/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://from1.test.com/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body></body>";
  const char *rewritten =
    "<head>"
    "<link rel=\"subresource\" src=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" src=\"/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://from1.test.com/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body></body>";
  ValidateExpected("multi_resource", source, rewritten);
}

TEST_F(StripSubresourceHintsFilterTestRewriteLevelCoreFilters,
        MultiResource) {
  const char *source =
    "<head>"
    "<link rel=\"subresource\" src=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" src=\"/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://from1.test.com/test.gif\"/>"
    "<link rel=\"subresource\" src=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body></body>";
  const char *rewritten =
    "<head>"
    "<link rel=\"subresource\" src=\"/dontdropme.gif\"/>"
    "<link rel=\"subresource\" src=\"http://www.example.com/test.gif\"/>"
    "</head>"
    "<body></body>";
  ValidateExpected("multi_resource", source, rewritten);
}

}  // namespace net_instaweb
