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

#include <cstddef>
#include <set>

#include "net/instaweb/http/public/inflating_fetch.h"

#include "net/instaweb/http/public/async_fetch.h"  // for StringAsyncFetch
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/platform.h"
#include "net/instaweb/util/public/scoped_ptr.h"            // for scoped_ptr
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace {

const char kClearData[] = "Hello";

// This was generated with 'xxd -i hello.gz' after gzipping a file with "Hello".
const unsigned char kGzippedData[] = {
  0x1f, 0x8b, 0x08, 0x08, 0x3b, 0x3a, 0xf3, 0x4e, 0x00, 0x03, 0x68, 0x65,
  0x6c, 0x6c, 0x6f, 0x00, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00, 0x82,
  0x89, 0xd1, 0xf7, 0x05, 0x00, 0x00, 0x00
};

bool binary_data_same(const void* left, size_t left_len,
                      const void* right, size_t right_len) {
  return left_len == right_len && memcmp(left, right, left_len) == 0;
}

}  // namespace

namespace net_instaweb {

class MockFetch : public StringAsyncFetch {
 public:
  explicit MockFetch(const RequestContextPtr& ctx) : StringAsyncFetch(ctx) {}
  virtual ~MockFetch() {}

  void ExpectAcceptEncoding(const StringPiece& encoding) {
    encoding.CopyToString(&accept_encoding_);
  }

  virtual void HandleHeadersComplete() {
    if (!accept_encoding_.empty()) {
      EXPECT_TRUE(request_headers()->HasValue(
          HttpAttributes::kAcceptEncoding, accept_encoding_));
    }
    StringAsyncFetch::HandleHeadersComplete();
  }

 private:
  // If non-empty, EXPECT that each request must accept this encoding.
  GoogleString accept_encoding_;

  DISALLOW_COPY_AND_ASSIGN(MockFetch);
};

class InflatingFetchTest : public testing::Test {
 protected:
  InflatingFetchTest()
      : inflating_fetch_(NULL),
        gzipped_data_(reinterpret_cast<const char*>(kGzippedData),
                      STATIC_STRLEN(kGzippedData)),
        thread_system_(Platform::CreateThreadSystem()) {
  }

  virtual void SetUp() {
    mock_fetch_.reset(new MockFetch(
        RequestContext::NewTestRequestContext(thread_system_.get())));
    inflating_fetch_ = new InflatingFetch(mock_fetch_.get());
  }

  scoped_ptr<MockFetch> mock_fetch_;
  // Self-deletes in Done(), so no need to deallocate.
  InflatingFetch* inflating_fetch_;
  GoogleMessageHandler message_handler_;
  StringPiece gzipped_data_;
  scoped_ptr<ThreadSystem> thread_system_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InflatingFetchTest);
};

// Tests that if we ask for clear text & get it, we pass through the data
// unchanged.
TEST_F(InflatingFetchTest, ClearRequestResponse) {
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(kClearData, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_EQ(kClearData, mock_fetch_->buffer());
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());
}

