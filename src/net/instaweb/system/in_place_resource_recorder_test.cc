/*
 * Copyright 2014 Google Inc.
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
// Unit tests for InPlaceResourceRecorder.

#include "net/instaweb/system/public/in_place_resource_recorder.h"

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"


namespace net_instaweb {

namespace {

const int kMaxResponseBytes = 1024;
const char kTestUrl[] = "http://www.example.com/";
const char kHello[] = "Hello, IPRO.";
const char kBye[] = "Bye IPRO.";

class InPlaceResourceRecorderTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    InPlaceResourceRecorder::InitStats(statistics());
  }

  InPlaceResourceRecorder* MakeRecorder(StringPiece url) {
    RequestHeaders headers;
    return new InPlaceResourceRecorder(
        url, headers, true /* respect_vary*/,
        kMaxResponseBytes, 4, /* max_concurrent_recordings*/
        300 * Timer::kSecondMs /* implicit_cache_ttl_ms*/,
        http_cache(), statistics(), message_handler());
  }
};

TEST_F(InPlaceResourceRecorderTest, BasicOperation) {
  ResponseHeaders ok_headers;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &ok_headers);

  scoped_ptr<InPlaceResourceRecorder> recorder(MakeRecorder(kTestUrl));
  recorder->Write(kHello, message_handler());
  recorder->Write(kBye, message_handler());
  recorder.release()->DoneAndSetHeaders(&ok_headers);

  HTTPValue value_out;
  ResponseHeaders headers_out;
  EXPECT_EQ(HTTPCache::kFound,
            HttpBlockingFind(kTestUrl, http_cache(), &value_out, &headers_out));
  StringPiece contents;
  EXPECT_TRUE(value_out.ExtractContents(&contents));
  EXPECT_EQ(StrCat(kHello, kBye), contents);
}

TEST_F(InPlaceResourceRecorderTest, DontRemember304) {
  // 304
  ResponseHeaders not_modified_headers;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &not_modified_headers);
  not_modified_headers.SetStatusAndReason(HttpStatus::kNotModified);
  not_modified_headers.ComputeCaching();

  scoped_ptr<InPlaceResourceRecorder> recorder(MakeRecorder(kTestUrl));
  recorder.release()->DoneAndSetHeaders(&not_modified_headers);

  HTTPValue value_out;
  ResponseHeaders headers_out;
  // This should be not found, not one of the RememberNot... statuses
  EXPECT_EQ(HTTPCache::kNotFound,
            HttpBlockingFind(kTestUrl, http_cache(), &value_out, &headers_out));
}

TEST_F(InPlaceResourceRecorderTest, Remember500AsFetchFailed) {
  ResponseHeaders error_headers;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &error_headers);
  error_headers.SetStatusAndReason(HttpStatus::kInternalServerError);
  error_headers.ComputeCaching();

  scoped_ptr<InPlaceResourceRecorder> recorder(MakeRecorder(kTestUrl));
  recorder.release()->DoneAndSetHeaders(&error_headers);

  HTTPValue value_out;
  ResponseHeaders headers_out;
  // This should be not found, not one of the RememberNot... statuses
  EXPECT_EQ(HTTPCache::kRecentFetchFailed,
            HttpBlockingFind(kTestUrl, http_cache(), &value_out, &headers_out));
}

}  // namespace

}  // namespace net_instaweb
