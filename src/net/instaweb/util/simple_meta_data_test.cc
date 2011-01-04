// Copyright 2010 and onwards Google Inc.
// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test SimpleUrlData, in particular it's HTTP header parser.

#include "net/instaweb/util/public/simple_meta_data.h"
#include <algorithm>
#include "base/basictypes.h"
#include "base/logging.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

const char* kTestDir = "/net/instaweb/util/testdata/";

class SimpleMetaDataTest : public testing::Test {
 protected:
  SimpleMetaDataTest() { }

  void CheckGoogleHeaders(const MetaData& meta_data) {
    EXPECT_EQ(200, meta_data.status_code());
    EXPECT_EQ(1, meta_data.major_version());
    EXPECT_EQ(0, meta_data.minor_version());
    EXPECT_EQ(std::string("OK"), std::string(meta_data.reason_phrase()));
    CharStarVector values;
    EXPECT_TRUE(meta_data.Lookup("X-Google-Experiment", &values));
    EXPECT_EQ(std::string("23729,24249,24253"), std::string(values[0]));
    EXPECT_TRUE(meta_data.Lookup(HttpAttributes::kSetCookie, &values));
    EXPECT_EQ(2, values.size());
    EXPECT_EQ(std::string("PREF=ID=3935f510d83d2a7a:TM=1270493386:LM=127049338"
                           "6:S=u_18e6r8aJ83N6P1; "
                           "expires=Wed, 04-Apr-2012 18:49:46 GMT; path=/; "
                           "domain=.google.com"),
              std::string(values[0]));
    EXPECT_EQ(std::string("NID=33=aGkk7cKzznoUuCd19qTgXlBjXC8fc_luIo2Yk9BmrevU"
                           "gXYPTazDF8Q6JvsO6LvTu4mfI8_44iIBLu4pF-Mvpe4wb7pYwej"
                           "4q9HvbMLRxt-OzimIxmd-bwyYVfZ2PY1B; "
                           "expires=Tue, 05-Oct-2010 18:49:46 GMT; path=/; "
                           "domain=.google.com; HttpOnly"),
              std::string(values[1]));
    EXPECT_EQ(15, meta_data.NumAttributes());
    EXPECT_EQ(std::string("X-Google-GFE-Response-Body-Transformations"),
              std::string(meta_data.Name(14)));
    EXPECT_EQ(std::string("gunzipped"), std::string(meta_data.Value(14)));
  }

  void ParseHeaders(const StringPiece& headers) {
    meta_data_.ParseChunk(headers, &message_handler_);
  }

  // Check sizes of the header vector and map.
  void ExpectSizes(int num_headers, int num_header_names) {
    EXPECT_EQ(num_headers, meta_data_.NumAttributes());
    EXPECT_EQ(num_header_names, meta_data_.NumAttributeNames());
  }

  bool ComputeImplicitCaching(int status_code, const char* content_type) {
    std::string header_text =
        StringPrintf("HTTP/1.0 %d OK\r\n"
                     "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
                     "Content-type: %s\r\n\r\n",
                     status_code, content_type);
    meta_data_.Clear();
    ParseHeaders(header_text);
    return meta_data_.IsCacheable();
  }

  // At the end of every test, check to make sure that clearing the
  // meta-data produces an equivalent structure to a freshly initiliazed
  // one.
  virtual void TearDown() {
    meta_data_.Clear();
    SimpleMetaData empty_meta_data;

    // TODO(jmarantz): at present we lack a comprehensive serialization
    // that covers all the member variables, but at least we can serialize
    // to an HTTP-compatible string.
    EXPECT_EQ(empty_meta_data.ToString(), meta_data_.ToString());
  }

  GoogleMessageHandler message_handler_;
  SimpleMetaData meta_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleMetaDataTest);
};

// Parse the headers from google.com
TEST_F(SimpleMetaDataTest, TestParseAndWrite) {
  std::string fname = GTestSrcDir() + kTestDir + "google.http";
  std::string http_buffer;
  StdioFileSystem fs;

  ASSERT_TRUE(fs.ReadFile(fname.c_str(), &http_buffer, &message_handler_));

  // Make a small buffer to test that we will successfully parse headers
  // that are split across buffers.  This is from
  //     wget --save-headers http://www.google.com
  const int bufsize = 100;
  int num_consumed = 0;
  for (int i = 0; i < http_buffer.size(); i += bufsize) {
    int size = std::min(bufsize, static_cast<int>(http_buffer.size() - i));
    num_consumed += meta_data_.ParseChunk(http_buffer.substr(i, size),
                                          &message_handler_);
    if (meta_data_.headers_complete()) {
      break;
    }
  }

  // Verifies that after the headers, we see the content.  Note that this
  // test uses 'wget' style output, and wget takes care of any unzipping,
  // so this should not be mistaken for a content decoder, such as the
  // net/instaweb/latencylabs/http_response_serializer.h.
  static const char start_of_doc[] = "<!doctype html>";
  EXPECT_EQ(0, strncmp(start_of_doc, http_buffer.c_str() + num_consumed,
                       sizeof(start_of_doc) - 1));
  CheckGoogleHeaders(meta_data_);

  // Now write the headers into a string.
  std::string outbuf;
  StringWriter writer(&outbuf);
  meta_data_.Write(&writer, &message_handler_);

  // Re-read into a fresh meta-data object and parse again.
  SimpleMetaData meta_data2;
  num_consumed = meta_data2.ParseChunk(outbuf, &message_handler_);
  EXPECT_EQ(outbuf.size(), static_cast<size_t>(num_consumed));
  CheckGoogleHeaders(meta_data2);
}

