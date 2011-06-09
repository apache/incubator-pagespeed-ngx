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

// Unit-test SimpleUrlData, in particular it's HTTP header parser.

#include "net/instaweb/http/public/response_headers.h"

#include <cstddef>                     // for size_t
#include <algorithm>
#include "net/instaweb/http/public/meta_data.h"  // for HttpAttributes
#include "net/instaweb/http/public/response_headers_parser.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

class ResponseHeadersTest : public testing::Test {
 protected:
  ResponseHeadersTest() : parser_(&response_headers_) { }

  void CheckGoogleHeaders(const ResponseHeaders& response_headers) {
    EXPECT_EQ(200, response_headers.status_code());
    EXPECT_EQ(1, response_headers.major_version());
    EXPECT_EQ(0, response_headers.minor_version());
    EXPECT_EQ(GoogleString("OK"),
              GoogleString(response_headers.reason_phrase()));
    StringStarVector values;
    EXPECT_TRUE(response_headers.Lookup("X-Google-Experiment", &values));
    EXPECT_EQ(GoogleString("23729,24249,24253"), *(values[0]));
    EXPECT_TRUE(response_headers.Lookup(HttpAttributes::kSetCookie, &values));
    EXPECT_EQ(2, values.size());
    EXPECT_EQ(GoogleString("PREF=ID=3935f510d83d2a7a:TM=1270493386:LM=127049338"
                           "6:S=u_18e6r8aJ83N6P1; "
                           "expires=Wed, 04-Apr-2012 18:49:46 GMT; path=/; "
                           "domain=.google.com"),
              *(values[0]));
    EXPECT_EQ(GoogleString("NID=33=aGkk7cKzznoUuCd19qTgXlBjXC8fc_luIo2Yk9BmrevU"
                           "gXYPTazDF8Q6JvsO6LvTu4mfI8_44iIBLu4pF-Mvpe4wb7pYwej"
                           "4q9HvbMLRxt-OzimIxmd-bwyYVfZ2PY1B; "
                           "expires=Tue, 05-Oct-2010 18:49:46 GMT; path=/; "
                           "domain=.google.com; HttpOnly"),
              *(values[1]));
    EXPECT_EQ(15, response_headers.NumAttributes());
    EXPECT_EQ(GoogleString("X-Google-GFE-Response-Body-Transformations"),
              GoogleString(response_headers.Name(14)));
    EXPECT_EQ(GoogleString("gunzipped"),
              GoogleString(response_headers.Value(14)));
  }

  void ParseHeaders(const StringPiece& headers) {
    parser_.Clear();
    parser_.ParseChunk(headers, &message_handler_);
  }

  // Check sizes of the header vector and map.
  void ExpectSizes(int num_headers, int num_header_names) {
    EXPECT_EQ(num_headers, response_headers_.NumAttributes());
    EXPECT_EQ(num_header_names, response_headers_.NumAttributeNames());
  }

  bool ComputeImplicitCaching(int status_code, const char* content_type) {
    GoogleString header_text =
        StringPrintf("HTTP/1.0 %d OK\r\n"
                     "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
                     "Content-type: %s\r\n\r\n",
                     status_code, content_type);
    response_headers_.Clear();
    ParseHeaders(header_text);
    return response_headers_.IsCacheable();
  }

  // At the end of every test, check to make sure that clearing the
  // meta-data produces an equivalent structure to a freshly initiliazed
  // one.
  virtual void TearDown() {
    response_headers_.Clear();
    ResponseHeaders empty_response_headers;

    // TODO(jmarantz): at present we lack a comprehensive serialization
    // that covers all the member variables, but at least we can serialize
    // to an HTTP-compatible string.
    EXPECT_EQ(empty_response_headers.ToString(), response_headers_.ToString());
  }

  GoogleMessageHandler message_handler_;
  ResponseHeaders response_headers_;
  ResponseHeadersParser parser_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResponseHeadersTest);
};

