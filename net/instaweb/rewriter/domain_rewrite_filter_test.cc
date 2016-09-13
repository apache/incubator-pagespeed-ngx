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

#include "net/instaweb/rewriter/public/domain_rewrite_filter.h"

#include <memory>

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
    add_html_tags_ = true;
  }

  void ExpectNoChange(const char* tag, StringPiece url) {
    GoogleString orig = StrCat("<link rel=stylesheet href=", url, ">");
    ValidateNoChanges(tag, orig);
    EXPECT_EQ(0, DeltaRewrites());
  }

  void ExpectChange(const char* tag, StringPiece url,
                    StringPiece expected) {
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
  virtual bool AddHtmlTags() const { return add_html_tags_; }

 protected:
  bool add_html_tags_;

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
  ValidateExpected("forms and a tags",
                   StrCat("<a href=\"", kFrom1Domain,
                          "link.html\"/>"
                          "<form action=\"",
                          kFrom1Domain,
                          "blank\"/>"
                          "<a href=\"https://from1.test.com/1.html\"/>"
                          "<area href=\"",
                          kFrom1Domain,
                          "2.html\"/>"
                          "<iframe src=\"",
                          kFrom1Domain,
                          "iframe.html\"/>"
                          "<iframe id=\"psmob-iframe\""
                          " src=\"",
                          kFrom1Domain, "iframe.html\"/>"),
                   "<a href=\"http://to1.test.com/link.html\"/>"
                   "<form action=\"http://to1.test.com/blank\"/>"
                   "<a href=\"https://from1.test.com/1.html\"/>"
                   "<area href=\"http://to1.test.com/2.html\"/>"
                   "<iframe src=\"http://to1.test.com/iframe.html\"/>"
                   "<iframe id=\"psmob-iframe\""
                   " src=\"http://to1.test.com/iframe.html\"/>");
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
          StaticAssetEnum::CLIENT_DOMAIN_REWRITER, options());

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

TEST_F(DomainRewriteFilterTest, ClientDomainRewriteDisabledDueToAmp) {
  options()->ClearSignatureForTesting();
  AddRewriteDomainMapping(kHtmlDomain, "http://clientrewrite.com/");
  options()->set_domain_rewrite_hyperlinks(true);
  options()->set_client_domain_rewrite(true);

  SetupWriter();
  html_parse()->StartParse("http://test.com/");
  html_parse()->ParseText("<html amp><body>");
  html_parse()->Flush();
  html_parse()->ParseText("</body></html>");
  html_parse()->FinishParse();

  EXPECT_EQ("<html amp><body></body></html>", output_buffer_);
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
  ValidateNoChanges(url, "<a href='http://other.com/x.html'>r</a>");

  ValidateExpectedUrl(url,
                      StrCat("<img src='http://", kOriginalHost,
                             "/image.png'>"),
                      StrCat("<img src='http://", kOriginalHost, kSuffix,
                             "/image.png'>"));

  ValidateExpectedUrl(url,
                      StrCat("<link rel=stylesheet href='http://",
                             kOriginalHost, "/style.css'>"),
                      StrCat("<link rel=stylesheet href='http://",
                             kOriginalHost, kSuffix, "/style.css'>"));

  ValidateExpectedUrl(url,
                      StrCat("<script src='http://", kOriginalHost,
                             "/script.js'></script>"),
                      StrCat("<script src='http://", kOriginalHost,
                             kSuffix, "/script.js'></script>"));

  ValidateNoChanges(url, "<img src='http://other.example/image.png'>");

  // An iframe does not get relocated.
  ValidateNoChanges(url,
                    StrCat("<iframe src='http://", kOriginalHost,
                           "/frame.html'></iframe>"));

  // Also test that we can fix up location: headers.
  ResponseHeaders headers;
  headers.Add(HttpAttributes::kLocation, "https://sub.example.com/a.html");
  DomainRewriteFilter::UpdateDomainHeaders(gurl, server_context(),
                                           options(), &headers);
  EXPECT_STREQ("https://sub.example.com.suffix/a.html",
               headers.Lookup1(HttpAttributes::kLocation));
}

