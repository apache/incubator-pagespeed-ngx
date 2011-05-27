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

#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class FileLoadPolicyTest : public ::testing::Test {
 protected:
  // Generally use this for URLs you don't expect to be loaded from files.
  // e.g. EXPECT_FALSE(TryLoadFromFile("http://www.example.com/"));
  bool TryLoadFromFile(const StringPiece& url_string) {
    GoogleUrl url(url_string);
    GoogleString filename;
    return policy_.ShouldLoadFromFile(url, &filename);
  }

  // Generally use this for URLs you do expect to be loaded from files.
  // e.g. EXPECT_EQ("filename", LoadFromFile("url"));
  GoogleString LoadFromFile(const StringPiece& url_string) {
    GoogleUrl url(url_string);
    GoogleString filename;
    bool load = policy_.ShouldLoadFromFile(url, &filename);
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

  // Map other associations.
  EXPECT_EQ("/example/images/static/another.gif",
            LoadFromFile("http://www.example.com/images/another.gif"));
  EXPECT_EQ("/var/www/some-site.com/a/b.js",
            LoadFromFile("http://www.some-site.com/with/many/dirs/a/b.js"));

  // Don't map other URLs
  EXPECT_FALSE(TryLoadFromFile("http://www.other-site.com/foo.png"));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/foo.png"));
  EXPECT_FALSE(TryLoadFromFile("http://www.example.com/static/../foo.png"));
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

TEST_F(FileLoadPolicyTest, Merge) {
  FileLoadPolicy policy1;
  FileLoadPolicy policy2;

  policy1.Associate("http://www.example.com/1/", "/1/");
  policy1.Associate("http://www.example.com/2/", "/2/");
  policy2.Associate("http://www.example.com/3/", "/3/");
  policy2.Associate("http://www.example.com/4/", "/4/");

  ASSERT_EQ(2, policy1.url_filenames_.size());
  FileLoadPolicy::UrlFilenames::const_iterator iter;
  iter = policy1.url_filenames_.begin();
  EXPECT_EQ("http://www.example.com/1/", iter->url_prefix);
  ++iter;
  EXPECT_EQ("http://www.example.com/2/", iter->url_prefix);

  ASSERT_EQ(2, policy2.url_filenames_.size());
  iter = policy2.url_filenames_.begin();
  EXPECT_EQ("http://www.example.com/3/", iter->url_prefix);
  ++iter;
  EXPECT_EQ("http://www.example.com/4/", iter->url_prefix);

  policy2.Merge(policy1);

  ASSERT_EQ(4, policy2.url_filenames_.size());
  iter = policy2.url_filenames_.begin();
  EXPECT_EQ("http://www.example.com/3/", iter->url_prefix);
  ++iter;
  EXPECT_EQ("http://www.example.com/4/", iter->url_prefix);
  ++iter;
  EXPECT_EQ("http://www.example.com/1/", iter->url_prefix);
  ++iter;
  EXPECT_EQ("http://www.example.com/2/", iter->url_prefix);
}

}  // namespace net_instaweb
