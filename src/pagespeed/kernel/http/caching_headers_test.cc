// Copyright 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jmarantz@google.com (Joshua Marantz)

#include "pagespeed/kernel/http/caching_headers.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

class TestCachingHeaders : public CachingHeaders {
 public:
  explicit TestCachingHeaders(const char* cache_control)
      : CachingHeaders(HttpStatus::kOK),
        cache_control_(cache_control) {
  }

  virtual bool Lookup(const GoogleString& key, StringPieceVector* values) {
    EXPECT_STREQ(HttpAttributes::kCacheControl, key);
    SplitStringPieceToVector(cache_control_, ",", values, true);
    for (int i = 0, n = values->size(); i < n; ++i) {
      TrimWhitespace(&((*values)[i]));
    }
    return true;
  }

  virtual bool IsLikelyStaticResourceType() const {
    DCHECK(false);  // not called in our use-case.
    return false;
  }

  virtual bool IsCacheableResourceStatusCode() const {
    DCHECK(false);  // not called in our use-case.
    return false;
  }

 private:
  GoogleString cache_control_;

  DISALLOW_COPY_AND_ASSIGN(TestCachingHeaders);
};

class CachingHeadersTest : public testing::Test {
 protected:
  void SetCacheControl(const char* cache_control) {
    headers_.reset(new TestCachingHeaders(cache_control));
  }

  GoogleString DisableCacheControl() {
    return headers_->GenerateDisabledCacheControl();
  }

  scoped_ptr<TestCachingHeaders> headers_;
};

TEST_F(CachingHeadersTest, DisableEmpty) {
  SetCacheControl("");
  EXPECT_STREQ(HttpAttributes::kNoCacheMaxAge0, DisableCacheControl());
}

TEST_F(CachingHeadersTest, DisableCaching) {
  SetCacheControl("max-age=60");
  EXPECT_STREQ(HttpAttributes::kNoCacheMaxAge0, DisableCacheControl());
}

TEST_F(CachingHeadersTest, DisablePrivateCaching) {
  SetCacheControl("private, max-age=60");
  EXPECT_STREQ(HttpAttributes::kNoCacheMaxAge0, DisableCacheControl());
}

TEST_F(CachingHeadersTest, DisablePublicCaching) {
  SetCacheControl("public, max-age=60");
  EXPECT_STREQ(HttpAttributes::kNoCacheMaxAge0, DisableCacheControl());
}

TEST_F(CachingHeadersTest, DisableNostore) {
  SetCacheControl("must-revalidate, private, no-store");
  EXPECT_STREQ(StrCat(HttpAttributes::kNoCacheMaxAge0,
                      ", must-revalidate, ",
                      HttpAttributes::kNoStore),
               DisableCacheControl());
}

TEST_F(CachingHeadersTest, DisableNostoreRetainNoCache) {
  SetCacheControl("no-cache, must-revalidate, private, no-store");
  EXPECT_STREQ(StrCat(HttpAttributes::kNoCacheMaxAge0,
                      ", must-revalidate, ",
                      HttpAttributes::kNoStore),
               DisableCacheControl());
}

}  // namespace net_instaweb
