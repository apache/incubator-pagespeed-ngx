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

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the string-splitter.

#include "pagespeed/kernel/http/google_url.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace {

const char kUrl[] = "http://a.com/b/c/d.ext?f=g/h";
const char kUrlWithPort[] = "http://a.com:8080/b/c/d.ext?f=g/h";
const char kBadQueryString[] =
    "\a\b\t\n\v\f\r !\"$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLM"
    "NOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\177#extra#more";

}  // namespace

namespace net_instaweb {

class GoogleUrlTest : public testing::Test {
 protected:
  GoogleUrlTest()
  : gurl_(kUrl),
    gurl_with_port_(kUrlWithPort)
  {}

  void TestCopyAndAddEscapedQueryParamCase(const char* before,
                                           const char* after) {
    GoogleUrl before_url(before);
    StringPiece before_url_original(before_url.UncheckedSpec());
    scoped_ptr<GoogleUrl> after_url(
        before_url.CopyAndAddEscapedQueryParam("r", "s"));
    EXPECT_EQ(after_url->UncheckedSpec(), after);
    EXPECT_TRUE(after_url->IsWebValid());
    EXPECT_EQ(before_url_original, before_url.UncheckedSpec());
  }

  void TestAllExceptQueryCase(const char* before, const char* after) {
    GoogleUrl before_url(before);
    EXPECT_EQ(before_url.AllExceptQuery(), GoogleString(after));
  }

  void TestAllAfterQueryCase(const char* before, const char* after) {
    GoogleUrl before_url(before);
    EXPECT_EQ(before_url.AllAfterQuery(), GoogleString(after));
  }

  // RunMostMethods and RunAllMethods parse a url and run methods to make
  // sure they don't crash.

  // None of these methods should CHECK-crash if a bad URL is passed in.
  // They will LOG(DFATAL)-crash in debug mode, though, so you should use
  // EXPECT_DFATAL(RunMostMethods("..."), "");
  void RunMostMethods(const StringPiece& url_string) {
    GoogleUrl url(url_string);
    url.AllExceptQuery();
    url.AllAfterQuery();
    url.AllExceptLeaf();
    url.LeafWithQuery();
    url.LeafSansQuery();
    url.PathAndLeaf();
    url.PathSansLeaf();
    url.PathSansQuery();
    url.ExtractFileName();
    url.Host();
    url.HostAndPort();
    url.Origin();
    url.Query();
    url.Scheme();
    url.UncheckedSpec();
    url.IntPort();
    url.EffectiveIntPort();
  }

  // Runs all methods, even ones that will CHECK-crash on invalid URLs.
  void RunAllMethods(const StringPiece& url_string) {
    RunMostMethods(url_string);

    GoogleUrl url(url_string);
    ASSERT_TRUE(url.IsAnyValid()) << url_string << " is invalid";
    url.IsWebValid();
    url.IsWebOrDataValid();
    url.Spec();
    url.spec_c_str();

    url.Relativize(kAbsoluteUrl, url);
    url.Relativize(kNetPath, url);
    url.Relativize(kAbsolutePath, url);
    url.Relativize(kRelativePath, url);
  }

  void TestEscapeUnescape(StringPiece value) {
    EXPECT_STREQ(value, GoogleUrl::Unescape(GoogleUrl::Escape(value)));
  }