// Parse the headers from google.com
TEST_F(ResponseHeadersTest, TestParseAndWrite) {
  static const char http_data[] =
      "HTTP/1.0 200 OK\r\n"
      "X-Google-Experiment: 23729,24249,24253\r\n"
      "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
      "Expires: -1\r\n"
      "Cache-Control: private, max-age=0\r\n"
      "Content-Type: text/html; charset=ISO-8859-1\r\n"
      "X-Google-GFE-Backend-Request-Info: eid=yjC6S9qRCYmUnAe_mKVd\r\n"
      "Set-Cookie: PREF=ID=3935f510d83d2a7a:TM=1270493386:LM=1270493386:S="
      "u_18e6r8aJ83N6P1; expires=Wed, 04-Apr-2012 18:49:46 GMT; path=/; do"
      "main=.google.com\r\n"
      "Set-Cookie: NID=33=aGkk7cKzznoUuCd19qTgXlBjXC8fc_luIo2Yk9BmrevUgXYP"
      "TazDF8Q6JvsO6LvTu4mfI8_44iIBLu4pF-Mvpe4wb7pYwej4q9HvbMLRxt-OzimIxmd"
      "-bwyYVfZ2PY1B; expires=Tue, 05-Oct-2010 18:49:46 GMT; path=/; domai"
      "n=.google.com; HttpOnly\r\n"
      "Server: gws\r\n"
      "X-XSS-Protection: 0\r\n"
      "X-Google-Backends: /bns/ib/borg/ib/bns/gws-prod/staticweb.staticfro"
      "ntend.gws/50,qyva4:80\r\n"
      "X-Google-GFE-Request-Trace: qyva4:80,/bns/ib/borg/ib/bns/gws-prod/s"
      "taticweb.staticfrontend.gws/50,qyva4:80\r\n"
      "X-Google-GFE-Service-Trace: home\r\n"
      "X-Google-Service: home\r\n"
      "X-Google-GFE-Response-Body-Transformations: gunzipped\r\n"
      "\r\n"
      "<!doctype html><html><head><meta http-equiv=\"content-type\" content=\"";

  // Make a small buffer to test that we will successfully parse headers
  // that are split across buffers.  This is from
  //     wget --save-headers http://www.google.com
  const int bufsize = 100;
  int num_consumed = 0;
  for (int i = 0, n = STATIC_STRLEN(http_data); i < n; i += bufsize) {
    int size = std::min(bufsize, n - i);
    num_consumed += parser_.ParseChunk(StringPiece(http_data + i, size),
                                       &message_handler_);
    if (parser_.headers_complete()) {
      break;
    }
  }

  // Verifies that after the headers, we see the content.  Note that this
  // test uses 'wget' style output, and wget takes care of any unzipping,
  // so this should not be mistaken for a content decoder, such as the
  // net/instaweb/latencylabs/http_response_serializer.h.
  static const char start_of_doc[] = "<!doctype html>";
  EXPECT_EQ(0, strncmp(start_of_doc, http_data + num_consumed,
                       STATIC_STRLEN(start_of_doc)));
  CheckGoogleHeaders(response_headers_);

  // Now write the headers into a string.
  GoogleString outbuf;
  StringWriter writer(&outbuf);
  response_headers_.WriteAsHttp(&writer, &message_handler_);

  // Re-read into a fresh meta-data object and parse again.
  ResponseHeaders response_headers2;
  ResponseHeadersParser parser2(&response_headers2);
  num_consumed = parser2.ParseChunk(outbuf, &message_handler_);
  EXPECT_EQ(outbuf.size(), static_cast<size_t>(num_consumed));
  CheckGoogleHeaders(response_headers2);

  // Write the headers as binary into a string.
  outbuf.clear();
  response_headers_.WriteAsBinary(&writer, &message_handler_);

  // Re-read into a fresh meta-data object and compare.
  ResponseHeaders response_headers3;
  ASSERT_TRUE(response_headers3.ReadFromBinary(outbuf, &message_handler_));
  CheckGoogleHeaders(response_headers3);
}

// Test caching header interpretation.  Note that the detailed testing
// of permutations is done in pagespeed/core/resource_util_test.cc.  We
// are just trying to ensure that we have populated the Resource object
// properly and that we have extracted the bits we need.
TEST_F(ResponseHeadersTest, TestCachingNeedDate) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Cache-control: max-age=300\r\n\r\n");
  EXPECT_FALSE(response_headers_.IsCacheable());
  EXPECT_EQ(0, response_headers_.CacheExpirationTimeMs());
}