TEST_F(DomainRewriteFilterTest, ProxyBaseUrl) {
  options()->ClearSignatureForTesting();
  options()->set_domain_rewrite_hyperlinks(true);
  static const char kSuffix[] = ".suffix";
  static const char kOriginalHost[] = "www.example.com";
  GoogleString origin_no_suffix(StrCat("http://", kOriginalHost));
  GoogleString origin_with_suffix(StrCat(origin_no_suffix, kSuffix));
  GoogleString url(StrCat(origin_with_suffix, "/index.html"));
  options()->WriteableDomainLawyer()->set_proxy_suffix(kSuffix);
  EXPECT_TRUE(options()->domain_lawyer()->can_rewrite_domains());

  add_html_tags_ = false;
  ValidateExpectedUrl(url,
                      StrCat("<html><head><base href='",
                             origin_no_suffix,
                             "'/></head></html>"),
                      StrCat("<html><head><base href='",
                             origin_with_suffix,
                             "/'/></head></html>"));

  ValidateNoChanges(url,
                    "<html><head><base href='http://other.example.com/'/>"
                    "</head></html>");

  ValidateNoChanges(url,
                    StrCat("<html><head><base href='",
                           origin_with_suffix,
                           "/'/></head></html>"));
}

TEST_F(DomainRewriteFilterTest, TestParseRefreshContent) {
  StringPiece before, url, after;
  EXPECT_FALSE(
      DomainRewriteFilter::ParseRefreshContent("42", &before, &url, &after));
  EXPECT_FALSE(
      DomainRewriteFilter::ParseRefreshContent("42 !", &before, &url, &after));
  EXPECT_FALSE(
      DomainRewriteFilter::ParseRefreshContent("42, ", &before, &url, &after));

  // Real-life, not spec behavior.
  EXPECT_TRUE(
      DomainRewriteFilter::ParseRefreshContent("42, rul",
                                               &before, &url, &after));
  EXPECT_EQ("42, ", before);
  EXPECT_EQ("rul", url);
  EXPECT_EQ("", after);

  // Real-life, not spec behavior.
  EXPECT_TRUE(
      DomainRewriteFilter::ParseRefreshContent("43, url",
                                               &before, &url, &after));
  EXPECT_EQ("43, ", before);
  EXPECT_EQ("url", url);
  EXPECT_EQ("", after);


  EXPECT_FALSE(
      DomainRewriteFilter::ParseRefreshContent("42, url = ",
                                               &before, &url, &after));

  EXPECT_TRUE(
      DomainRewriteFilter::ParseRefreshContent("44, url=a.org",
                                               &before, &url, &after));
  EXPECT_EQ("44, url=", before);
  EXPECT_EQ("a.org", url);
  EXPECT_EQ("", after);

  EXPECT_TRUE(
      DomainRewriteFilter::ParseRefreshContent(" 42.45; UrL = b.com  ",
                                               &before, &url, &after));
  EXPECT_EQ(" 42.45; UrL = ", before);
  EXPECT_EQ("b.com", url);
  EXPECT_EQ("", after);

  EXPECT_TRUE(
      DomainRewriteFilter::ParseRefreshContent(" 42 ; url='c.gov ' ",
                                               &before, &url, &after));
  EXPECT_EQ(" 42 ; url=", before);
  EXPECT_EQ("c.gov", url);
  EXPECT_EQ(" ", after);

  EXPECT_TRUE(
      DomainRewriteFilter::ParseRefreshContent(" 42 ; url='c.gov 'foo",
                                               &before, &url, &after));
  EXPECT_EQ(" 42 ; url=", before);
  EXPECT_EQ("c.gov", url);
  EXPECT_EQ("foo", after);

  EXPECT_TRUE(
      DomainRewriteFilter::ParseRefreshContent(" 42 ; url=\"d.edu ",
                                               &before, &url, &after));
  EXPECT_EQ(" 42 ; url=", before);
  EXPECT_EQ("d.edu", url);
  EXPECT_EQ("", after);
}

