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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/file_load_policy.h"

#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class FileLoadPolicyTest : public ::testing::Test {
 protected:
  // Generally use this for URLs you don't expect to be loaded from files.
  // e.g. EXPECT_FALSE(TryLoadFromFile("http://www.example.com/"));
  bool TryLoadFromFile(const StringPiece& url_string) {
    return TryLoadFromFile(url_string, &policy_);
  }

  bool TryLoadFromFile(const StringPiece& url_string, FileLoadPolicy* policy) {
    GoogleUrl url(url_string);
    GoogleString filename;
    return policy->ShouldLoadFromFile(url, &filename);
  }

  // Generally use this for URLs you do expect to be loaded from files.
  // e.g. EXPECT_EQ("filename", LoadFromFile("url"));
  GoogleString LoadFromFile(const StringPiece& url_string) {
    return LoadFromFile(url_string, &policy_);
  }

  GoogleString LoadFromFile(const StringPiece& url_string,
                            FileLoadPolicy* policy) {
    GoogleUrl url(url_string);
    GoogleString filename;
    bool load = policy->ShouldLoadFromFile(url, &filename);
    if (!load) {
      EXPECT_TRUE(filename.empty());
    }
    return filename;
  }

  FileLoadPolicy policy_;
};

TEST_F(FileLoadPolicyTest, EmptyPolicy) {
  GoogleString filename;

  // Empty policy. Don't map anything.
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/static/foo.png"));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/static/bar/"));
  EXPECT_FALSE(TryLoadFromFile(
      "http://www.example.com/static/some/more/dirs/b.css"));
  EXPECT_FALSE(TryLoadFromFile(
      "http://www.example.com/static/foo.png?version=3.1"));
  EXPECT_FALSE(TryLoadFromFile(
      "http://www.example.com/static/foo.png?a?b#/c?foo"));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/static/foo%20bar.png"));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/static/foo%2Fbar.png"));

  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/images/another.gif"));
  EXPECT_FALSE(TryLoadFromFile(
      "http://www.some-site.com/with/many/dirs/a/b.js"));

  EXPECT_FALSE(TryLoadFromFile("http://www.other-site.com/foo.png"));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/foo.png"));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/static/../foo.png"));
}

TEST_F(FileLoadPolicyTest, OnePrefix) {
  policy_.Associate("http://www.example.com/static/", "/example/1/");

  GoogleString filename;

  // Map URLs to files.
  EXPECT_EQ("/example/1/foo.png",
            LoadFromFile("http://www.example.com/static/foo.png"));
  // Note: We might have to deal with this case. It will never load :/
  EXPECT_EQ("/example/1/bar/",
            LoadFromFile("http://www.example.com/static/bar/"));
  EXPECT_EQ("/example/1/some/more/dirs/b.css",
            LoadFromFile("http://www.example.com/static/some/more/dirs/b.css"));
  // Drop query string.
  EXPECT_EQ("/example/1/foo.png",
            LoadFromFile("http://www.example.com/static/foo.png?version=3.1"));
  EXPECT_EQ("/example/1/foo.png",
            LoadFromFile("http://www.example.com/static/foo.png?a?b#/c?foo"));
  EXPECT_EQ("/example/1/foo bar.png",
            LoadFromFile("http://www.example.com/static/foo%20bar.png"));
  EXPECT_EQ("/example/1/foo%2Fbar.png",
            LoadFromFile("http://www.example.com/static/foo%2Fbar.png"));

  // Don't map other URLs
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/images/another.gif"));
  EXPECT_FALSE(TryLoadFromFile(
      "http://www.some-site.com/with/many/dirs/a/b.js"));

  EXPECT_FALSE(TryLoadFromFile("http://www.other-site.com/foo.png"));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/foo.png"));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/static/../foo.png"));
}