TEST_F(ResponseHeadersTest, TestCachingPublic) {
  // In this test we'll leave the explicit "public" flag in to make sure
  // we can parse it.
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Cache-control: public, max-age=300\r\n\r\n");
  EXPECT_TRUE(response_headers_.IsCacheable());
  EXPECT_TRUE(response_headers_.IsProxyCacheable());
  EXPECT_EQ(300 * 1000,
            response_headers_.CacheExpirationTimeMs() -
            response_headers_.timestamp_ms());
}

// Private caching
TEST_F(ResponseHeadersTest, TestCachingPrivate) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Cache-control: private, max-age=10\r\n\r\n");
  EXPECT_TRUE(response_headers_.IsCacheable());
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
  EXPECT_EQ(10 * 1000,
            response_headers_.CacheExpirationTimeMs() -
            response_headers_.timestamp_ms());
}

// Default caching (when in doubt, it's public)
TEST_F(ResponseHeadersTest, TestCachingDefault) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Cache-control: max-age=100\r\n\r\n");
  EXPECT_TRUE(response_headers_.IsCacheable());
  EXPECT_TRUE(response_headers_.IsProxyCacheable());
  EXPECT_EQ(100 * 1000,
            response_headers_.CacheExpirationTimeMs() -
            response_headers_.timestamp_ms());
}

// Test that we don't erroneously cache a 204.
TEST_F(ResponseHeadersTest, TestCachingInvalidStatus) {
  ParseHeaders("HTTP/1.0 204 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Cache-control: max-age=300\r\n\r\n");
  EXPECT_FALSE(response_headers_.IsCacheable());
}

// Test that we don't cache an HTML file without explicit caching, but
// that we do cache images, css, and javascript.
TEST_F(ResponseHeadersTest, TestImplicitCache) {
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

TEST_F(ResponseHeadersTest, TestRemoveAll) {
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
  response_headers_.RemoveAll(HttpAttributes::kVary);
  ExpectSizes(6, 3);
  response_headers_.RemoveAll(HttpAttributes::kSetCookie);
  ExpectSizes(2, 2);
  EXPECT_EQ(2, response_headers_.NumAttributes());
  response_headers_.RemoveAll(HttpAttributes::kDate);
  ExpectSizes(1, 1);
  response_headers_.RemoveAll(HttpAttributes::kCacheControl);
  ExpectSizes(0, 0);
}

TEST_F(ResponseHeadersTest, TestReasonPhrase) {
  response_headers_.SetStatusAndReason(HttpStatus::kOK);
  EXPECT_EQ(HttpStatus::kOK, response_headers_.status_code());
  EXPECT_EQ(GoogleString("OK"),
            GoogleString(response_headers_.reason_phrase()));
}

TEST_F(ResponseHeadersTest, TestSetDate) {
  response_headers_.SetStatusAndReason(HttpStatus::kOK);
  response_headers_.SetDate(MockTimer::kApr_5_2010_ms);
  response_headers_.Add(HttpAttributes::kCacheControl, "max-age=100");
  StringStarVector date;
  ASSERT_TRUE(response_headers_.Lookup("Date", &date));
  EXPECT_EQ(1, date.size());
  response_headers_.ComputeCaching();
  const int64 k100_sec = 100 * 1000;
  ASSERT_EQ(MockTimer::kApr_5_2010_ms + k100_sec,
            response_headers_.CacheExpirationTimeMs());
}

TEST_F(ResponseHeadersTest, TestUpdateFrom) {
  const char old_header_string[] =
      "HTTP/1.1 200 OK\r\n"
      "Date: Fri, 22 Apr 2011 19:34:33 GMT\r\n"
      "Server: Apache/2.2.3 (CentOS)\r\n"
      "Last-Modified: Tue, 08 Mar 2011 18:28:32 GMT\r\n"
      "Accept-Ranges: bytes\r\n"
      "Content-Length: 241260\r\n"
      "Cache-control: public, max-age=600\r\n"
      "Content-Type: image/jpeg\r\n"
      "\r\n";
  const char new_header_string[] =
      "HTTP/1.1 304 Not Modified\r\n"
      "Date: Fri, 22 Apr 2011 19:49:59 GMT\r\n"
      "Server: Apache/2.2.3 (CentOS)\r\n"
      "Cache-control: public, max-age=3600\r\n"
      "Set-Cookie: LA=1275937193\r\n"
      "Set-Cookie: UA=chrome\r\n"
      "\r\n";
  const char expected_merged_header_string[] =
      "HTTP/1.1 200 OK\r\n"
      "Last-Modified: Tue, 08 Mar 2011 18:28:32 GMT\r\n"
      "Accept-Ranges: bytes\r\n"
      "Content-Length: 241260\r\n"
      "Content-Type: image/jpeg\r\n"
      "Date: Fri, 22 Apr 2011 19:49:59 GMT\r\n"
      "Server: Apache/2.2.3 (CentOS)\r\n"
      "Cache-control: public, max-age=3600\r\n"
      "Set-Cookie: LA=1275937193\r\n"
      "Set-Cookie: UA=chrome\r\n"
      "\r\n";

  // Setup old and new headers
  ResponseHeaders old_headers, new_headers;
  ResponseHeadersParser old_parser(&old_headers), new_parser(&new_headers);
  old_parser.ParseChunk(old_header_string, &message_handler_);
  new_parser.ParseChunk(new_header_string, &message_handler_);

  // Update old_headers from new_headers.
  old_headers.UpdateFrom(new_headers);

  // Make sure in memory map is updated.
  StringStarVector date_strings;
  EXPECT_TRUE(old_headers.Lookup("Date", &date_strings));
  EXPECT_EQ(1, date_strings.size());
  EXPECT_EQ("Fri, 22 Apr 2011 19:49:59 GMT", *date_strings[0]);
  StringStarVector set_cookie_strings;
  EXPECT_TRUE(old_headers.Lookup("Set-Cookie", &set_cookie_strings));
  EXPECT_EQ(8, old_headers.NumAttributeNames());

  // Make sure protobuf is updated.
  GoogleString actual_merged_header_string;
  StringWriter merged_writer(&actual_merged_header_string);
  old_headers.WriteAsHttp(&merged_writer, &message_handler_);

  EXPECT_EQ(expected_merged_header_string, actual_merged_header_string);
}

//Make sure resources that vary user-agent and cookie don't get cached.
TEST_F(ResponseHeadersTest, TestCachingVary) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Cache-control: public, max-age=300\r\n"
               "Vary: User-Agent\r\n\r\n\r\n");

  EXPECT_FALSE(response_headers_.IsCacheable());
}

