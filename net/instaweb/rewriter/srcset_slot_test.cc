/*
 * Copyright 2016 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)

// Unit-test the srcset slot.

#include "net/instaweb/rewriter/public/srcset_slot.h"

#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_writer_filter.h"
#include "pagespeed/kernel/http/content_type.h"

namespace {

static const char kHtmlUrl[] = "http://www.example.com/dir/a.html";

}  // namespace

namespace net_instaweb {

TEST(SrcSetSlotParseTest, ParseAndSerialize) {
  std::vector<SrcSetSlotCollection::ImageCandidate> out;
  SrcSetSlotCollection::ParseSrcSet(
      "    ,a.jpg, b.jpg 100w,, c.jpg 10h, d.jpg (very, fancy) , e,f.jpg 10w",
      &out);
  ASSERT_EQ(5, out.size());
  EXPECT_EQ("a.jpg", out[0].url);
  EXPECT_EQ("", out[0].descriptor);

  EXPECT_EQ("b.jpg", out[1].url);
  EXPECT_EQ("100w", out[1].descriptor);

  EXPECT_EQ("c.jpg", out[2].url);
  EXPECT_EQ("10h", out[2].descriptor);

  EXPECT_EQ("d.jpg", out[3].url);
  EXPECT_EQ("(very, fancy)", out[3].descriptor);

  EXPECT_EQ("e,f.jpg", out[4].url);
  EXPECT_EQ("10w", out[4].descriptor);

  EXPECT_EQ("a.jpg, b.jpg 100w, c.jpg 10h, d.jpg (very, fancy), e,f.jpg 10w",
            SrcSetSlotCollection::Serialize(out));

  std::vector<SrcSetSlotCollection::ImageCandidate> out2;
  SrcSetSlotCollection::ParseSrcSet(
      "a.jpg ,b.jpg 100w , c.jpg 200w, d.jpg",
      &out2);
  ASSERT_EQ(4, out2.size());
  EXPECT_EQ("a.jpg", out2[0].url);
  EXPECT_EQ("", out2[0].descriptor);

  EXPECT_EQ("b.jpg", out2[1].url);
  EXPECT_EQ("100w", out2[1].descriptor);

  EXPECT_EQ("c.jpg", out2[2].url);
  EXPECT_EQ("200w", out2[2].descriptor);

  EXPECT_EQ("d.jpg", out2[3].url);
  EXPECT_EQ("", out2[3].descriptor);

  EXPECT_EQ("a.jpg, b.jpg 100w, c.jpg 200w, d.jpg",
            SrcSetSlotCollection::Serialize(out2));
}

class SrcSetSlotTest : public RewriteTestBase {
 protected:
  bool AddBody() const override { return false; }

  void SetUp() override {
    RewriteTestBase::SetUp();

    RewriteDriver* driver = rewrite_driver();
    driver->AddFilters();
    ASSERT_TRUE(driver->StartParseId(kHtmlUrl, "srcset_slot_test",
                                     kContentTypeHtml));
    element_ = driver->NewElement(nullptr, HtmlName::kImg);
    driver->AddAttribute(element_, HtmlName::kSrcset,
                         "a.jpg, b.jpg 100w, c.png 1000w");
    attribute_ = element_->FindAttribute(HtmlName::kSrcset);
    driver->AddElement(element_, 42 /* line number */);
    driver->CloseElement(element_, HtmlElement::BRIEF_CLOSE, 43 /* line # */);
  }

  void TearDown() override {
    rewrite_driver()->FinishParse();
    RewriteTestBase::TearDown();
  }

  GoogleString GetHtmlDomAsString() {
    output_buffer_.clear();
    html_parse()->ApplyFilter(html_writer_filter_.get());
    return output_buffer_;
  }

  HtmlElement* element_;
  HtmlElement::Attribute* attribute_;
};

TEST_F(SrcSetSlotTest, BasicOperation) {
  SetupWriter();
  RefCountedPtr<SrcSetSlotCollection> collection(new SrcSetSlotCollection(
      rewrite_driver(),
      rewrite_driver()->FindFilter("ic"),
      element_,
      attribute_));
  ASSERT_EQ(3, collection->num_image_candidates());
  EXPECT_EQ("a.jpg", collection->url(0));
  EXPECT_EQ("", collection->descriptor(0));
  EXPECT_EQ("b.jpg", collection->url(1));
  EXPECT_EQ("100w", collection->descriptor(1));
  EXPECT_EQ("c.png", collection->url(2));
  EXPECT_EQ("1000w", collection->descriptor(2));
  ResourceSlotPtr slot0(collection->slot(0));
  EXPECT_EQ("http://www.example.com/dir/a.jpg", slot0->resource()->url());
  ResourceSlotPtr slot1(collection->slot(1));
  EXPECT_EQ("http://www.example.com/dir/b.jpg", slot1->resource()->url());
  ResourceSlotPtr slot2(collection->slot(2));
  EXPECT_EQ("http://www.example.com/dir/c.png", slot2->resource()->url());

  // Now rewrite the 3 slots, but only render 2, with 1 prevented
  // from rendering.
  bool unused;
  GoogleUrl optimized_a("http://www.example.com/dir/a.pagespeed.webp");
  slot0->SetResource(
    rewrite_driver()->CreateInputResource(optimized_a, &unused));

  GoogleUrl optimized_b("http://www.example.com/dir/b.pagespeed.webp");
  slot1->SetResource(
    rewrite_driver()->CreateInputResource(optimized_b, &unused));

  GoogleUrl optimized_c("http://www.example.com/dir/c.pagespeed.png");
  slot2->SetResource(
    rewrite_driver()->CreateInputResource(optimized_c, &unused));

  slot0->set_disable_rendering(true);
  slot0->Render();
  slot1->Render();

  EXPECT_EQ("srcset_slot_test: candidate image 0 of srcset at 42-43",
            slot0->LocationString());
  EXPECT_EQ("srcset_slot_test: candidate image 1 of srcset at 42-43",
            slot1->LocationString());
  EXPECT_EQ("srcset_slot_test: candidate image 2 of srcset at 42-43",
            slot2->LocationString());

  EXPECT_EQ("<img srcset=\"a.jpg, b.pagespeed.webp 100w, c.png 1000w\"/>",
            GetHtmlDomAsString());
}

}  // namespace net_instaweb