TEST_F(FileLoadPolicyTest, ManyPrefixes) {
  policy_.Associate("http://www.example.com/static/", "/example/1/");
  // Note: File prefix doesn't end in '/'.
  policy_.Associate("http://www.example.com/images/", "/example/images/static");
  // Note: URL prefix doesn't end in '/'.
  policy_.Associate("http://www.some-site.com/with/many/dirs",
                    "/var/www/some-site.com/");

  GoogleString filename;

  // Map URLs to files.
  EXPECT_EQ("/example/1/foo.png",
            LoadFromFile("http://www.example.com/static/foo.png"));
  EXPECT_EQ("/example/1/bar/",
            LoadFromFile("http://www.example.com/static/bar/"));
  EXPECT_EQ("/example/1/some/more/dirs/b.css",
            LoadFromFile("http://www.example.com/static/some/more/dirs/b.css"));
  // Drop query string.
  EXPECT_EQ("/example/1/foo.png",
            LoadFromFile("http://www.example.com/static/foo.png?version=3.1"));
  EXPECT_EQ("/example/1/foo.png",
            LoadFromFile("http://www.example.com/static/foo.png?a?b#/c?foo"));
  EXPECT_EQ("/example/1/foo bar.png",
            LoadFromFile("http://www.example.com/static/foo%20bar.png"));
  EXPECT_EQ("/example/1/foo%2Fbar.png",
            LoadFromFile("http://www.example.com/static/foo%2Fbar.png"));

  // Map other associations.
  EXPECT_EQ("/example/images/static/another.gif",
            LoadFromFile("http://www.example.com/images/another.gif"));
  EXPECT_EQ("/var/www/some-site.com/a/b.js",
            LoadFromFile("http://www.some-site.com/with/many/dirs/a/b.js"));

  // Don't map other URLs
  EXPECT_FALSE(TryLoadFromFile("http://www.other-site.com/foo.png"));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/foo.png"));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/static/../foo.png"));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/static/%2E%2E/foo.png"));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/static/%2e%2e/foo.png"));
}

TEST_F(FileLoadPolicyTest, RegexpBackreferences) {
  GoogleString error;
  policy_.AssociateRegexp("^https?://example.com/~([^/]*)/static/",
                          "/var/static/\\1/", &error);
  EXPECT_TRUE(error.empty());
  EXPECT_EQ("/var/static/pat/cat.jpg",
            LoadFromFile("http://example.com/~pat/static/cat.jpg"));
  EXPECT_EQ("/var/static/sam/dog.jpg",
            LoadFromFile("http://example.com/~sam/static/dog.jpg"));
  EXPECT_EQ("/var/static/al/css/ie",
            LoadFromFile("https://example.com/~al/static/css/ie"));
}

TEST_F(FileLoadPolicyTest, RegexpNotPrefix) {
  GoogleString error;
  EXPECT_FALSE(policy_.AssociateRegexp("http://example.com/[^/]*/static",
                                       "/var/static/", &error));
  EXPECT_FALSE(error.empty());
}

TEST_F(FileLoadPolicyTest, RegexpExcessBackreferences) {
  GoogleString error;
  EXPECT_FALSE(policy_.AssociateRegexp("^http://([^/]*).com/([^/]*)/static",
                                       "/var/\\1/\\2/\\3/static/", &error));
  EXPECT_FALSE(error.empty());
}

TEST_F(FileLoadPolicyTest, RegexpInvalid) {
  GoogleString error;
  EXPECT_FALSE(policy_.AssociateRegexp("^http://(.com/static",
                                       "/var/www/static/", &error));
  EXPECT_FALSE(error.empty());
}

// Note(sligocki): I'm not sure we should allow overlapping prefixes, but
// here's what happens if you do that now. And I think it's the most reasonable
// behavior if we do allow it.
TEST_F(FileLoadPolicyTest, OverlappingPrefixes) {
  policy_.Associate("http://www.example.com/static/", "/1/");
  policy_.Associate("http://www.example.com/", "/2/");
  policy_.Associate("http://www.example.com/static/sub/dir/", "/3/");

  // Later associations take precedence over earlier ones.
  EXPECT_EQ("/2/foo.png",
            LoadFromFile("http://www.example.com/foo.png"));
  EXPECT_EQ("/2/static/foo.png",
            LoadFromFile("http://www.example.com/static/foo.png"));
  EXPECT_EQ("/3/foo.png",
            LoadFromFile("http://www.example.com/static/sub/dir/foo.png"));
  EXPECT_EQ("/3/plus/foo.png",
            LoadFromFile("http://www.example.com/static/sub/dir/plus/foo.png"));
}