TEST_F(ResponseHeadersTest, TestCachingVaryCookie) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Cache-control: public, max-age=300\r\n"
               "Vary: Cookie\t\r\n\r\n\r\n");
  EXPECT_FALSE(response_headers_.IsCacheable());
}

TEST_F(ResponseHeadersTest, TestCachingVaryBoth) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Cache-control: public, max-age=300\r\n"
               "Vary: Accept-Encoding, Cookie\r\n\r\n\r\n");
  EXPECT_FALSE(response_headers_.IsCacheable());
}

TEST_F(ResponseHeadersTest, TestCachingVaryStar) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Cache-control: public, max-age=300\r\n"
               "Vary: *\r\n\r\n\r\n");
  EXPECT_FALSE(response_headers_.IsCacheable());
}

TEST_F(ResponseHeadersTest, TestCachingVaryNegative) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Cache-control: public, max-age=300\r\n"
               "Vary: Accept-Encoding\r\n\r\n\r\n");
  EXPECT_TRUE(response_headers_.IsCacheable());
}

TEST_F(ResponseHeadersTest, TestCachingVarySpaces) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Date: Mon, 05 Apr 2010 18:49:46 GMT\r\n"
               "Cache-control: public, max-age=300\r\n"
               "Vary: Accept-Encoding, ,\r\n\r\n\r\n");
  EXPECT_TRUE(response_headers_.IsCacheable());
}

TEST_F(ResponseHeadersTest, TestSetDateAndCaching) {
  response_headers_.SetDateAndCaching(MockTimer::kApr_5_2010_ms,
                                      6 * Timer::kMinuteMs);
  const char expected_headers[] =
      "HTTP/1.0 0 (null)\r\n"
      "Date: Mon, 05 Apr 2010 18:51:26 GMT\r\n"
      "Expires: Mon, 05 Apr 2010 18:57:26 GMT\r\n"
      "Cache-Control: max-age=360\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers, response_headers_.ToString());
}

}  // namespace net_instaweb