TEST_F(DomainRewriteFilterTest, TestParseSetCookieAttributes) {
  DomainRewriteFilter::SetCookieAttributes attrs;
  StringPiece cookie_string;

  DomainRewriteFilter::ParseSetCookieAttributes("", &cookie_string, &attrs);
  EXPECT_TRUE(attrs.empty());

  DomainRewriteFilter::ParseSetCookieAttributes("a=b", &cookie_string, &attrs);
  EXPECT_EQ("a=b", cookie_string);
  EXPECT_TRUE(attrs.empty());

  DomainRewriteFilter::ParseSetCookieAttributes("c=d;", &cookie_string, &attrs);
  EXPECT_EQ("c=d", cookie_string);
  EXPECT_TRUE(attrs.empty());

  DomainRewriteFilter::ParseSetCookieAttributes("e=f; foo = bar",
                                                &cookie_string, &attrs);
  EXPECT_EQ("e=f", cookie_string);
  ASSERT_EQ(1, attrs.size());
  EXPECT_EQ("foo", attrs[0].first);
  EXPECT_EQ("bar", attrs[0].second);

  DomainRewriteFilter::ParseSetCookieAttributes("g=h; foo = bar; httponly  ",
                                                &cookie_string, &attrs);
  EXPECT_EQ("g=h", cookie_string);
  ASSERT_EQ(2, attrs.size());
  EXPECT_EQ("foo", attrs[0].first);
  EXPECT_EQ("bar", attrs[0].second);
  EXPECT_EQ("httponly", attrs[1].first);
  EXPECT_EQ("", attrs[1].second);

  // No name really shouldn't happen, but test our robustness on it.
  DomainRewriteFilter::ParseSetCookieAttributes("i=j;  = bar; secure; a = b",
                                                &cookie_string, &attrs);
  EXPECT_EQ("i=j", cookie_string);
  ASSERT_EQ(3, attrs.size());
  EXPECT_EQ("", attrs[0].first);
  EXPECT_EQ("bar", attrs[0].second);
  EXPECT_EQ("secure", attrs[1].first);
  EXPECT_EQ("", attrs[1].second);
  EXPECT_EQ("a", attrs[2].first);
  EXPECT_EQ("b", attrs[2].second);
}