TEST_F(FileLoadPolicyTest, Rules) {
  GoogleString error;
  policy_.Associate("http://example.com/", "/www/");
  EXPECT_EQ("/www/1", LoadFromFile("http://example.com/1"));
  EXPECT_EQ("/www/cgi-bin/guestbook.pl",
            LoadFromFile("http://example.com/cgi-bin/guestbook.pl"));
  policy_.AddRule("/www/cgi-bin/", false /* literal */, false /* disallow */,
                  &error);
  EXPECT_TRUE(error.empty());
  EXPECT_FALSE(TryLoadFromFile("http://example.com/cgi-bin/guestbook.pl"));
  policy_.AddRule("\\.html$", true /* regexp */, true /* allow */, &error);
  EXPECT_TRUE(error.empty());
  EXPECT_EQ("/www/cgi-bin/guestbook.html",
            LoadFromFile("http://example.com/cgi-bin/guestbook.html"));
  policy_.AddRule("\\.ssi\\.html$", true /* regexp */, false /* disallow */,
                  &error);
  EXPECT_TRUE(error.empty());
  EXPECT_FALSE(
      TryLoadFromFile("http://example.com/cgi-bin/guestbook.ssi.html"));
  policy_.AddRule("/www/cgi-bin/allow", false /* literal */, true /* allow */,
                  &error);
  EXPECT_TRUE(error.empty());
  EXPECT_EQ("/www/cgi-bin/allow.ssi.html",
            LoadFromFile("http://example.com/cgi-bin/allow.ssi.html"));
}

TEST_F(FileLoadPolicyTest, Merge) {
  FileLoadPolicy policy1;
  FileLoadPolicy policy2;
  GoogleString error;

  policy1.Associate("http://www.example.com/1/", "/1/");
  EXPECT_EQ("/1/foo.png",
            LoadFromFile("http://www.example.com/1/foo.png", &policy1));
  EXPECT_TRUE(
      policy1.AssociateRegexp("^http://www\\.example\\.com/([^/]*)/",
                              "/\\1/a/", &error));
  EXPECT_TRUE(error.empty());
  // The regexp match is added later so takes precendence over the literal one.

  EXPECT_TRUE(policy1.AddRule("/5/", false /* literal */,
                              false /* disallow */, &error));
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(policy1.AddRule("\\.jpg$", true /* regexp */,
                              false /* disallow */, &error));
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("/1/a/foo.png",
            LoadFromFile("http://www.example.com/1/foo.png", &policy1));
  EXPECT_EQ("/2/a/foo.png",
            LoadFromFile("http://www.example.com/2/foo.png", &policy1));
  EXPECT_EQ("/3/a/foo.png",
            LoadFromFile("http://www.example.com/3/foo.png", &policy1));

  // The next two are blacklisted.
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/3/foo.jpg", &policy1));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/5/foo.png", &policy1));

  policy2.Associate("http://www.example.com/3/", "/3/");
  policy2.Associate("http://www.example.com/4/", "/4/");
  EXPECT_TRUE(policy2.AddRule("exception\\.jpg$", true /* regexp */,
                              true /* allow */, &error));
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("/3/foo.png",
            LoadFromFile("http://www.example.com/3/foo.png", &policy2));
  EXPECT_EQ("/4/foo.png",
            LoadFromFile("http://www.example.com/4/foo.png", &policy2));
  EXPECT_EQ("/4/foo.jpg",
            LoadFromFile("http://www.example.com/4/foo.jpg", &policy2));
  policy1.Merge(policy2);

  EXPECT_EQ("/1/a/foo.png",
            LoadFromFile("http://www.example.com/1/foo.png", &policy1));
  EXPECT_EQ("/2/a/foo.png",
            LoadFromFile("http://www.example.com/2/foo.png", &policy1));

  // Later policies take precendence, so policy2 wins for /3/.
  EXPECT_EQ("/3/foo.png",
            LoadFromFile("http://www.example.com/3/foo.png", &policy1));
  EXPECT_EQ("/4/foo.png",
            LoadFromFile("http://www.example.com/4/foo.png", &policy1));

  // Check rules.
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/5/foo.png", &policy1));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/4/foo.jpg", &policy1));
  EXPECT_EQ("/4/foo.notjpg",
            LoadFromFile("http://www.example.com/4/foo.notjpg", &policy1));
  EXPECT_EQ("/4/exception.jpg",
            LoadFromFile("http://www.example.com/4/exception.jpg", &policy1));
  EXPECT_EQ("/4/anexception.jpg",
            LoadFromFile("http://www.example.com/4/anexception.jpg", &policy1));
  EXPECT_EQ("/5/a/exception.jpg",
            LoadFromFile("http://www.example.com/5/exception.jpg", &policy1));

  // No changes to policy2.
  EXPECT_EQ("/3/foo.png",
            LoadFromFile("http://www.example.com/3/foo.png", &policy2));
  EXPECT_EQ("/4/foo.png",
            LoadFromFile("http://www.example.com/4/foo.png", &policy2));
}

}  // namespace net_instaweb