  GoogleUrl gurl_;
  GoogleUrl gurl_with_port_;
};

// Document which sorts of strings are and are not valid.
TEST_F(GoogleUrlTest, TestNotValid) {
  GoogleUrl empty_url;
  EXPECT_FALSE(empty_url.IsWebValid());
  EXPECT_FALSE(empty_url.IsWebOrDataValid());
  EXPECT_FALSE(empty_url.IsAnyValid());

  GoogleUrl invalid_url("Hello, world!");
  EXPECT_FALSE(invalid_url.IsWebValid());

  GoogleUrl relative_url1("/foo/bar.html");
  EXPECT_FALSE(relative_url1.IsWebValid());

  GoogleUrl relative_url2("foo/bar.html");
  EXPECT_FALSE(relative_url2.IsWebValid());

  GoogleUrl relative_url3("bar.html");
  EXPECT_FALSE(relative_url3.IsWebValid());

  // Only http: and https: are considered WebValid.
  GoogleUrl proxy_filename("proxy:http://www.example.com/index.html");
  EXPECT_FALSE(proxy_filename.IsWebValid());
  EXPECT_FALSE(proxy_filename.IsWebOrDataValid());
  // But it's still considered to be a valid URL, oddly enough.
  EXPECT_TRUE(proxy_filename.IsAnyValid());

  GoogleUrl data_url("data:plain/text,foobar");
  EXPECT_FALSE(data_url.IsWebValid());
  EXPECT_TRUE(data_url.IsWebOrDataValid());
  EXPECT_TRUE(data_url.IsAnyValid());
}

TEST_F(GoogleUrlTest, TestSpec) {
  EXPECT_STREQ(kUrl, gurl_.Spec());
  EXPECT_STREQ("http://a.com/b/c/", gurl_.AllExceptLeaf());
  EXPECT_STREQ("d.ext?f=g/h", gurl_.LeafWithQuery());
  EXPECT_STREQ("d.ext", gurl_.LeafSansQuery());
  EXPECT_STREQ("http://a.com", gurl_.Origin());
  EXPECT_STREQ("/b/c/d.ext?f=g/h", gurl_.PathAndLeaf());
  EXPECT_STREQ("/b/c/d.ext", gurl_.PathSansQuery());
}


TEST_F(GoogleUrlTest, TestSpecWithPort) {
  EXPECT_STREQ(kUrlWithPort, gurl_with_port_.Spec());
  EXPECT_STREQ("http://a.com:8080/b/c/", gurl_with_port_.AllExceptLeaf());
  EXPECT_STREQ("d.ext?f=g/h", gurl_with_port_.LeafWithQuery());
  EXPECT_STREQ("d.ext", gurl_with_port_.LeafSansQuery());
  EXPECT_STREQ("http://a.com:8080", gurl_with_port_.Origin());
  EXPECT_STREQ("/b/c/d.ext?f=g/h", gurl_with_port_.PathAndLeaf());
  EXPECT_STREQ("/b/c/d.ext", gurl_.PathSansQuery());
  EXPECT_STREQ("/b/c/", gurl_.PathSansLeaf());
}

TEST_F(GoogleUrlTest, TestDots) {
  GoogleUrl url1("http://www.example.com/foo/../bar/baz/./../index.html");
  EXPECT_EQ("http://www.example.com/bar/index.html", url1.Spec());

  GoogleUrl url2("http://www.example.com/../../../../..");
  EXPECT_EQ("http://www.example.com/", url2.Spec());

  GoogleUrl url3("http://www.example.com/foo/./bar/index.html");
  EXPECT_EQ("http://www.example.com/foo/bar/index.html", url3.Spec());
}

// Note: These tests are basically gold tests done for documentation more
// than expectation. If they change that should be fine, we just want to
// know what is decoded and what is not.
TEST_F(GoogleUrlTest, TestDecode) {
  // Not "%20" -> " "
  GoogleUrl url1("http://www.example.com/foo%20bar.html");
  EXPECT_EQ("http://www.example.com/foo%20bar.html", url1.Spec());
  // "%2E" -> "."
  GoogleUrl url2("http://www.example.com/foo/%2E%2E/bar.html");
  EXPECT_EQ("http://www.example.com/bar.html", url2.Spec());
  GoogleUrl url2b("http://www.example.com/foo/%2e%2e/bar.html");
  EXPECT_EQ("http://www.example.com/bar.html", url2b.Spec());
  GoogleUrl url2c("http://www.example.com/%2e%2e/%2e%2e/some/file/path/");
  EXPECT_EQ("http://www.example.com/some/file/path/", url2c.Spec());
  // Not "%2F" -> "/"
  GoogleUrl url3("http://www.example.com/foo%2Fbar.html");
  EXPECT_EQ("http://www.example.com/foo%2Fbar.html", url3.Spec());
  GoogleUrl url3b("http://www.example.com/foo%2fbar.html");
  EXPECT_EQ("http://www.example.com/foo%2fbar.html", url3b.Spec());
  // Other
  GoogleUrl url4("http://www.example.com/%53%2D%38%25%32%35");
  EXPECT_EQ("http://www.example.com/S-8%2525", url4.Spec());
}

TEST_F(GoogleUrlTest, TestCopyAndAddEscapedQueryParam) {
  TestCopyAndAddEscapedQueryParamCase("http://a.com/b/c/d.ext",
                                      "http://a.com/b/c/d.ext?r=s");

  TestCopyAndAddEscapedQueryParamCase("http://a.com/b/c/d.ext?p=q",
                                      "http://a.com/b/c/d.ext?p=q&r=s");

  TestCopyAndAddEscapedQueryParamCase("http://a.com",
                                      "http://a.com/?r=s");

  TestCopyAndAddEscapedQueryParamCase("http://a.com?p=q",
                                      "http://a.com/?p=q&r=s");

  TestCopyAndAddEscapedQueryParamCase("http://a.com/b/c/d.ext?p=q#ref",
                                      "http://a.com/b/c/d.ext?p=q&r=s#ref");
}

TEST_F(GoogleUrlTest, TestAllExceptQuery) {
  TestAllExceptQueryCase("http://a.com/b/c/d.ext",
                         "http://a.com/b/c/d.ext");

  TestAllExceptQueryCase("http://a.com/b/c/d.ext?p=p&q=q",
                         "http://a.com/b/c/d.ext");

  TestAllExceptQueryCase("http://a.com?p=p&q=q",
                         "http://a.com/");
}

TEST_F(GoogleUrlTest, TestAllAfterQuery) {
  TestAllAfterQueryCase("http://a.com/b/c/d.ext",
                        "");

  TestAllAfterQueryCase("http://a.com/b/c/d.ext?p=p&q=q",
                        "");

  TestAllAfterQueryCase("http://a.com/b/c/d.ext?p=p&q=q#ref",
                        "#ref");

  TestAllAfterQueryCase("http://a.com/b/c/d.ext?p=p&q=q#ref1#ref2",
                        "#ref1#ref2");

  TestAllAfterQueryCase("http://a.com#ref",
                        "#ref");
}

TEST_F(GoogleUrlTest, TestTrivialAllExceptLeaf) {
  GoogleUrl queryless("http://a.com/b/c/d.ext");
  EXPECT_STREQ("http://a.com/b/c/", queryless.AllExceptLeaf());
  GoogleUrl queryful("http://a.com/b/c/d.ext?p=p&q=q");
  EXPECT_STREQ("http://a.com/b/c/", queryful.AllExceptLeaf());
}

TEST_F(GoogleUrlTest, TestAllExceptLeafIsIdempotent) {
  // In various places the code takes either a full URL or the URL's
  // AllExceptLeaf and depends on the fact that calling AllExceptLeaf on either
  // gives you the same thing. This test is catch any breakage to that.
  GoogleUrl queryless("http://a.com/b/c/d.ext");
  StringPiece all_except_leaf(queryless.AllExceptLeaf());
  GoogleUrl all_except_leaf_url(all_except_leaf);
  EXPECT_TRUE(all_except_leaf_url.IsWebValid());
  EXPECT_EQ(all_except_leaf, all_except_leaf_url.AllExceptLeaf());
}

TEST_F(GoogleUrlTest, TestTrivialLeafSansQuery) {
  GoogleUrl queryless("http://a.com/b/c/d.ext");
  EXPECT_STREQ("d.ext", queryless.LeafSansQuery());
}

TEST_F(GoogleUrlTest, ResolveRelative) {
  GoogleUrl base(StringPiece("http://www.google.com"));
  ASSERT_TRUE(base.IsWebValid());
  GoogleUrl resolved(base, "test.html");
  ASSERT_TRUE(resolved.IsWebValid());
  EXPECT_STREQ("http://www.google.com/test.html", resolved.Spec());
  EXPECT_STREQ("/test.html", resolved.PathSansQuery());
}

TEST_F(GoogleUrlTest, ResolveAbsolute) {
  GoogleUrl base(StringPiece("http://www.google.com"));
  ASSERT_TRUE(base.IsWebValid());
  GoogleUrl resolved(base, "http://www.google.com");
  ASSERT_TRUE(resolved.IsWebValid());
  EXPECT_STREQ("http://www.google.com/", resolved.Spec());
  EXPECT_STREQ("/", resolved.PathSansQuery());
}

TEST_F(GoogleUrlTest, TestReset) {
  GoogleUrl url("http://www.google.com");
  EXPECT_TRUE(url.IsWebValid());
  url.Clear();
  EXPECT_FALSE(url.IsWebValid());
  EXPECT_TRUE(url.is_empty());
  EXPECT_TRUE(url.UncheckedSpec().empty());
}

TEST_F(GoogleUrlTest, TestHostAndPort) {
  const char kExpected5[] = "example.com:5";
  EXPECT_EQ(kExpected5, GoogleUrl("http://example.com:5").HostAndPort());
  EXPECT_EQ(kExpected5, GoogleUrl("http://example.com:5/a/b").HostAndPort());
  EXPECT_EQ(kExpected5, GoogleUrl("https://example.com:5").HostAndPort());
  const char kExpected[] = "example.com";
  EXPECT_EQ(kExpected, GoogleUrl("http://example.com").HostAndPort());
  EXPECT_EQ(kExpected, GoogleUrl("http://example.com/a/b").HostAndPort());
  EXPECT_EQ(kExpected, GoogleUrl("https://example.com").HostAndPort());
}

TEST_F(GoogleUrlTest, TestPort) {
  EXPECT_EQ(5, GoogleUrl("http://example.com:5").IntPort());
  EXPECT_EQ(5, GoogleUrl("http://example.com:5").EffectiveIntPort());
  EXPECT_EQ(5, GoogleUrl("https://example.com:5").IntPort());
  EXPECT_EQ(5, GoogleUrl("https://example.com:5").EffectiveIntPort());
  EXPECT_EQ(-1, GoogleUrl("http://example.com").IntPort());
  EXPECT_EQ(80, GoogleUrl("http://example.com").EffectiveIntPort());
  EXPECT_EQ(-1, GoogleUrl("https://example.com").IntPort());
  EXPECT_EQ(443, GoogleUrl("https://example.com").EffectiveIntPort());
}

TEST_F(GoogleUrlTest, TestExtraSlash) {
  // We always preserve // in URLs.
  GoogleUrl no_base("http://example.com//extra_slash/index.html");
  EXPECT_STREQ("http://example.com//extra_slash/index.html", no_base.Spec());

  GoogleUrl base("http://www.example.com");
  GoogleUrl early_slashes(base, "http://a.com//extra_slash/index.html");
  EXPECT_STREQ("http://a.com//extra_slash/index.html", early_slashes.Spec());

  GoogleUrl late_slashes(base, "http://a.com/extra_slash//index.html");
  EXPECT_STREQ("http://a.com/extra_slash//index.html", late_slashes.Spec());

  GoogleUrl three_slashes(base, "http://a.com///extra_slash/index.html");
  EXPECT_STREQ("http://a.com///extra_slash/index.html", three_slashes.Spec());
}

TEST_F(GoogleUrlTest, SchemeRelativeBase) {
  GoogleUrl base("http://www.example.com");
  GoogleUrl resolved(base, "//other.com/file.ext");
  ASSERT_TRUE(resolved.IsWebValid());
  EXPECT_STREQ("http://other.com/file.ext", resolved.Spec());
}

TEST_F(GoogleUrlTest, SchemeRelativeHttpsBase) {
  GoogleUrl base("https://www.example.com");
  GoogleUrl resolved(base, "//other.com/file.ext");
  ASSERT_TRUE(resolved.IsWebValid());
  EXPECT_STREQ("https://other.com/file.ext", resolved.Spec());
}

TEST_F(GoogleUrlTest, SchemeRelativeNoBase) {
  GoogleUrl gurl("//other.com/file.ext");
  EXPECT_FALSE(gurl.IsWebValid());
}

TEST_F(GoogleUrlTest, FindRelativity) {
  EXPECT_EQ(kAbsoluteUrl, GoogleUrl::FindRelativity(
      "http://example.com/foo/bar/file.ext?k=v#f"));
  EXPECT_EQ(kNetPath, GoogleUrl::FindRelativity(
      "//example.com/foo/bar/file.ext?k=v#f"));
  EXPECT_EQ(kAbsolutePath, GoogleUrl::FindRelativity(
      "/foo/bar/file.ext?k=v#f"));
  EXPECT_EQ(kRelativePath, GoogleUrl::FindRelativity(
      "bar/file.ext?k=v#f"));
}

TEST_F(GoogleUrlTest, Relativize) {
  GoogleUrl url("http://example.com/foo/bar/file.ext?k=v#f");
  GoogleUrl good_base_url("http://example.com/foo/index.html");

  EXPECT_EQ("http://example.com/foo/bar/file.ext?k=v#f",
            url.Relativize(kAbsoluteUrl, good_base_url));
  EXPECT_EQ("//example.com/foo/bar/file.ext?k=v#f",
            url.Relativize(kNetPath, good_base_url));
  EXPECT_EQ("/foo/bar/file.ext?k=v#f",
            url.Relativize(kAbsolutePath, good_base_url));
  EXPECT_EQ("bar/file.ext?k=v#f",
            url.Relativize(kRelativePath, good_base_url));

  GoogleUrl bad_base_url("https://www.example.com/other/path.html");
  EXPECT_EQ("http://example.com/foo/bar/file.ext?k=v#f",
            url.Relativize(kAbsoluteUrl, bad_base_url));
  EXPECT_EQ("http://example.com/foo/bar/file.ext?k=v#f",
            url.Relativize(kNetPath, bad_base_url));
  EXPECT_EQ("http://example.com/foo/bar/file.ext?k=v#f",
            url.Relativize(kAbsolutePath, bad_base_url));
  EXPECT_EQ("http://example.com/foo/bar/file.ext?k=v#f",
            url.Relativize(kRelativePath, bad_base_url));

  GoogleUrl double_slash("http://example.com//index.html");
  GoogleUrl double_base("http://example.com/");
  EXPECT_EQ("http://example.com//index.html",
            double_slash.Relativize(kAbsoluteUrl, double_base));
  // Safe to use net path.
  EXPECT_EQ("//example.com//index.html",
            double_slash.Relativize(kNetPath, double_base));
  // We cannot shorten to "//index.html", that looks like a net path.
  // Perhaps we could shorten to "/.//index.html" instead.
  EXPECT_EQ("http://example.com//index.html",
            double_slash.Relativize(kAbsolutePath, double_base));
  // We cannot shorten to "/index.html", that looks like an absolute path.
  // Perhaps we could shorten to ".//index.html" instead.
  EXPECT_EQ("http://example.com//index.html",
            double_slash.Relativize(kRelativePath, double_base));

  GoogleUrl query_url("http://example.com/?bar");
  GoogleUrl query_base("http://example.com/foo.html");
  // We cannot shorten to "?bar", because that would refer to foo.html?bar
  EXPECT_EQ("http://example.com/?bar",
            query_url.Relativize(kRelativePath, query_base));

  GoogleUrl fragment_url("http://example.com/#bar");
  GoogleUrl fragment_base("http://example.com/foo.html");
  // We cannot shorten to "#bar", because that would refer to foo.html#bar
  EXPECT_EQ("http://example.com/#bar",
            fragment_url.Relativize(kRelativePath, fragment_base));

  GoogleUrl perverse_url("http://example.com/http://otherdomain.com/");
  GoogleUrl perverse_base("http://example.com/");
  // We cannot shorten to "http://otherdomain.com/" ... obviously.
  EXPECT_EQ("http://example.com/http://otherdomain.com/",
            perverse_url.Relativize(kRelativePath, perverse_base));

  GoogleUrl no_slash_base("http://example.com");
  GoogleUrl no_slash_url("http://example.com/index.html");
  // Make sure we don't strip this to "/index.html".
  EXPECT_EQ("index.html",
            no_slash_url.Relativize(kRelativePath, no_slash_base));
}

TEST_F(GoogleUrlTest, RelativeUrls) {
  const StringPiece base_urls[] = {
    "http://example.com/",
    "https://example.com/foo/bar/file.ext?k=v#f",
    "file:///dir/sub/foo.html",
    "file://",
  };

  // URLs which will be reproduced by Relativize().
  const StringPiece reproducible_urls[] = {
    "http://example.com/", "/", "/foo.html", "foo.html", "dir/foo.html",
    "//example.com/foo.html",
  };

  for (int i = 0; i < arraysize(reproducible_urls); ++i) {
    UrlRelativity url_relativity =
        GoogleUrl::FindRelativity(reproducible_urls[i]);

    for (int j = 0; j < arraysize(base_urls); ++j) {
      GoogleUrl base(base_urls[j]);
      GoogleUrl url(base, reproducible_urls[i]);
      EXPECT_EQ(reproducible_urls[i], url.Relativize(url_relativity, base));
    }
  }

  // URLs which will change after Relativize().
  // Format: { (original url), (after relativized w/ base_urls[0]),
  //           (after relativized w/ base_urls[1]), ... }
  const StringPiece non_reproducible_urls[][arraysize(base_urls) + 1] = {
    { "../file.html",
      "file.html", "https://example.com/foo/file.html",
      "file:///dir/file.html", "file.html", },
    { "../../file.html",
      "file.html", "https://example.com/file.html",
      "file:///file.html", "file.html", },
    { "./file.html",
      "file.html", "file.html", "file.html", "file.html", },
    { "../bar/file.html",
      "bar/file.html", "file.html",
      "file:///dir/bar/file.html", "bar/file.html", },
    { "",
      "", "file.ext?k=v", "foo.html", "", },
    { "?a=b",
      "?a=b", "file.ext?a=b", "foo.html?a=b", "?a=b", },
    { "#f2",
      "#f2", "file.ext?k=v#f2", "foo.html#f2", "#f2", },
    { ".",
      "", "https://example.com/foo/bar/", "file:///dir/sub/", "", },
  };
  for (int i = 0; i < arraysize(non_reproducible_urls); ++i) {
    StringPiece in_url = non_reproducible_urls[i][0];
    UrlRelativity url_relativity = GoogleUrl::FindRelativity(in_url);

    for (int j = 0; j < arraysize(base_urls); ++j) {
      GoogleUrl base(base_urls[j]);
      GoogleUrl url(base, in_url);

      StringPiece expected_url = non_reproducible_urls[i][j+1];
      EXPECT_EQ(expected_url, url.Relativize(url_relativity, base))
          << in_url << " " << base_urls[j];
    }
  }
}

// Make sure weird URLs don't crash our system.
TEST_F(GoogleUrlTest, TestNoCrash) {
  RunAllMethods("http://www.example.com/");
  RunAllMethods("http:foo.css");
  RunAllMethods("data:");
  RunAllMethods("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAA"
                "FCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljN"
                "BAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");
  RunAllMethods("https://secure.example.org/foo/bar.html?blah=t#frag");
  RunAllMethods("file:");
  RunAllMethods("file:foobar");
  RunAllMethods("file:foo/bar/baz.rtf");
  RunAllMethods("file:///var/log/");
  RunAllMethods("ftp://ftp.example.com/");

}

TEST_F(GoogleUrlTest, Query) {
  // First try very simple names and values.
  GoogleUrl gurl("http://example.com/a?b=c&d=e");
  ASSERT_TRUE(gurl.IsWebValid());
  EXPECT_STREQ("b=c&d=e", gurl.Query());

  // Now use a URL that will require escaping.
  gurl.Reset("http://example.com/a?b=<value requiring escapes>");
  ASSERT_TRUE(gurl.IsWebValid());
  EXPECT_STREQ("b=%3Cvalue%20requiring%20escapes%3E", gurl.Query());
  EXPECT_STREQ("b=<value requiring escapes>",
               GoogleUrl::Unescape(gurl.Query()));
}

TEST_F(GoogleUrlTest, URLQuery) {
  // Test that the result of Query() is encoded as we expect (and rely on in
  // QueryParams), so that we know if our assumptions become invalid:
  // 1. HASHes ('#') cannot be in the result as that terminates the component.
  // 2. TABs, NLs, & CRs are removed completely.
  // 3. Control characters are % encoded.
  // 4. SPACE (' '), DOUBLE QUOTE ('"'), LESS THAN ('<'), GREATER THAN ('>'),
  //    and DEL ('\177') are % encoded.
  // 5. In the open source build, which uses chromium's version of the URL
  //    libraries, single-quote ("'") is also %-encoded.
  // 6. HASH ('#') would be encoded but it cannot be in the result per 1 above.
  // The code that does all this is:
  // * ParsePath in url_parse.cc extracts the query part, starting at the
  //   first '?' and ending at e.o.string or the first '#'.
  // * url_canon::RemoveURLWhitespace strips tabs, newlines, & carriage returns
  //   per the url_canon::IsRemovableURLWhitespace method.
  // * url_canon::IsQueryChar results in the encoding of control characters,
  //   the characters in [ "#<>], & DEL (& "'" in open source), however since
  //   '#' terminates the query part it cannot be in the result.
  GoogleString good_query_param1(kBadQueryString);
  ASSERT_EQ(1, GlobalReplaceSubstring("#more",   "", &good_query_param1));
  ASSERT_EQ(1, GlobalReplaceSubstring("#extra",  "", &good_query_param1));
  ASSERT_EQ(1, GlobalReplaceSubstring("\t",      "", &good_query_param1));
  ASSERT_EQ(1, GlobalReplaceSubstring("\n",      "", &good_query_param1));
  ASSERT_EQ(1, GlobalReplaceSubstring("\r",      "", &good_query_param1));
  ASSERT_EQ(1, GlobalReplaceSubstring("\a",   "%07", &good_query_param1));
  ASSERT_EQ(1, GlobalReplaceSubstring("\b",   "%08", &good_query_param1));
  ASSERT_EQ(1, GlobalReplaceSubstring("\v",   "%0B", &good_query_param1));
  ASSERT_EQ(1, GlobalReplaceSubstring("\f",   "%0C", &good_query_param1));
  ASSERT_EQ(1, GlobalReplaceSubstring(" ",    "%20", &good_query_param1));
  ASSERT_EQ(1, GlobalReplaceSubstring("\"",   "%22", &good_query_param1));
  ASSERT_EQ(1, GlobalReplaceSubstring("<",    "%3C", &good_query_param1));
  ASSERT_EQ(1, GlobalReplaceSubstring(">",    "%3E", &good_query_param1));
  ASSERT_EQ(1, GlobalReplaceSubstring("\177", "%7F", &good_query_param1));
  GoogleString good_query_param2 = good_query_param1;
  ASSERT_EQ(1, GlobalReplaceSubstring("'",    "%27", &good_query_param2));

  // Despite all the ugliness in the query parameter, it's [now] a valid URL.
  GoogleUrl gurl(StrCat("http://example.com/?", kBadQueryString));
  ASSERT_TRUE(gurl.IsAnyValid());
  ASSERT_TRUE(!gurl.Query().empty());

  if (gurl.Query() != good_query_param1 && gurl.Query() != good_query_param2) {
    EXPECT_TRUE(false)
        << "gurl.Query() does not equal either of the expected values:\n"
        << "  Actual: " << gurl.Query() << "\n"
        << "Expected: " << good_query_param1 << "\n"
        << "      Or: " << good_query_param2;
  }
}

TEST_F(GoogleUrlTest, Unescape) {
  EXPECT_STREQ("", GoogleUrl::Unescape(""));
  EXPECT_STREQ("noescaping", GoogleUrl::Unescape("noescaping"));
  EXPECT_STREQ("http://example.com:8080/src/example.html?a=b&a=c,d",
               GoogleUrl::Unescape(
                   "http%3A%2f%2Fexample.com%3A8080%2Fsrc%2Fexample.html"
                   "%3Fa%3Db%26a%3dc%2Cd"));
  EXPECT_STREQ("%:%1z%zZ%a%", GoogleUrl::Unescape("%%3a%1z%zZ%a%"));
}

TEST_F(GoogleUrlTest, Escape) {
  EXPECT_STREQ("Hello1234-5678_910~", GoogleUrl::Escape("Hello1234-5678_910~"));

  // Note, even commas are escaped :(.
  EXPECT_STREQ("Hello%2c+World%21", GoogleUrl::Escape("Hello, World!"));

  TestEscapeUnescape("Hello, World!");
  TestEscapeUnescape("Hello1234-5678_910~");
  TestEscapeUnescape("noescaping");
  TestEscapeUnescape("http://example.com:8080/src/example.html?a=b&a=c,d");
  TestEscapeUnescape("%:%1z%zZ%a%");
  // Ensure that we encode/decode everything all characters.
  TestEscapeUnescape(kBadQueryString);
  // Ensure that we correctly re-encode/decode an already-encoded string.
  TestEscapeUnescape(GoogleUrl::Escape(kBadQueryString));
}

}  // namespace net_instaweb
