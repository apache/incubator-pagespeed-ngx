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
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

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

class DomainRewriteFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    SetHtmlMimetype();  // Prevent insertion of CDATA tags to static JS.
    options()->Disallow("*dont_shard*");
    DomainLawyer* lawyer = options()->WriteableDomainLawyer();
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
  ExpectNoChange("", "");
  ExpectNoChange("relative", "relative.css");
  ExpectNoChange("absolute", "/absolute.css");
  ExpectNoChange("html domain", StrCat(kHtmlDomain, "absolute.css"));
  ExpectNoChange("other domain", StrCat(kOtherDomain, "absolute.css"));
  ExpectNoChange("disallow1", StrCat(kFrom1Domain, "dont_shard.css"));
  ExpectNoChange("disallow2", StrCat(kFrom2Domain, "dont_shard.css"));
  ExpectNoChange("http://absolute.css", "http://absolute.css");
  ExpectNoChange("data:image/gif;base64,AAAA", "data:image/gif;base64,AAAA");
}

TEST_F(DomainRewriteFilterTest, RelativeUpReferenceRewrite) {
  ExpectNoChange("subdir/relative", "under_subdir.css");
  ExpectNoChange("subdir/relative", "../under_top.css");

  AddRewriteDomainMapping(kTo1Domain, kHtmlDomain);
  ExpectChange("subdir/relative", "under_subdir.css",
               StrCat(kTo1Domain, "subdir/under_subdir.css"));
  ExpectChange("subdir/relative", "../under_top2.css",
               StrCat(kTo1Domain, "under_top2.css"));
}

TEST_F(DomainRewriteFilterTest, RelativeUpReferenceShard) {
  AddRewriteDomainMapping(kTo2Domain, kHtmlDomain);
  ExpectChange("subdir/relative", "under_subdir.css",
               StrCat(kTo2ADomain, "subdir/under_subdir.css"));
  ExpectChange("subdir/relative", "../under_top1.css",
               StrCat(kTo2BDomain, "under_top1.css"));
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
  ExpectNoChange("other domain",
                 Encode(kFrom1Domain, "cf", "0", "a.css", "css"));
}

TEST_F(DomainRewriteFilterTest, RewriteHyperlinks) {
  options()->ClearSignatureForTesting();
  options()->set_domain_rewrite_hyperlinks(true);
  ValidateExpected(
      "forms and a tags",
      StrCat("<a href=\"", kFrom1Domain, "link.html\"/>"
             "<form action=\"", kFrom1Domain, "blank\"/>"
             "<a href=\"https://from1.test.com/1.html\"/>"
             "<area href=\"", kFrom1Domain, "2.html\"/>"),
      "<a href=\"http://to1.test.com/link.html\"/>"
      "<form action=\"http://to1.test.com/blank\"/>"
      "<a href=\"https://from1.test.com/1.html\"/>"
      "<area href=\"http://to1.test.com/2.html\"/>");
}

TEST_F(DomainRewriteFilterTest, RewriteButDoNotShardHyperlinks) {
  options()->ClearSignatureForTesting();
  options()->set_domain_rewrite_hyperlinks(true);
  ValidateExpected(
      "forms and a tags",
      StrCat("<a href=\"", kFrom2Domain, "link.html\"/>"
             "<form action=\"", kFrom2Domain, "blank\"/>"
             "<a href=\"https://from2.test.com/1.html\"/>"
             "<area href=\"", kFrom2Domain, "2.html\"/>"),
      "<a href=\"http://to2.test.com/link.html\"/>"
      "<form action=\"http://to2.test.com/blank\"/>"
      "<a href=\"https://from2.test.com/1.html\"/>"
      "<area href=\"http://to2.test.com/2.html\"/>");
}

TEST_F(DomainRewriteFilterTest, RewriteRedirectLocations) {
  options()->ClearSignatureForTesting();
  options()->set_domain_rewrite_hyperlinks(true);
  ResponseHeaders headers;
  headers.Add(HttpAttributes::kLocation,
              StrCat(kFrom1Domain, "redirect"));
  rewrite_driver()->set_response_headers_ptr(&headers);

  ValidateNoChanges("headers", "");
  EXPECT_EQ(StrCat(kTo1Domain, "redirect"),
            headers.Lookup1(HttpAttributes::kLocation));
}

TEST_F(DomainRewriteFilterTest, NoClientDomainRewrite) {
  options()->ClearSignatureForTesting();
  options()->set_domain_rewrite_hyperlinks(true);
  options()->set_client_domain_rewrite(true);
  ValidateNoChanges("client domain rewrite", "<html><body></body></html>");
}

TEST_F(DomainRewriteFilterTest, ClientDomainRewrite) {
  options()->ClearSignatureForTesting();
  AddRewriteDomainMapping(kHtmlDomain, "http://clientrewrite.com/");
  options()->set_domain_rewrite_hyperlinks(true);
  options()->set_client_domain_rewrite(true);
  StringPiece client_domain_rewriter_code =
      server_context_->static_asset_manager()->GetAsset(
          StaticAssetManager::kClientDomainRewriter, options());

  SetupWriter();
  html_parse()->StartParse("http://test.com/");
  html_parse()->ParseText("<html><body>");
  html_parse()->Flush();
  html_parse()->ParseText("</body></html>");
  html_parse()->FinishParse();

  EXPECT_EQ(StrCat("<html><body>",
                   "<script type=\"text/javascript\">",
                   client_domain_rewriter_code,
                   "pagespeed.clientDomainRewriterInit("
                   "[\"http://clientrewrite.com/\"]);</script>",
                   "</body></html>"),
            output_buffer_);
}

TEST_F(DomainRewriteFilterTest, ProxySuffix) {
  options()->ClearSignatureForTesting();
  options()->set_domain_rewrite_hyperlinks(true);
  static const char kSuffix[] = ".suffix";
  static const char kOriginalHost[] = "www.example.com";
  GoogleString url(StrCat("http://", kOriginalHost, kSuffix, "/index.html"));
  GoogleUrl gurl(url);
  options()->WriteableDomainLawyer()->set_proxy_suffix(kSuffix);
  EXPECT_TRUE(options()->domain_lawyer()->can_rewrite_domains());

  // No need to change relative URLs -- they will be relative to the suffixed
  // domain as far as the browser is concerned.
  ValidateNoChanges("unchanged", "<a href='relative.html'>r</a>");

  // An absolute reference to a new destination in the origin domain gets
  // suffixed.
  ValidateExpectedUrl(url,
                      StrCat("<a href='http://", kOriginalHost,
                             "/absolute.html'>r</a>"),
                      StrCat("<a href='http://", kOriginalHost, kSuffix,
                             "/absolute.html'>r</a>"));

  // It also works even if the reference is a domain that's related to the
  // base, by consulting the known suffixes list via domain_registry.
  ValidateExpectedUrl(url,
                      "<a href='http://other.example.com/x.html'>r</a>",
                      "<a href='http://other.example.com.suffix/x.html'>r</a>");

  // However a link to a completely unrelated domain is left unchanged.
  ValidateExpectedUrl(url,
                      "<a href='http://other.com/x.html'>r</a>",
                      "<a href='http://other.com/x.html'>r</a>");
}

}  // namespace net_instaweb
