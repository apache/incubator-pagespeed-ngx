// Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test InflatingFetch.

#include "net/instaweb/http/public/inflating_fetch.h"

#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kClearData[] = "Hello";

// This was generated with 'xxd -i hello.gz' after gzipping a file with "Hello".
const unsigned char kGzippedData[] = {
  0x1f, 0x8b, 0x08, 0x08, 0x3b, 0x3a, 0xf3, 0x4e, 0x00, 0x03, 0x68, 0x65,
  0x6c, 0x6c, 0x6f, 0x00, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00, 0x82,
  0x89, 0xd1, 0xf7, 0x05, 0x00, 0x00, 0x00
};

}  // namespace

namespace net_instaweb {

class InflatingFetchTest : public testing::Test {
 protected:
  InflatingFetchTest()
      : inflating_fetch_(&mock_fetch_),
        gzipped_data_(reinterpret_cast<const char*>(kGzippedData),
                      STATIC_STRLEN(kGzippedData)) {
  }

  StringAsyncFetch mock_fetch_;
  InflatingFetch inflating_fetch_;
  GoogleMessageHandler message_handler_;
  StringPiece gzipped_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InflatingFetchTest);
};

// Tests that if we ask for clear text & get it, we pass through the data
// unchanged.
TEST_F(InflatingFetchTest, ClearRequestResponse) {
  inflating_fetch_.response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_.Write(kClearData, &message_handler_);
  inflating_fetch_.Done(true);
  EXPECT_EQ(kClearData, mock_fetch_.buffer());
  EXPECT_TRUE(mock_fetch_.done());
  EXPECT_TRUE(mock_fetch_.success());
}

// Tests that if we ask for clear text, and get a response that claims to
// be gzipped but is actually garbage, our mock callback gets HandleDone(false)
// called, despite the fact that the fetcher (mocked by this code below) called
// Done(true).
TEST_F(InflatingFetchTest, AutoInflateGarbage) {
  inflating_fetch_.response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_.response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_.Write("this garbage won't inflate", &message_handler_);
  inflating_fetch_.Done(true);
  EXPECT_TRUE(mock_fetch_.done());
  EXPECT_FALSE(mock_fetch_.success());
}

// Tests that if we ask for clear text but get a properly compressed buffer,
// that our inflating-fetch will make this transparent to our Expect callback.
TEST_F(InflatingFetchTest, AutoInflate) {
  inflating_fetch_.response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_.response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_.Write(gzipped_data_, &message_handler_);
  inflating_fetch_.Done(true);
  EXPECT_EQ(kClearData, mock_fetch_.buffer())
      << "data should be auto-inflated";
  EXPECT_TRUE(mock_fetch_.response_headers()->Lookup1(
      HttpAttributes::kContentEncoding) == NULL)
      << "Content encoding should be stripped since we inflated the data";
  EXPECT_TRUE(mock_fetch_.done());
  EXPECT_TRUE(mock_fetch_.success());
}

// Tests that if we asked for a gzipped response in the first place that
// we don't inflate or strip the content-encoding header.
TEST_F(InflatingFetchTest, ExpectGzipped) {
  inflating_fetch_.request_headers()->Add(
      HttpAttributes::kAcceptEncoding, HttpAttributes::kGzip);
  inflating_fetch_.response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_.response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_.Write(gzipped_data_, &message_handler_);
  inflating_fetch_.Done(true);
  EXPECT_STREQ(gzipped_data_, mock_fetch_.buffer())
      << "data should be untouched";
  EXPECT_STREQ(HttpAttributes::kGzip, mock_fetch_.response_headers()->Lookup1(
      HttpAttributes::kContentEncoding)) << "content-encoding not stripped";
  EXPECT_TRUE(mock_fetch_.done());
  EXPECT_TRUE(mock_fetch_.success());
}

TEST_F(InflatingFetchTest, ContentGzipAndDeflatedButWantClear) {
  inflating_fetch_.response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kDeflate);

  // Apply gzip second so that it gets decoded first as we want to decode
  // in reverse order to how the encoding was done.
  inflating_fetch_.response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_.response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_.Write(gzipped_data_, &message_handler_);
  inflating_fetch_.Done(true);
  EXPECT_EQ(kClearData, mock_fetch_.buffer())
      << "data should be auto-unzipped but deflate is not attemped";
  EXPECT_STREQ(HttpAttributes::kDeflate,
               mock_fetch_.response_headers()->Lookup1(
                   HttpAttributes::kContentEncoding))
      << "deflate encoding remains though gzip encoding is stripped";
  EXPECT_TRUE(mock_fetch_.done());
  EXPECT_TRUE(mock_fetch_.success());
}

// Tests that content that was first gzipped, and then encoded with
// some encoder ("frob") unknown to our system does not get touched.
// We should not attempt to gunzip the 'frob' data.
TEST_F(InflatingFetchTest, GzippedAndFrobbedNotChanged) {
  inflating_fetch_.response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_.response_headers()->Add(
      HttpAttributes::kContentEncoding, "frob");
  inflating_fetch_.response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_.Write(gzipped_data_, &message_handler_);
  inflating_fetch_.Done(true);

  EXPECT_EQ(gzipped_data_, mock_fetch_.buffer())
      << "data should be not be altered (even though it happens to be gzipped)";
  ConstStringStarVector encodings;
  ASSERT_TRUE(mock_fetch_.response_headers()->Lookup(
      HttpAttributes::kContentEncoding, &encodings))
      << "deflate encoding remains though gzip encoding is stripped";
  ASSERT_EQ(2, encodings.size());
  EXPECT_STREQ(HttpAttributes::kGzip, *encodings[0]);
  EXPECT_STREQ("frob", *encodings[1]);
}

// TODO(jmarantz): test 'deflate' without gzip

}  // namespace net_instaweb