TEST_F(DomainRewriteFilterTest, TestUpdateSetCookieHeader) {
  GoogleString out;

  options()->ClearSignatureForTesting();
  options()->set_domain_rewrite_cookies(true);
  DomainLawyer* domain_lawyer = options()->WriteableDomainLawyer();
  domain_lawyer->Clear();
  domain_lawyer->AddRewriteDomainMapping(
      "http://someotherhost.com/after/", "www.example.com", &message_handler_);

  // For a bunch of tests, we test with page coming from a different domain
  // than the domain= lines. This will of course get rejected by the browser,
  // but it helps see that we're picking up the domain from the right place
  // when rewriting.
  GoogleUrl gurl_unrelated("http://unrelated.com");
  GoogleUrl gurl("http://www.example.com/page/");

  // No attributes.
  EXPECT_FALSE(
      DomainRewriteFilter::UpdateSetCookieHeader(
          gurl_unrelated, server_context(), options(), "foo = var", &out));

  // Non-domain attributes
  EXPECT_FALSE(
      DomainRewriteFilter::UpdateSetCookieHeader(
          gurl_unrelated, server_context(), options(),
          "foo = var; Secure; HttpOnly", &out));

  // Domain only.
  EXPECT_TRUE(
      DomainRewriteFilter::UpdateSetCookieHeader(
          gurl_unrelated, server_context(), options(),
          "foo = var; Domain=www.example.com", &out));
  EXPECT_EQ("foo = var; Domain=someotherhost.com", out);

  // Domain with the leading dot. Doesn't make any difference.
  EXPECT_TRUE(
      DomainRewriteFilter::UpdateSetCookieHeader(
          gurl_unrelated, server_context(), options(),
          "foo = var; Domain=.www.example.com", &out));
  EXPECT_EQ("foo = var; Domain=someotherhost.com", out);

  // Domain only + other stuff
  EXPECT_TRUE(
      DomainRewriteFilter::UpdateSetCookieHeader(
          gurl_unrelated, server_context(), options(),
          "foo = var; Domain=www.example.com;   HttpOnly", &out));
  EXPECT_EQ("foo = var; Domain=someotherhost.com; HttpOnly", out);

  // Multiple domain attributes. Last one wins, but we rewrite all.
  EXPECT_TRUE(
      DomainRewriteFilter::UpdateSetCookieHeader(
          gurl_unrelated, server_context(), options(),
          "foo = var; Domain=www.huh.com; Domain=www.example.com", &out));
  EXPECT_EQ("foo = var; Domain=someotherhost.com; Domain=someotherhost.com",
            out);

  // Multiple domain attributes. Last one wins, but is unrelated, so we don't
  // touch things.
  EXPECT_FALSE(DomainRewriteFilter::UpdateSetCookieHeader(
          gurl_unrelated, server_context(), options(),
          "foo = var; Domain=www.example.com; Domain=www.huh.com", &out));

  // Path only. We need a related URL here for mapping to apply.
  EXPECT_TRUE(
      DomainRewriteFilter::UpdateSetCookieHeader(
          gurl, server_context(), options(), "foo = var; Path=/subdir",
          &out));
  EXPECT_EQ("foo = var; Path=/after/subdir", out);

  // Path without starting slash --- ignored.
  EXPECT_FALSE(
      DomainRewriteFilter::UpdateSetCookieHeader(
          gurl, server_context(), options(), "foo = var; Path=subdir", &out));

  // Path + domain, related Domain=.
  EXPECT_TRUE(
      DomainRewriteFilter::UpdateSetCookieHeader(
          gurl, server_context(), options(),
          "foo = var; Domain=www.example.com; Path=/subdir/", &out));
  EXPECT_EQ("foo = var; Domain=someotherhost.com; Path=/after/subdir/", out);

  // Path + domain, unrelated Domain=.
  EXPECT_FALSE(
      DomainRewriteFilter::UpdateSetCookieHeader(
          gurl, server_context(), options(),
          "foo = var; Domain=unrelated.com; Path=/subdir/", &out));
}

TEST_F(DomainRewriteFilterTest, TestUpdateSetCookieHeaderDisabled) {
  GoogleUrl gurl("http://www.example.com/page/");
  // Make sure the off switch works.
  GoogleString out;

  options()->ClearSignatureForTesting();
  options()->set_domain_rewrite_cookies(true);
  DomainLawyer* domain_lawyer = options()->WriteableDomainLawyer();
  domain_lawyer->Clear();
  domain_lawyer->AddRewriteDomainMapping(
      "http://someotherhost.com/after/", "www.example.com", &message_handler_);
  EXPECT_FALSE(
      DomainRewriteFilter::UpdateSetCookieHeader(
          gurl, server_context(), options(),
          "foo = var; Domain=unrelated.com; Path=/subdir/", &out));
}

