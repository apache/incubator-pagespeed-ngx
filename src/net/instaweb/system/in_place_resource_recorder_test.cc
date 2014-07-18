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
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

namespace {

const int kMaxResponseBytes = 1024;
const char kTestUrl[] = "http://www.example.com/";
const char kHello[] = "Hello, IPRO.";
const char kBye[] = "Bye IPRO.";

const char kUncompressedData[] = "Hello";

// This was generated with 'xxd -i hello.gz' after gzipping a file with "Hello".
const unsigned char kGzippedData[] = {
  0x1f, 0x8b, 0x08, 0x08, 0x3b, 0x3a, 0xf3, 0x4e, 0x00, 0x03, 0x68, 0x65,
  0x6c, 0x6c, 0x6f, 0x00, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00, 0x82,
  0x89, 0xd1, 0xf7, 0x05, 0x00, 0x00, 0x00
};

class InPlaceResourceRecorderTest : public RewriteTestBase {
 protected:
  enum GzipHeaderTime {
    kPrelimGzipHeader,
    kLateGzipHeader
  };

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    InPlaceResourceRecorder::InitStats(statistics());
  }

  InPlaceResourceRecorder* MakeRecorder(StringPiece url) {
    RequestHeaders headers;
    return new InPlaceResourceRecorder(
        RequestContext::NewTestRequestContext(
            server_context()->thread_system()),
        url, rewrite_driver_->CacheFragment(), headers.GetProperties(),
        kMaxResponseBytes, 4, /* max_concurrent_recordings*/
        http_cache(), statistics(), message_handler());
  }

  void TestWithGzip(GzipHeaderTime header_time) {
    ResponseHeaders prelim_headers;
    prelim_headers.set_status_code(HttpStatus::kOK);
    if (header_time == kPrelimGzipHeader) {
      prelim_headers.Add(HttpAttributes::kContentEncoding,
                         HttpAttributes::kGzip);
    }

    ResponseHeaders final_headers;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &final_headers);
    if (header_time == kLateGzipHeader) {
      final_headers.Add(HttpAttributes::kContentEncoding,
                        HttpAttributes::kGzip);
    }
    final_headers.ComputeCaching();


    scoped_ptr<InPlaceResourceRecorder> recorder(MakeRecorder(kTestUrl));
    recorder->ConsiderResponseHeaders(
        InPlaceResourceRecorder::kPreliminaryHeaders, &prelim_headers);

    StringPiece gzipped(reinterpret_cast<const char*>(&kGzippedData[0]),
                        STATIC_STRLEN(kGzippedData));
    recorder->Write(gzipped, message_handler());
    recorder.release()->DoneAndSetHeaders(&final_headers);

    HTTPValue value_out;
    ResponseHeaders headers_out;
    EXPECT_EQ(HTTPCache::kFound,
              HttpBlockingFind(kTestUrl, http_cache(),
                               &value_out, &headers_out));
    StringPiece contents;
    EXPECT_TRUE(value_out.ExtractContents(&contents));

    // gzip in prelim headers will cause us to decompress, gzip in final_headers
    // will not.
    EXPECT_EQ(header_time == kPrelimGzipHeader ? kUncompressedData : gzipped,
              contents);

    // We should not have a content-encoding header since we either decompressed
    // data ourselves or captured it before data was saved. Also no
    // content-length since we may have done gunzip'ing.
    EXPECT_FALSE(headers_out.Has(HttpAttributes::kContentEncoding));
    EXPECT_FALSE(headers_out.Has(HttpAttributes::kContentLength));
  }

  void CheckCacheableContentType(const ContentType* content_type) {
    ResponseHeaders headers;
    SetDefaultLongCacheHeaders(content_type, &headers);
    scoped_ptr<InPlaceResourceRecorder> recorder(MakeRecorder(kTestUrl));
    recorder->ConsiderResponseHeaders(
        InPlaceResourceRecorder::kFullHeaders, &headers);
    EXPECT_FALSE(recorder->failed());
    HTTPValue value_out;
    ResponseHeaders headers_out;
    EXPECT_EQ(
        HTTPCache::kNotFound,  // Check it wasn't cached as 'not cacheable'.
        HttpBlockingFind(kTestUrl, http_cache(), &value_out, &headers_out));
  }

  void CheckNotCacheableContentType(const ContentType* content_type) {
    ResponseHeaders headers;
    SetDefaultLongCacheHeaders(content_type, &headers);
    scoped_ptr<InPlaceResourceRecorder> recorder(MakeRecorder(kTestUrl));
    recorder->ConsiderResponseHeaders(
        InPlaceResourceRecorder::kFullHeaders, &headers);
    EXPECT_TRUE(recorder->failed());
    HTTPValue value_out;
    ResponseHeaders headers_out;
    EXPECT_EQ(
        HTTPCache::kRecentFetchNotCacheable,
        HttpBlockingFind(kTestUrl, http_cache(), &value_out, &headers_out));
  }
};

