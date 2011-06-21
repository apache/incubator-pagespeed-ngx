/*
 * Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

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

class DomainRewriteFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    options()->Disallow("*dont_shard*");
    DomainLawyer* lawyer = options()->domain_lawyer();
    lawyer->AddRewriteDomainMapping(kTo1Domain, kFrom1Domain,
                                    &message_handler_);
    lawyer->AddRewriteDomainMapping(kTo2Domain, kFrom2Domain,
                                    &message_handler_);
    lawyer->AddShard(kTo2Domain, StrCat(kTo2ADomain, ",", kTo2BDomain),
                     &message_handler_);
    AddFilter(RewriteOptions::kRewriteDomains);
    domain_rewrites_ = statistics()->GetVariable("domain_rewrites");
    prev_num_rewrites_ = 0;
  }

  void ExpectNoChange(const char* tag, const StringPiece& url) {
    GoogleString orig = StrCat("<link rel=stylesheet href=", url, ">");
    ValidateNoChanges(tag, orig);
    EXPECT_EQ(0, DeltaRewrites());
  }

  void ExpectChange(const char* tag, const StringPiece& url,
                    const StringPiece& expected) {
    GoogleString orig = StrCat("<link rel=stylesheet href=", url, ">");
    GoogleString hacked = StrCat("<link rel=stylesheet href=", expected, ">");
    ValidateExpected(tag, orig, hacked);
    EXPECT_EQ(1, DeltaRewrites());
  }

  // Computes the number of domain rewrites done since the previous invocation
  // of DeltaRewrites.
  virtual int DeltaRewrites() {
    int num_rewrites = domain_rewrites_->Get();
    int delta = num_rewrites - prev_num_rewrites_;
    prev_num_rewrites_ = num_rewrites;
    return delta;
  }

  virtual bool AddBody() const { return false; }

 private:
  Variable* domain_rewrites_;
  int prev_num_rewrites_;
};

TEST_F(DomainRewriteFilterTest, DontTouch) {
  ExpectNoChange("relative", "relative.css");
  ExpectNoChange("absolute", "/absolute.css");
  ExpectNoChange("html domain", StrCat(kHtmlDomain, "absolute.css"));
  ExpectNoChange("other domain", StrCat(kOtherDomain, "absolute.css"));
  ExpectNoChange("disallow1", StrCat(kFrom1Domain, "dont_shard.css"));
  ExpectNoChange("disallow2", StrCat(kFrom2Domain, "dont_shard.css"));
}

TEST_F(DomainRewriteFilterTest, MappedAndSharded) {
  ExpectChange("rewrite", StrCat(kFrom1Domain, "absolute.css"),
               StrCat(kTo1Domain, "absolute.css"));
  ExpectChange("rewrite", StrCat(kFrom1Domain, "absolute.css?p1=v1"),
               StrCat(kTo1Domain, "absolute.css?p1=v1"));
  ExpectChange("shard0", StrCat(kFrom2Domain, "0.css"),
               StrCat(kTo2ADomain, "0.css"));
  ExpectChange("shard0", StrCat(kFrom2Domain, "0.css?p1=v1&amp;p2=v2"),
               StrCat(kTo2BDomain, "0.css?p1=v1&amp;p2=v2"));
}

TEST_F(DomainRewriteFilterTest, DontTouchIfAlreadyRewritten) {
  ExpectNoChange("other domain", StrCat(kFrom1Domain,
                                        "a.css.pagespeed.cf.0.css"));
}

}  // namespace net_instaweb