// Tests that if we ask for clear text, and get a response that claims to
// be gzipped but is actually garbage, our mock callback gets HandleDone(false)
// called, despite the fact that the fetcher (mocked by this code below) called
// Done(true).
TEST_F(InflatingFetchTest, AutoInflateGarbage) {
  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write("this garbage won't inflate", &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_FALSE(mock_fetch_->success());
}

// Tests that if we ask for clear text but get a properly compressed buffer,
// that our inflating-fetch will make this transparent to our Expect callback.
TEST_F(InflatingFetchTest, AutoInflate) {
  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_EQ(kClearData, mock_fetch_->buffer())
      << "data should be auto-inflated.";
  EXPECT_TRUE(mock_fetch_->response_headers()->Lookup1(
      HttpAttributes::kContentEncoding) == NULL)
      << "Content encoding should be stripped since we inflated the data.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());
}

// Tests that if we asked for a gzipped response in the first place that
// we don't inflate or strip the content-encoding header.
TEST_F(InflatingFetchTest, ExpectGzipped) {
  inflating_fetch_->request_headers()->Add(
      HttpAttributes::kAcceptEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_TRUE(
      binary_data_same(
          gzipped_data_.data(), gzipped_data_.length(),
          mock_fetch_->buffer().data(), mock_fetch_->buffer().size()))
          << "data should be untouched.";
  EXPECT_STREQ(HttpAttributes::kGzip, mock_fetch_->response_headers()->Lookup1(
      HttpAttributes::kContentEncoding)) << "content-encoding not stripped.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());
}

// Tests that when we asked for gzipped content and Content-Type was set to
// blacklisted binary/octet-stream type, we do not inflate it.
TEST_F(InflatingFetchTest, ExpectGzippedOnOctetStream) {
  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);

  // Make sure we pass through octet-stream data.
  std::set<const ContentType*> blacklist;
  blacklist.insert(&kContentTypeBinaryOctetStream);

  inflating_fetch_->set_inflation_content_type_blacklist(blacklist);

  // We need to set Content-Type to be one of octet-streams types.
  inflating_fetch_->response_headers()->Add(HttpAttributes::kContentType,
                                            "application/octet-stream");
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_TRUE(
      binary_data_same(
          gzipped_data_.data(), gzipped_data_.length(),
          mock_fetch_->buffer().data(), mock_fetch_->buffer().size()))
          << "data should be untouched when octet-stream is blacklisted.";
  EXPECT_STREQ(HttpAttributes::kGzip, mock_fetch_->response_headers()->Lookup1(
      HttpAttributes::kContentEncoding)) << "content-encoding is stripped.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());

  // Use a different Content-Type and check that data will be inflated.
  mock_fetch_->Reset();
  inflating_fetch_ = new InflatingFetch(mock_fetch_.get());
  inflating_fetch_->set_inflation_content_type_blacklist(blacklist);

  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->Add(HttpAttributes::kContentType,
                                            "image/gif");
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_STREQ(kClearData, mock_fetch_->buffer())
      << "data should be inflated when content-type is not in blacklist.";
  EXPECT_FALSE(
      mock_fetch_->response_headers()->Has(HttpAttributes::kContentEncoding))
          << "content-encoding is not stripped.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());
}

// Tests that unknown content type, when listed does not trigger inflation.
TEST_F(InflatingFetchTest, ExpectGzippedWithNullContentType) {
  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);

  // Disable inflating of unknown types.
  std::set<const ContentType*> blacklist;
  blacklist.insert(NULL /*unknown content type*/);

  inflating_fetch_->set_inflation_content_type_blacklist(blacklist);

  // We don't set content type here, so determined type is NULL.
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_TRUE(
      binary_data_same(
          gzipped_data_.data(), gzipped_data_.length(),
          mock_fetch_->buffer().data(), mock_fetch_->buffer().size()))
          << "data should be untouched when unknown content type is"
          << " blacklisted.";
  EXPECT_STREQ(HttpAttributes::kGzip, mock_fetch_->response_headers()->Lookup1(
      HttpAttributes::kContentEncoding)) << "content-encoding is stripped.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());

  // Now check that without NULL, unknown types are inflated.
  mock_fetch_->Reset();
  inflating_fetch_ = new InflatingFetch(mock_fetch_.get());
  blacklist.clear();
  inflating_fetch_->set_inflation_content_type_blacklist(blacklist);

  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_STREQ(kClearData, mock_fetch_->buffer())
      << "data should be inflated when content-type is not in blacklist.";
  EXPECT_FALSE(
      mock_fetch_->response_headers()->Has(HttpAttributes::kContentEncoding))
          << "content-encoding is not stripped.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());
}

// Same as previous test, but does testing of other types (included and not in
// the blacklist).
TEST_F(InflatingFetchTest, ExpectGzippedWithNullContentTypeAndWithOther) {
  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);

  // Set up unknown and jpeg types not to be inflated.
  std::set<const ContentType*> blacklist;
  blacklist.insert(NULL /*unknown content type*/);
  blacklist.insert(&kContentTypeJpeg);

  inflating_fetch_->set_inflation_content_type_blacklist(blacklist);

  // We need to set Content-Type to be NULL here.
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_TRUE(
      binary_data_same(
          gzipped_data_.data(), gzipped_data_.length(),
          mock_fetch_->buffer().data(), mock_fetch_->buffer().size()))
          << "data should be untouched when unknown content type is"
          << " blacklisted.";
  EXPECT_STREQ(HttpAttributes::kGzip, mock_fetch_->response_headers()->Lookup1(
      HttpAttributes::kContentEncoding)) << "content-encoding is stripped.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());

  // Use a different Content-Type and check that data will be inflated.
  mock_fetch_->Reset();
  inflating_fetch_ = new InflatingFetch(mock_fetch_.get());
  inflating_fetch_->set_inflation_content_type_blacklist(blacklist);

  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->Add(HttpAttributes::kContentType,
                                            "image/gif");
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_STREQ(kClearData, mock_fetch_->buffer())
      << "data should be inflated when content-type is not in blacklist.";
  EXPECT_FALSE(
      mock_fetch_->response_headers()->Has(HttpAttributes::kContentEncoding))
          << "content-encoding is not stripped.";

  // Now use Jpeg, which is also blacklisted to make sure it's not inflated.
  mock_fetch_->Reset();
  inflating_fetch_ = new InflatingFetch(mock_fetch_.get());
  inflating_fetch_->set_inflation_content_type_blacklist(blacklist);

  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->Add(HttpAttributes::kContentType,
                                            "image/jpeg");
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_TRUE(
      binary_data_same(
          gzipped_data_.data(), gzipped_data_.length(),
          mock_fetch_->buffer().data(), mock_fetch_->buffer().size()))
          << "data should be inflated when content-type is not in blacklist.";

  // Now check that without NULL, but with other types,
  // unknown types are inflated.
  mock_fetch_->Reset();
  inflating_fetch_ = new InflatingFetch(mock_fetch_.get());
  blacklist.clear();
  blacklist.insert(&kContentTypeGif);
  inflating_fetch_->set_inflation_content_type_blacklist(blacklist);

  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_STREQ(kClearData, mock_fetch_->buffer())
      << "data should be inflated when content-type is not in blacklist.";
  EXPECT_FALSE(
      mock_fetch_->response_headers()->Has(HttpAttributes::kContentEncoding))
          << "content-encoding is not stripped.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());
}

