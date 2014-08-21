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
        cache_control_(cache_control),
        likely_static_resource_type_(true),
        cacheable_resource_status_code_(true) {
  }

  virtual bool Lookup(const StringPiece& key, StringPieceVector* values) {
    if (key == HttpAttributes::kCacheControl) {
      SplitStringPieceToVector(cache_control_, ",", values, true);
      for (int i = 0, n = values->size(); i < n; ++i) {
        TrimWhitespace(&((*values)[i]));
      }
      return true;
    } else {
      return false;
    }
  }

  virtual bool IsLikelyStaticResourceType() const {
    return likely_static_resource_type_;
  }

  virtual bool IsCacheableResourceStatusCode() const {
    return cacheable_resource_status_code_;
  }

  void set_likely_static_resource_type(bool x) {
    likely_static_resource_type_ = x;
  }

  void set_cacheable_resource_status_code(bool x) {
    cacheable_resource_status_code_ = x;
  }

 private:
  GoogleString cache_control_;
  bool likely_static_resource_type_;
  bool cacheable_resource_status_code_;

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

TEST_F(CachingHeadersTest, IsCacheable) {
  // Default of no headers, likely static resource type and cacheable status
  // code is cacheable.
  SetCacheControl("");
  EXPECT_TRUE(headers_->IsCacheable());

  // It's false if type isn't likely static or status isn't cacheable, though.
  SetCacheControl("");
  headers_->set_cacheable_resource_status_code(false);
  EXPECT_FALSE(headers_->IsCacheable());

  SetCacheControl("");
  headers_->set_likely_static_resource_type(false);
  EXPECT_FALSE(headers_->IsCacheable());

  // Private is OK, for browser cacheability.
  SetCacheControl("private");
  EXPECT_TRUE(headers_->IsCacheable());
  EXPECT_FALSE(headers_->IsProxyCacheable());

  // Various flags that make it non-cacheable.
  SetCacheControl("no-cache");
  EXPECT_FALSE(headers_->IsCacheable());

  SetCacheControl("no-store");
  EXPECT_FALSE(headers_->IsCacheable());

  SetCacheControl("must-revalidate");
  EXPECT_FALSE(headers_->IsCacheable());
  EXPECT_FALSE(headers_->ProxyRevalidate());
  EXPECT_TRUE(headers_->MustRevalidate());

  SetCacheControl("proxy-revalidate");
  EXPECT_TRUE(headers_->IsCacheable());
  EXPECT_TRUE(headers_->ProxyRevalidate());
  EXPECT_FALSE(headers_->MustRevalidate());

  // must-revalidate does not imply uncacheability: it just means
  // that stale content should not be trusted.
  SetCacheControl("must-revalidate,max-age=600");
  EXPECT_FALSE(headers_->ProxyRevalidate());
  EXPECT_TRUE(headers_->MustRevalidate());

  // proxy-revalidate is similar, but does not affect browser heuristics
  SetCacheControl("proxy-revalidate,max-age=600");
  EXPECT_TRUE(headers_->ProxyRevalidate());
  EXPECT_FALSE(headers_->MustRevalidate());
}

}  // namespace net_instaweb