TEST_F(InPlaceResourceRecorderTest, BasicOperation) {
  ResponseHeaders prelim_headers;
  prelim_headers.set_status_code(HttpStatus::kOK);

  ResponseHeaders ok_headers;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &ok_headers);

  scoped_ptr<InPlaceResourceRecorder> recorder(MakeRecorder(kTestUrl));
  recorder->ConsiderResponseHeaders(
      InPlaceResourceRecorder::kPreliminaryHeaders, &prelim_headers);
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

TEST_F(InPlaceResourceRecorderTest, CheckCacheableContentTypes) {
  CheckCacheableContentType(&kContentTypeJpeg);
  CheckCacheableContentType(&kContentTypeCss);
  CheckCacheableContentType(&kContentTypeJavascript);
  CheckCacheableContentType(&kContentTypeJson);
}

TEST_F(InPlaceResourceRecorderTest, CheckNotCacheableContentTypes) {
  CheckNotCacheableContentType(&kContentTypePdf);
}

TEST_F(InPlaceResourceRecorderTest, BasicOperationFullHeaders) {
  // Deliver full headers initially. This is how nginx works.
  ResponseHeaders ok_headers;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &ok_headers);

  scoped_ptr<InPlaceResourceRecorder> recorder(MakeRecorder(kTestUrl));
  recorder->ConsiderResponseHeaders(
      InPlaceResourceRecorder::kFullHeaders, &ok_headers);
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
  ResponseHeaders prelim_headers;
  prelim_headers.set_status_code(HttpStatus::kOK);

  // 304
  ResponseHeaders not_modified_headers;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &not_modified_headers);
  not_modified_headers.SetStatusAndReason(HttpStatus::kNotModified);
  not_modified_headers.ComputeCaching();

  scoped_ptr<InPlaceResourceRecorder> recorder(MakeRecorder(kTestUrl));
  recorder->ConsiderResponseHeaders(
      InPlaceResourceRecorder::kPreliminaryHeaders, &prelim_headers);
  recorder.release()->DoneAndSetHeaders(&not_modified_headers);

  HTTPValue value_out;
  ResponseHeaders headers_out;
  // This should be not found, not one of the RememberNot... statuses
  EXPECT_EQ(HTTPCache::kNotFound,
            HttpBlockingFind(kTestUrl, http_cache(), &value_out, &headers_out));
}

TEST_F(InPlaceResourceRecorderTest, Remember500AsFetchFailed) {
  ResponseHeaders prelim_headers;
  prelim_headers.set_status_code(HttpStatus::kOK);

  ResponseHeaders error_headers;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &error_headers);
  error_headers.SetStatusAndReason(HttpStatus::kInternalServerError);
  error_headers.ComputeCaching();

  scoped_ptr<InPlaceResourceRecorder> recorder(MakeRecorder(kTestUrl));
  recorder->ConsiderResponseHeaders(
      InPlaceResourceRecorder::kPreliminaryHeaders, &prelim_headers);
  recorder.release()->DoneAndSetHeaders(&error_headers);

  HTTPValue value_out;
  ResponseHeaders headers_out;
  // This should be not found, not one of the RememberNot... statuses
  EXPECT_EQ(HTTPCache::kRecentFetchFailed,
            HttpBlockingFind(kTestUrl, http_cache(), &value_out, &headers_out));
}

TEST_F(InPlaceResourceRecorderTest, DecompressGzipIfNeeded) {
  // Test where we get already-gzip'd content, as shown by preliminary headers.
  // This corresponds to reverse proxy cases.
  TestWithGzip(kPrelimGzipHeader);
}

TEST_F(InPlaceResourceRecorderTest, SplitOperationWithGzip) {
  // Test that gzip on non-prelim headers don't cause gunzip'ing.
  // This is to permit capture of headers after mod_deflate has run.
  TestWithGzip(kLateGzipHeader);
}

}  // namespace

}  // namespace net_instaweb