// Tests that a blacklist with multiple types leaves those types compressed, and
// that types not in the blacklist are inflated.
TEST_F(InflatingFetchTest, ExpectGzippedOnManyTypes) {
  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);

  // Make sure we pass through octet-stream & jpeg data.
  std::set<const ContentType*> blacklist;
  blacklist.insert(&kContentTypeJpeg);
  blacklist.insert(&kContentTypeBinaryOctetStream);

  inflating_fetch_->set_inflation_content_type_blacklist(blacklist);

  // We need to set Content-Type to be one of them octet-streams.
  inflating_fetch_->response_headers()->Add(HttpAttributes::kContentType,
                                            "image/jpeg");
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_TRUE(
      binary_data_same(
          gzipped_data_.data(), gzipped_data_.length(),
          mock_fetch_->buffer().data(), mock_fetch_->buffer().size()))
          << "data should be untouched when jpeg is contained in blacklist.";
  EXPECT_STREQ(HttpAttributes::kGzip, mock_fetch_->response_headers()->Lookup1(
      HttpAttributes::kContentEncoding)) << "content-encoding is stripped.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());

  // Check octet-stream (should be still compressed).
  mock_fetch_->Reset();
  inflating_fetch_ = new InflatingFetch(mock_fetch_.get());
  inflating_fetch_->set_inflation_content_type_blacklist(blacklist);

  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->Add(HttpAttributes::kContentType,
                                            "application/octet-stream");
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_TRUE(
      binary_data_same(
          gzipped_data_.data(), gzipped_data_.length(),
          mock_fetch_->buffer().data(), mock_fetch_->buffer().size()))
          << "data should not be inflated when content-type is in the"
          << " blacklist.";
  EXPECT_STREQ(HttpAttributes::kGzip, mock_fetch_->response_headers()->Lookup1(
      HttpAttributes::kContentEncoding)) << "content-encoding is not stripped.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());

  // Use a different image type and check that data will be inflated.
  mock_fetch_->Reset();
  inflating_fetch_ = new InflatingFetch(mock_fetch_.get());
  inflating_fetch_->set_inflation_content_type_blacklist(blacklist);

  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->Add(HttpAttributes::kContentType,
                                            "image/gif");
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_STREQ(kClearData, mock_fetch_->buffer())
      << "data should be inflated when content-type is not in blacklist.";
  EXPECT_FALSE(
      mock_fetch_->response_headers()->Has(HttpAttributes::kContentEncoding))
          << "content-encoding is not stripped.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());
}