TEST_F(DomainRewriteFilterTest, ProxySuffixRefresh) {
  options()->ClearSignatureForTesting();
  options()->set_domain_rewrite_hyperlinks(true);
  static const char kSuffix[] = ".suffix";
  static const char kOriginalHost[] = "www.example.com";
  GoogleString origin_no_suffix(StrCat("http://", kOriginalHost));
  GoogleString origin_with_suffix(StrCat(origin_no_suffix, kSuffix));
  GoogleString url(StrCat(origin_with_suffix, "/index.html"));
  GoogleUrl gurl(url);
  options()->WriteableDomainLawyer()->set_proxy_suffix(kSuffix);
  EXPECT_TRUE(options()->domain_lawyer()->can_rewrite_domains());

  add_html_tags_ = false;
  ValidateExpectedUrl(url,
                      StrCat("<meta http-equiv=refresh content=\"5; url=",
                             origin_no_suffix, "\">"),
                      StrCat("<meta http-equiv=refresh content=\"5; url=",
                             "&quot;", origin_with_suffix, "/&quot;\">"));

  // Also test that we can fix it up in headers.
  ResponseHeaders headers;
  headers.Add(HttpAttributes::kRefresh, "42; https://sub.example.com/a.html");
  DomainRewriteFilter::UpdateDomainHeaders(gurl, server_context(),
                                           options(), &headers);
  EXPECT_STREQ("42; \"https://sub.example.com.suffix/a.html\"",
               headers.Lookup1(HttpAttributes::kRefresh));

  // Make sure we quote things correctly. This can't be a suffix, but requires
  // the more usual mapping.
  GoogleString after = "http://someotherhost.com/\"Subdir\"";
  options()->WriteableDomainLawyer()->Clear();
  options()->WriteableDomainLawyer()->AddRewriteDomainMapping(
      after, kOriginalHost, &message_handler_);

  headers.Replace(HttpAttributes::kRefresh,
                  StrCat("10; http://", kOriginalHost, "/a.html"));
  DomainRewriteFilter::UpdateDomainHeaders(gurl, server_context(),
                                           options(), &headers);
  EXPECT_STREQ("10; \"http://someotherhost.com/%22Subdir%22/a.html\"",
               headers.Lookup1(HttpAttributes::kRefresh));
}

TEST_F(DomainRewriteFilterTest, ProxySuffixSetCookie) {
options()->ClearSignatureForTesting();
  options()->set_domain_rewrite_hyperlinks(true);
  options()->set_domain_rewrite_cookies(true);
  static const char kSuffix[] = ".suffix";
  static const char kOriginalHost[] = "www.example.com";
  GoogleString origin_no_suffix(StrCat("http://", kOriginalHost));
  GoogleString origin_with_suffix(StrCat(origin_no_suffix, kSuffix));
  GoogleString url(StrCat(origin_with_suffix, "/index.html"));
  GoogleUrl gurl(url);
  options()->WriteableDomainLawyer()->set_proxy_suffix(kSuffix);
  EXPECT_TRUE(options()->domain_lawyer()->can_rewrite_domains());

  add_html_tags_ = false;
  ValidateExpectedUrl(url,
                      StrCat("<meta http-equiv=set-cookie content=\"a=b; ",
                             "domain= ", kOriginalHost, "\">"),
                      StrCat("<meta http-equiv=set-cookie content=\"a=b; ",
                             "domain=", kOriginalHost, kSuffix, "\">"));

  // Now test with multiple HTTP headers, to make sure all are fixed.
  ResponseHeaders headers;
  headers.Add(HttpAttributes::kSetCookie,
              StrCat("a=b; Domain=", kOriginalHost));
  headers.Add(HttpAttributes::kSetCookie,
              StrCat("c=d; Secure; Domain=", kOriginalHost));
  DomainRewriteFilter::UpdateDomainHeaders(gurl, server_context(),
                                           options(), &headers);
  DomainRewriteFilter::UpdateDomainHeaders(gurl, server_context(),
                                           options(), &headers);

  ConstStringStarVector vals;
  ASSERT_TRUE(headers.Lookup(HttpAttributes::kSetCookie, &vals));
  ASSERT_EQ(2, vals.size());
  EXPECT_EQ(StrCat("a=b; Domain=", kOriginalHost, kSuffix),
            *vals[0]);
  EXPECT_EQ(StrCat("c=d; Secure; Domain=", kOriginalHost, kSuffix),
            *vals[1]);
}

}  // namespace net_instaweb