// Test caching header interpretation.  Note that the detailed testing
// of permutations is done in pagespeed/core/resource_util_test.cc.  We
// are just trying to ensure that we have populated the Resource object
// properly and that we have extracted the bits we need.
TEST_F(SimpleMetaDataTest, TestCachingNeedDate) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Cache-control: max-age=300\r\n\r\n");
  EXPECT_FALSE(meta_data_.IsCacheable());
  EXPECT_EQ(0, meta_data_.CacheExpirationTimeMs());
}

TEST_F(SimpleMetaDataTest, TestCachingPublic) {
  // In this test we'll leave the explicit "public" flag in to make sure
  // we can parse it.
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Cache-control: public, max-age=300\r\n\r\n");
  EXPECT_TRUE(meta_data_.IsCacheable());
  EXPECT_TRUE(meta_data_.IsProxyCacheable());
  EXPECT_EQ(300 * 1000,
            meta_data_.CacheExpirationTimeMs() - meta_data_.timestamp_ms());
}

// Private caching
TEST_F(SimpleMetaDataTest, TestCachingPrivate) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Cache-control: private, max-age=10\r\n\r\n");
  EXPECT_TRUE(meta_data_.IsCacheable());
  EXPECT_FALSE(meta_data_.IsProxyCacheable());
  EXPECT_EQ(10 * 1000,
            meta_data_.CacheExpirationTimeMs() - meta_data_.timestamp_ms());
}

// Default caching (when in doubt, it's public)
TEST_F(SimpleMetaDataTest, TestCachingDefault) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Cache-control: max-age=100\r\n\r\n");
  EXPECT_TRUE(meta_data_.IsCacheable());
  EXPECT_TRUE(meta_data_.IsProxyCacheable());
  EXPECT_EQ(100 * 1000,
            meta_data_.CacheExpirationTimeMs() - meta_data_.timestamp_ms());
}

// Test that we don't erroneously cache a 204.
TEST_F(SimpleMetaDataTest, TestCachingInvalidStatus) {
  ParseHeaders("HTTP/1.0 204 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Cache-control: max-age=300\r\n\r\n");
  EXPECT_FALSE(meta_data_.IsCacheable());
}

// Test that we don't cache an HTML file without explicit caching, but
// that we do cache images, css, and javascript.
TEST_F(SimpleMetaDataTest, TestImplicitCache) {
  EXPECT_FALSE(ComputeImplicitCaching(200, "text/html"));
  EXPECT_FALSE(ComputeImplicitCaching(200, "unknown"));
  EXPECT_TRUE(ComputeImplicitCaching(200, "text/javascript"));
  EXPECT_TRUE(ComputeImplicitCaching(200, "text/css"));
  EXPECT_TRUE(ComputeImplicitCaching(200, "image/jpeg"));
  EXPECT_TRUE(ComputeImplicitCaching(200, "image/gif"));
  EXPECT_TRUE(ComputeImplicitCaching(200, "image/png"));

  EXPECT_FALSE(ComputeImplicitCaching(204, "text/html"));
  EXPECT_FALSE(ComputeImplicitCaching(204, "unknown"));
  EXPECT_FALSE(ComputeImplicitCaching(204, "text/javascript"));
  EXPECT_FALSE(ComputeImplicitCaching(204, "text/css"));
  EXPECT_FALSE(ComputeImplicitCaching(204, "image/jpeg"));
  EXPECT_FALSE(ComputeImplicitCaching(204, "image/gif"));
  EXPECT_FALSE(ComputeImplicitCaching(204, "image/png"));
}

TEST_F(SimpleMetaDataTest, TestRemoveAll) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Set-Cookie: CG=US:CA:Mountain+View\r\n"
               "Set-Cookie: UA=chrome\r\n"
               "Cache-Control: max-age=100\r\n"
               "Set-Cookie: path=/\r\n"
               "Vary: User-Agent\r\n"
               "Set-Cookie: LA=1275937193\r\n"
               "Vary: Accept-Encoding\r\n"
               "\r\n");
  ExpectSizes(8, 4);
  meta_data_.RemoveAll("Vary");
  ExpectSizes(6, 3);
  meta_data_.RemoveAll(HttpAttributes::kSetCookie);
  ExpectSizes(2, 2);
  EXPECT_EQ(2, meta_data_.NumAttributes());
  meta_data_.RemoveAll("Date");
  ExpectSizes(1, 1);
  meta_data_.RemoveAll(HttpAttributes::kCacheControl);
  ExpectSizes(0, 0);
}

TEST_F(SimpleMetaDataTest, TestReasonPhrase) {
  meta_data_.SetStatusAndReason(HttpStatus::kOK);
  EXPECT_EQ(HttpStatus::kOK, meta_data_.status_code());
  EXPECT_EQ(std::string("OK"), std::string(meta_data_.reason_phrase()));
}

TEST_F(SimpleMetaDataTest, TestSetDate) {
  meta_data_.SetStatusAndReason(HttpStatus::kOK);
  meta_data_.SetDate(MockTimer::kApr_5_2010_ms);
  meta_data_.Add(HttpAttributes::kCacheControl, "max-age=100");
  CharStarVector date;
  ASSERT_TRUE(meta_data_.Lookup("Date", &date));
  EXPECT_EQ(1, date.size());
  meta_data_.ComputeCaching();
  const int64 k100_sec = 100 * 1000;
  ASSERT_EQ(MockTimer::kApr_5_2010_ms + k100_sec,
            meta_data_.CacheExpirationTimeMs());
}

}  // namespace net_instaweb
