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

#include "net/instaweb/apache/header_util.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/http/http_names.h"

#include "apr_pools.h"                                               // NOLINT
#include "http_request.h"                                            // NOLINT

// Provide stubs for functions normally resolved when loaded into
// httpd, which we are not linking into the test.
extern "C" {
  void ap_set_content_type(request_rec* request, char* type) {
    // Note that this is not the exact correct functionallity, but is
    // sufficient for the test.
    apr_table_set(request->headers_out,
                  net_instaweb::HttpAttributes::kContentType,
                  type);
  }

  void ap_remove_output_filter(ap_filter_t* filter) {
  }
}

namespace net_instaweb {

class HeaderUtilTest : public testing::Test {
 protected:
  virtual void SetUp() {
    apr_initialize();
    atexit(apr_terminate);
    apr_pool_create(&pool_, NULL);
    request_.headers_out = apr_table_make(pool_, 10);
  }

  virtual void TearDown() {
    apr_pool_destroy(pool_);
  }

  void SetCacheControl(const char* cache_control) {
    apr_table_set(request_.headers_out, HttpAttributes::kCacheControl,
                  cache_control);
  }

  const char* GetCacheControl() {
    return apr_table_get(request_.headers_out, HttpAttributes::kCacheControl);
  }

  apr_pool_t* pool_;
  request_rec request_;
};

TEST_F(HeaderUtilTest, DisableEmpty) {
  DisableCaching(&request_);
  EXPECT_STREQ(HttpAttributes::kNoCacheMaxAge0, GetCacheControl());
}

TEST_F(HeaderUtilTest, DisableCaching) {
  SetCacheControl("max-age=60");
  DisableCaching(&request_);
  EXPECT_STREQ(HttpAttributes::kNoCacheMaxAge0, GetCacheControl());
}

TEST_F(HeaderUtilTest, DisablePrivateCaching) {
  SetCacheControl("private, max-age=60");
  DisableCaching(&request_);
  EXPECT_STREQ(HttpAttributes::kNoCacheMaxAge0, GetCacheControl());
}

TEST_F(HeaderUtilTest, DisablePublicCaching) {
  SetCacheControl("public, max-age=60");
  DisableCaching(&request_);
  EXPECT_STREQ(HttpAttributes::kNoCacheMaxAge0, GetCacheControl());
}

TEST_F(HeaderUtilTest, DisableNostore) {
  SetCacheControl("must-revalidate, private, no-store");
  DisableCaching(&request_);
  EXPECT_STREQ(StrCat(HttpAttributes::kNoCacheMaxAge0,
                      ", must-revalidate, ",
                      HttpAttributes::kNoStore),
               GetCacheControl());
}

TEST_F(HeaderUtilTest, DisableNostoreRetainNoCache) {
  SetCacheControl("no-cache, must-revalidate, private, no-store");
  DisableCaching(&request_);
  EXPECT_STREQ(StrCat(HttpAttributes::kNoCacheMaxAge0,
                      ", must-revalidate, ",
                      HttpAttributes::kNoStore),
               GetCacheControl());
}

}  // namespace net_instaweb