// Check that empty blacklist is processed correctly and everything is inflated.
TEST_F(InflatingFetchTest, ExpectUnGzippedOnEmptyBlacklist) {
  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);

  // Set empty set to be left compressed.
  std::set<const ContentType*> blacklist;
  inflating_fetch_->set_inflation_content_type_blacklist(blacklist);

  // We need to set Content-Type to one of the octet-streams types.
  inflating_fetch_->response_headers()->Add(HttpAttributes::kContentType,
                                            "binary/octet-stream");
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_STREQ(kClearData, mock_fetch_->buffer())
      << "data should be uncompressed when blacklist filter is empty.";
  EXPECT_FALSE(
      mock_fetch_->response_headers()->Has(HttpAttributes::kContentEncoding))
          << "content-encoding is not stripped.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());

  // Check some other type.
  mock_fetch_->Reset();
  inflating_fetch_ = new InflatingFetch(mock_fetch_.get());
  inflating_fetch_->set_inflation_content_type_blacklist(blacklist);

  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->Add(HttpAttributes::kContentType,
                                            "image/gif");
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_STREQ(kClearData, mock_fetch_->buffer())
      << "data should be inflated when content-type is not in blacklist.";
  EXPECT_FALSE(
      mock_fetch_->response_headers()->Has(HttpAttributes::kContentEncoding))
          << "content-encoding is not stripped.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());
}


TEST_F(InflatingFetchTest, ContentGzipAndDeflatedButWantClear) {
  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kDeflate);

  // Apply gzip second so that it gets decoded first as we want to decode
  // in reverse order to how the encoding was done.
  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_EQ(kClearData, mock_fetch_->buffer())
      << "data should be auto-unzipped but deflate is not attempted.";
  EXPECT_STREQ(HttpAttributes::kDeflate,
               mock_fetch_->response_headers()->Lookup1(
                   HttpAttributes::kContentEncoding))
      << "deflate encoding remains though gzip encoding is stripped.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());
}

// Tests that content that was first gzipped, and then encoded with
// some encoder ("frob") unknown to our system does not get touched.
// We should not attempt to gunzip the 'frob' data.
TEST_F(InflatingFetchTest, GzippedAndFrobbedNotChanged) {
  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, "frob");
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);

  EXPECT_EQ(gzipped_data_, mock_fetch_->buffer())
      << "data should be not be altered (even though it happens to be gzipped)";
  ConstStringStarVector encodings;
  ASSERT_TRUE(mock_fetch_->response_headers()->Lookup(
      HttpAttributes::kContentEncoding, &encodings))
      << "deflate encoding remains though gzip encoding is stripped.";
  ASSERT_EQ(2, encodings.size());
  EXPECT_STREQ(HttpAttributes::kGzip, *encodings[0]);
  EXPECT_STREQ("frob", *encodings[1]);
}

TEST_F(InflatingFetchTest, TestEnableGzipFromBackend) {
  mock_fetch_->ExpectAcceptEncoding(HttpAttributes::kGzip);
  inflating_fetch_->EnableGzipFromBackend();
  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_EQ(kClearData, mock_fetch_->buffer())
      << "data should be auto-inflated.";
  EXPECT_TRUE(mock_fetch_->response_headers()->Lookup1(
      HttpAttributes::kContentEncoding) == NULL)
      << "Content encoding should be stripped since we inflated the data.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());
}

TEST_F(InflatingFetchTest, TestEnableGzipFromBackendWithCleartext) {
  mock_fetch_->ExpectAcceptEncoding(HttpAttributes::kGzip);
  inflating_fetch_->EnableGzipFromBackend();

  // We are going to ask the mock server for gzip, but we'll get cleartext.
  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(kClearData, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_EQ(kClearData, mock_fetch_->buffer());
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());
}

TEST_F(InflatingFetchTest, TestEnableGzipFromBackendExpectingGzip) {
  inflating_fetch_->request_headers()->Add(
      HttpAttributes::kAcceptEncoding, HttpAttributes::kGzip);
  inflating_fetch_->response_headers()->Add(
      HttpAttributes::kContentEncoding, HttpAttributes::kGzip);

  // Calling EnableGzipFromBackend here has no effect in this case,
  // because above we declare that we want to see gzipped data coming
  // into our Write methods.
  inflating_fetch_->EnableGzipFromBackend();
  mock_fetch_->ExpectAcceptEncoding(HttpAttributes::kGzip);

  inflating_fetch_->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  inflating_fetch_->Write(gzipped_data_, &message_handler_);
  inflating_fetch_->Done(true);
  EXPECT_TRUE(
      binary_data_same(
          gzipped_data_.data(), gzipped_data_.length(),
          mock_fetch_->buffer().data(), mock_fetch_->buffer().size()))
          << "data should be untouched.";
  EXPECT_STREQ(HttpAttributes::kGzip, mock_fetch_->response_headers()->Lookup1(
      HttpAttributes::kContentEncoding)) << "content-encoding not stripped.";
  EXPECT_TRUE(mock_fetch_->done());
  EXPECT_TRUE(mock_fetch_->success());
}

// TODO(jmarantz): test 'deflate' without gzip

}  // namespace net_instaweb
