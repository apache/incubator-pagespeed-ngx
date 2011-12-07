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
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"  // for HttpAttributes
#include "net/instaweb/http/public/response_headers_parser.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/timer.h"  // for Timer

namespace net_instaweb {

class ResponseHeadersTest : public testing::Test {
 protected:
  ResponseHeadersTest()
      : parser_(&response_headers_),
        max_age_300_("max-age=300") {
    ConvertTimeToString(MockTimer::kApr_5_2010_ms, &start_time_string_);
    ConvertTimeToString(MockTimer::kApr_5_2010_ms + 5 * Timer::kMinuteMs,
                        &start_time_plus_5_minutes_string_);
    ConvertTimeToString(MockTimer::kApr_5_2010_ms + 6 * Timer::kMinuteMs,
                        &start_time_plus_6_minutes_string_);
  }

  void CheckGoogleHeaders(const ResponseHeaders& response_headers) {
    EXPECT_EQ(200, response_headers.status_code());
    EXPECT_EQ(1, response_headers.major_version());
    EXPECT_EQ(0, response_headers.minor_version());
    EXPECT_EQ(GoogleString("OK"),
              GoogleString(response_headers.reason_phrase()));
    ConstStringStarVector values;
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
    EXPECT_EQ(12, response_headers.NumAttributes());
    EXPECT_EQ(GoogleString("X-Google-GFE-Response-Body-Transformations"),
              GoogleString(response_headers.Name(11)));
    EXPECT_EQ(GoogleString("gunzipped"),
              GoogleString(response_headers.Value(11)));
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
                     "Date: %s\r\n"
                     "Content-type: %s\r\n\r\n",
                     status_code, start_time_string_.c_str(), content_type);
    response_headers_.Clear();
    ParseHeaders(header_text);
    bool cacheable = response_headers_.IsCacheable();
    if (!cacheable) {
      EXPECT_EQ(NULL, response_headers_.Lookup1(HttpAttributes::kCacheControl));
      EXPECT_EQ(NULL, response_headers_.Lookup1(HttpAttributes::kExpires));
    } else {
      EXPECT_STREQ(max_age_300_,
                   response_headers_.Lookup1(HttpAttributes::kCacheControl));
      EXPECT_STREQ(start_time_plus_5_minutes_string_,
                   response_headers_.Lookup1(HttpAttributes::kExpires));
    }
    return cacheable;
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

  GoogleString start_time_string_;
  GoogleString start_time_plus_5_minutes_string_;
  GoogleString start_time_plus_6_minutes_string_;
  const GoogleString max_age_300_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResponseHeadersTest);
};

// Parse the headers from google.com
TEST_F(ResponseHeadersTest, TestParseAndWrite) {
  const GoogleString http_data = StrCat(
      "HTTP/1.0 200 OK\r\n"
      "X-Google-Experiment: 23729,24249,24253\r\n",
      "Date: ", start_time_string_, "\r\n",
      "Expires: -1\r\n"
      "Cache-Control: private, max-age=0\r\n"
      "Content-Type: text/html; charset=ISO-8859-1\r\n"
      "Set-Cookie: PREF=ID=3935f510d83d2a7a:TM=1270493386:LM=1270493386:S="
      "u_18e6r8aJ83N6P1; expires=Wed, 04-Apr-2012 18:49:46 GMT; path=/; do"
      "main=.google.com\r\n"
      "Set-Cookie: NID=33=aGkk7cKzznoUuCd19qTgXlBjXC8fc_luIo2Yk9BmrevUgXYP"
      "TazDF8Q6JvsO6LvTu4mfI8_44iIBLu4pF-Mvpe4wb7pYwej4q9HvbMLRxt-OzimIxmd"
      "-bwyYVfZ2PY1B; expires=Tue, 05-Oct-2010 18:49:46 GMT; path=/; domai"
      "n=.google.com; HttpOnly\r\n"
      "Server: gws\r\n"
      "X-XSS-Protection: 0\r\n"
      "ntend.gws/50,qyva4:80\r\n"
      "taticweb.staticfrontend.gws/50,qyva4:80\r\n"
      "X-Google-GFE-Response-Body-Transformations: gunzipped\r\n"
      "\r\n"
      "<!doctype html><html><head>"
      "<meta http-equiv=\"content-type\" content=\"");

  // Make a small buffer to test that we will successfully parse headers
  // that are split across buffers.  This is from
  //     wget --save-headers http://www.google.com
  const int bufsize = 100;
  int num_consumed = 0;
  for (int i = 0, n = http_data.size(); i < n; i += bufsize) {
    int size = std::min(bufsize, n - i);
    num_consumed += parser_.ParseChunk(StringPiece(http_data).substr(i, size),
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
  EXPECT_EQ(0, strncmp(start_of_doc, http_data.c_str() + num_consumed,
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

// Make sure we deal correctly when we have no Date or Cache-Control headers.
TEST_F(ResponseHeadersTest, TestNoHeaders) {
  ParseHeaders("HTTP/1.0 200 OK\r\n\r\n");
  EXPECT_FALSE(response_headers_.IsCacheable());
  EXPECT_EQ(0, response_headers_.CacheExpirationTimeMs());
}

// Corner case, bug noticed when we have Content-Type, but no Date header.
TEST_F(ResponseHeadersTest, TestNoContentTypeNoDate) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Content-Type: text/css\r\n\r\n");
  EXPECT_FALSE(response_headers_.IsCacheable());
  EXPECT_EQ(0, response_headers_.CacheExpirationTimeMs());
}

TEST_F(ResponseHeadersTest, TestNoContentTypeCacheNoDate) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Content-Type: text/css\r\n"
               "Cache-Control: max-age=301\r\n\r\n");
  EXPECT_FALSE(response_headers_.IsCacheable());
  EXPECT_EQ(0, response_headers_.CacheExpirationTimeMs());
}

TEST_F(ResponseHeadersTest, TestCachingPublic) {
  // In this test we'll leave the explicit "public" flag in to make sure
  // we can parse it.
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: public, max-age=300\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsCacheable());
  EXPECT_TRUE(response_headers_.IsProxyCacheable());
  EXPECT_EQ(300 * 1000,
            response_headers_.CacheExpirationTimeMs() -
            response_headers_.date_ms());
}

// Private caching
TEST_F(ResponseHeadersTest, TestCachingPrivate) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: private, max-age=10\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsCacheable());
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
  EXPECT_EQ(10 * 1000,
            response_headers_.CacheExpirationTimeMs() -
            response_headers_.date_ms());
}

// Default caching (when in doubt, it's public)
TEST_F(ResponseHeadersTest, TestCachingDefault) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: max-age=100\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsCacheable());
  EXPECT_TRUE(response_headers_.IsProxyCacheable());
  EXPECT_EQ(100 * 1000,
            response_headers_.CacheExpirationTimeMs() -
            response_headers_.date_ms());
}

// Test that we don't erroneously cache a 204 even though it is marked
// explicitly as cacheable. Note: We could cache this, but many status codes
// are only cacheable depending on precise input headers, to be cautious, we
// blacklist everything other than 200.
TEST_F(ResponseHeadersTest, TestCachingInvalidStatus) {
  ParseHeaders(StrCat("HTTP/1.0 204 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsCacheable());
}

// Test that we don't erroneously cache a 304.
// Note: Even though it claims to be publicly cacheable, that cacheability only
// applies to the response based on the precise request headers or it applies
// to the original 200 response.
TEST_F(ResponseHeadersTest, TestCachingNotModified) {
  ParseHeaders(StrCat("HTTP/1.0 304 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsCacheable());
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
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

TEST_F(ResponseHeadersTest, TestSetCookieCacheabilityForHtml) {
  // HTML is cacheable if there are explicit caching directives, but no
  // Set-Cookie headers.
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Content-Type: text/html\r\n"
                      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsCacheable());
  EXPECT_TRUE(response_headers_.IsProxyCacheable());

  response_headers_.Clear();
  // HTML is not cacheable if there is a Set-Cookie header even though there are
  // explicit caching directives.
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Content-Type: text/html\r\n"
                      "Set-Cookie: cookie\r\n"
                      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsCacheable());
  EXPECT_FALSE(response_headers_.IsProxyCacheable());

  response_headers_.Clear();
  // HTML is not cacheable if there is a Set-Cookie2 header even though there
  // are explicit caching directives.
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Content-Type: text/html\r\n"
                      "Set-Cookie2: cookie\r\n"
                      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsCacheable());
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
}

TEST_F(ResponseHeadersTest, TestSetCookieCacheabilityForNonHtml) {
  // CSS is cacheable if there are explicit caching directives, but no
  // Set-Cookie headers.
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Content-Type: text/css\r\n"
                      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsCacheable());
  EXPECT_TRUE(response_headers_.IsProxyCacheable());

  response_headers_.Clear();
  // CSS is still cacheable even if there is a Set-Cookie.
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Content-Type: text/css\r\n"
                      "Set-Cookie: cookie\r\n"
                      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsCacheable());
  EXPECT_TRUE(response_headers_.IsProxyCacheable());
}

TEST_F(ResponseHeadersTest, TestRemoveAll) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Set-Cookie: CG=US:CA:Mountain+View\r\n"
                      "Set-Cookie: UA=chrome\r\n"
                      "Cache-Control: max-age=100\r\n"
                      "Set-Cookie: path=/\r\n"
                      "Vary: User-Agent\r\n"
                      "Set-Cookie: LA=1275937193\r\n"
                      "Vary: Accept-Encoding\r\n"
                      "\r\n"));
  ConstStringStarVector vs;
  ExpectSizes(8, 4);

  // Removing a header which isn't there removes nothing and returns false.
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kLocation, &vs));
  EXPECT_FALSE(response_headers_.RemoveAll(HttpAttributes::kLocation));
  ExpectSizes(8, 4);

  // Removing a headers which is there works.
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &vs));
  EXPECT_TRUE(response_headers_.RemoveAll(HttpAttributes::kVary));
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kVary, &vs));
  ExpectSizes(6, 3);

  // Removing something which has already been removed has no effect.
  EXPECT_FALSE(response_headers_.RemoveAll(HttpAttributes::kVary));
  ExpectSizes(6, 3);

  // Remove the rest one-by-one.
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kSetCookie, &vs));
  EXPECT_TRUE(response_headers_.RemoveAll(HttpAttributes::kSetCookie));
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kSetCookie, &vs));
  ExpectSizes(2, 2);
  EXPECT_EQ(2, response_headers_.NumAttributes());

  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kDate, &vs));
  EXPECT_TRUE(response_headers_.RemoveAll(HttpAttributes::kDate));
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kDate, &vs));
  ExpectSizes(1, 1);

  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kCacheControl, &vs));
  EXPECT_TRUE(response_headers_.RemoveAll(HttpAttributes::kCacheControl));
  ExpectSizes(0, 0);
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kCacheControl, &vs));
}

TEST_F(ResponseHeadersTest, TestRemoveAllFromSet) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Set-Cookie: CG=US:CA:Mountain+View\r\n"
                      "Set-Cookie: UA=chrome\r\n"
                      "Cache-Control: max-age=100\r\n"
                      "Set-Cookie: path=/\r\n"
                      "Vary: User-Agent\r\n"
                      "Set-Cookie: LA=1275937193\r\n"
                      "Vary: Accept-Encoding\r\n"
                      "\r\n"));
  ConstStringStarVector vs;
  ExpectSizes(8, 4);

  // Empty set means remove nothing and return false.
  StringSetInsensitive removes0;
  EXPECT_FALSE(response_headers_.RemoveAllFromSet(removes0));
  ExpectSizes(8, 4);

  // Removing headers which aren't there removes nothing and returns false.
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kLocation, &vs));
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kGzip, &vs));
  removes0.insert(HttpAttributes::kLocation);
  removes0.insert(HttpAttributes::kGzip);
  EXPECT_FALSE(response_headers_.RemoveAllFromSet(removes0));
  ExpectSizes(8, 4);

  // Removing multiple headers works.
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &vs));
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kSetCookie, &vs));
  StringSetInsensitive removes1;
  removes1.insert(HttpAttributes::kVary);
  removes1.insert(HttpAttributes::kSetCookie);
  EXPECT_TRUE(response_headers_.RemoveAllFromSet(removes1));
  ExpectSizes(2, 2);
  EXPECT_EQ(2, response_headers_.NumAttributes());
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kVary, &vs));
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kSetCookie, &vs));

  // Removing something which has already been removed has no effect.
  EXPECT_FALSE(response_headers_.RemoveAllFromSet(removes1));
  ExpectSizes(2, 2);

  // Removing one header works.
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kDate, &vs));
  StringSetInsensitive removes2;
  removes2.insert(HttpAttributes::kDate);
  EXPECT_TRUE(response_headers_.RemoveAllFromSet(removes2));
  ExpectSizes(1, 1);
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kDate, &vs));

  // Removing a header that is there after one that isn't works.
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kCacheControl, &vs));
  StringSetInsensitive removes3;
  removes3.insert("X-Bogus-Attribute");
  removes3.insert(HttpAttributes::kCacheControl);
  EXPECT_TRUE(response_headers_.RemoveAllFromSet(removes3));
  ExpectSizes(0, 0);
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kCacheControl, &vs));
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
  ConstStringStarVector date;
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
  ConstStringStarVector date_strings;
  EXPECT_TRUE(old_headers.Lookup("Date", &date_strings));
  EXPECT_EQ(1, date_strings.size());
  EXPECT_EQ("Fri, 22 Apr 2011 19:49:59 GMT", *date_strings[0]);
  ConstStringStarVector set_cookie_strings;
  EXPECT_TRUE(old_headers.Lookup("Set-Cookie", &set_cookie_strings));
  EXPECT_EQ(8, old_headers.NumAttributeNames());

  // Make sure protobuf is updated.
  GoogleString actual_merged_header_string;
  StringWriter merged_writer(&actual_merged_header_string);
  old_headers.WriteAsHttp(&merged_writer, &message_handler_);

  EXPECT_EQ(expected_merged_header_string, actual_merged_header_string);
}

TEST_F(ResponseHeadersTest, TestCachingVaryStar) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: public, max-age=300\r\n"
                      "Vary: *\r\n\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsCacheable());
}

TEST_F(ResponseHeadersTest, TestSetDateAndCaching) {
  response_headers_.SetDateAndCaching(MockTimer::kApr_5_2010_ms,
                                      6 * Timer::kMinuteMs);
  const GoogleString expected_headers = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360\r\n"
      "\r\n");
  EXPECT_EQ(expected_headers, response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, TestReserializingCommaValues) {
  const GoogleString comma_headers = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360, private, must-revalidate\r\n"
      "Vary: Accept-Encoding, User-Agent\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(comma_headers);
  ConstStringStarVector values;
  response_headers_.Lookup(HttpAttributes::kCacheControl, &values);
  EXPECT_EQ(3, values.size());
  values.clear();
  response_headers_.Lookup(HttpAttributes::kVary, &values);
  EXPECT_EQ(2, values.size());
  EXPECT_EQ(comma_headers, response_headers_.ToString());
}

// There was a bug that calling RemoveAll would re-populate the proto from
// map_ which would separate all comma-separated values.
TEST_F(ResponseHeadersTest, TestRemoveDoesntSeparateCommaValues) {
  response_headers_.Add(HttpAttributes::kCacheControl, "max-age=0, no-cache");
  response_headers_.Add(HttpAttributes::kSetCookie, "blah");
  response_headers_.Add(HttpAttributes::kVary, "Accept-Encoding, Cookie");

  // 1) RemoveAll
  EXPECT_TRUE(response_headers_.RemoveAll(HttpAttributes::kSetCookie));

  ConstStringStarVector values;
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kCacheControl, &values));
  EXPECT_EQ(2, values.size());
  values.clear();
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &values));
  EXPECT_EQ(2, values.size());

  const char expected_headers[] =
      "HTTP/1.0 0 (null)\r\n"
      "Cache-Control: max-age=0, no-cache\r\n"
      "Vary: Accept-Encoding, Cookie\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers, response_headers_.ToString());

  // 2) Remove
  EXPECT_TRUE(response_headers_.Remove(HttpAttributes::kVary, "Cookie"));

  const char expected_headers2[] =
      "HTTP/1.0 0 (null)\r\n"
      "Cache-Control: max-age=0, no-cache\r\n"
      "Vary: Accept-Encoding\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers2, response_headers_.ToString());

  // 3) RemoveAllFromSet
  StringSetInsensitive set;
  set.insert(HttpAttributes::kVary);
  EXPECT_TRUE(response_headers_.RemoveAllFromSet(set));

  const char expected_headers3[] =
      "HTTP/1.0 0 (null)\r\n"
      "Cache-Control: max-age=0, no-cache\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers3, response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, TestKeepSeparateCommaValues) {
  response_headers_.Add(HttpAttributes::kVary, "Accept-Encoding");
  response_headers_.Add(HttpAttributes::kVary, "User-Agent");
  response_headers_.Add(HttpAttributes::kVary, "Cookie");

  ConstStringStarVector values;
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &values));
  EXPECT_EQ(3, values.size());

  // We keep values separate by default.
  const char expected_headers[] =
      "HTTP/1.0 0 (null)\r\n"
      "Vary: Accept-Encoding\r\n"
      "Vary: User-Agent\r\n"
      "Vary: Cookie\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers, response_headers_.ToString());

  EXPECT_TRUE(response_headers_.Remove(HttpAttributes::kVary, "User-Agent"));

  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &values));
  EXPECT_EQ(2, values.size());

  // But they are combined after a Remove.
  //
  // NOTE: This is mostly to document current behavior. Feel free to re-gold
  // this if you update the Remove method to not combine headers.
  const char expected_headers2[] =
      "HTTP/1.0 0 (null)\r\n"
      "Vary: Accept-Encoding, Cookie\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers2, response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, TestKeepTogetherCommaValues) {
  response_headers_.Add(HttpAttributes::kVary,
                        "Accept-Encoding, User-Agent, Cookie");

  ConstStringStarVector values;
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &values));
  EXPECT_EQ(3, values.size());

  const char expected_headers[] =
      "HTTP/1.0 0 (null)\r\n"
      "Vary: Accept-Encoding, User-Agent, Cookie\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers, response_headers_.ToString());

  EXPECT_TRUE(response_headers_.Remove(HttpAttributes::kVary, "User-Agent"));

  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &values));
  EXPECT_EQ(2, values.size());

  const char expected_headers2[] =
      "HTTP/1.0 0 (null)\r\n"
      "Vary: Accept-Encoding, Cookie\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers2, response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, TestGzipped) {
  const GoogleString comma_headers = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360\r\n"
      "Content-Encoding: deflate, gzip\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(comma_headers);
  ConstStringStarVector values;
  response_headers_.Lookup(HttpAttributes::kContentEncoding, &values);
  EXPECT_EQ(2, values.size());
  EXPECT_TRUE(response_headers_.IsGzipped());
  EXPECT_TRUE(response_headers_.WasGzippedLast());
}

TEST_F(ResponseHeadersTest, TestGzippedNotLast) {
  const GoogleString comma_headers = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360\r\n"
      "Content-Encoding: gzip, deflate\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(comma_headers);
  ConstStringStarVector values;
  response_headers_.Lookup(HttpAttributes::kContentEncoding, &values);
  EXPECT_EQ(2, values.size());
  EXPECT_TRUE(response_headers_.IsGzipped());
  EXPECT_FALSE(response_headers_.WasGzippedLast());
}

TEST_F(ResponseHeadersTest, TestRemove) {
  const GoogleString headers = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360\r\n"
      "Content-Encoding: chunked, deflate, chunked, gzip\r\n"
      "\r\n");
  const GoogleString headers_removed = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360\r\n"
      "Content-Encoding: chunked, deflate, gzip\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(headers);
  EXPECT_TRUE(response_headers_.Remove(HttpAttributes::kContentEncoding,
                                       "chunked"));
  EXPECT_EQ(headers_removed, response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, TestRemoveConcat) {
  const GoogleString headers = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Content-Encoding: gzip\r\n"
      "\r\n");
  const GoogleString headers_removed = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(headers);
  EXPECT_TRUE(response_headers_.Remove(HttpAttributes::kContentEncoding,
                                       "gzip"));
  EXPECT_EQ(headers_removed, response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, TestParseFirstLineOk) {
  response_headers_.ParseFirstLine("HTTP/1.0 200 OK");
  EXPECT_EQ(1, response_headers_.major_version());
  EXPECT_EQ(0, response_headers_.minor_version());
  EXPECT_EQ(200, response_headers_.status_code());
  EXPECT_EQ(GoogleString("OK"),
            GoogleString(response_headers_.reason_phrase()));
}

TEST_F(ResponseHeadersTest, TestParseFirstLinePermanentRedirect) {
  response_headers_.ParseFirstLine("HTTP/1.1 301 Moved Permanently");
  EXPECT_EQ(1, response_headers_.major_version());
  EXPECT_EQ(1, response_headers_.minor_version());
  EXPECT_EQ(301, response_headers_.status_code());
  EXPECT_EQ(GoogleString("Moved Permanently"),
            GoogleString(response_headers_.reason_phrase()));
}

TEST_F(ResponseHeadersTest, RemoveAllCaseInsensitivity) {
  ResponseHeaders headers;
  headers.Add("content-encoding", "gzip");
  EXPECT_STREQ("gzip", headers.Lookup1("Content-Encoding"));
  headers.RemoveAll("Content-Encoding");
  EXPECT_EQ(NULL, headers.Lookup1("content-encoding"));
  EXPECT_EQ(NULL, headers.Lookup1("Content-Encoding"));
  EXPECT_EQ(0, headers.NumAttributes()) << headers.Name(0);
}

TEST_F(ResponseHeadersTest, DetermineContentType) {
  static const char headers[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: image/png\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(headers);
  EXPECT_EQ(&kContentTypePng, response_headers_.DetermineContentType());
}

TEST_F(ResponseHeadersTest, DetermineContentTypeWithCharset) {
  static const char headers[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(headers);
  EXPECT_EQ(&kContentTypeHtml, response_headers_.DetermineContentType());
}

TEST_F(ResponseHeadersTest, DetermineCharset) {
  static const char headers_no_charset[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: image/png\r\n"
      "Content-Type: image/png\r\n"
      "Content-Type: image/png\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(headers_no_charset);
  EXPECT_TRUE(response_headers_.DetermineCharset().empty());

  static const char headers_with_charset[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: image/png\r\n"
      "Content-Type: image/png; charset=utf-8\r\n"
      "Content-Type: image/png\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(headers_with_charset);
  EXPECT_EQ("utf-8", response_headers_.DetermineCharset());

  // We take the first charset specified.
  static const char multiple_headers_with_charset[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: image/png\r\n"
      "Content-Type: image/png; charset=iso-8859-1\r\n"
      "Content-Type: image/png\r\n"
      "Content-Type: image/png; charset=utf-8\r\n"
      "Content-Type: image/png\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(multiple_headers_with_charset);
  EXPECT_EQ("iso-8859-1", response_headers_.DetermineCharset());
}

TEST_F(ResponseHeadersTest, FixupMissingDate) {
  static const char headers[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(headers);
  response_headers_.FixDateHeaders(MockTimer::kApr_5_2010_ms);
  response_headers_.ComputeCaching();
  EXPECT_EQ(MockTimer::kApr_5_2010_ms, response_headers_.date_ms());
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) == NULL);
}

TEST_F(ResponseHeadersTest, DoNotCorrectValidDate) {
  const GoogleString headers = StrCat(
      "HTTP/1.1 200 OK\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(headers);
  response_headers_.ComputeCaching();

  // Setting clock back by 1 second will not affect the date.
  int64 prev_date = response_headers_.date_ms();
  response_headers_.FixDateHeaders(prev_date - 1000);
  EXPECT_EQ(prev_date, response_headers_.date_ms());
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) == NULL);
}

TEST_F(ResponseHeadersTest, FixupStaleDate) {
  const GoogleString headers = StrCat(
      "HTTP/1.1 200 OK\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(headers);
  response_headers_.ComputeCaching();

  // Setting clock *forward* by 1 second *will* affect the date.
  int64 new_date = response_headers_.date_ms() + 1000;
  response_headers_.FixDateHeaders(new_date);
  EXPECT_EQ(new_date, response_headers_.date_ms());
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) == NULL);
}

TEST_F(ResponseHeadersTest, FixupStaleDateWithExpires) {
  const GoogleString headers = StrCat(
      "HTTP/1.1 200 OK\r\n"
      "Date:    ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_5_minutes_string_, "\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(headers);
  response_headers_.ComputeCaching();

  // Setting clock *forward* by 1 second *will* affect the date, and
  // also push the Expires along with it.
  int64 orig_date = response_headers_.date_ms();
  ASSERT_EQ(orig_date + 5 * Timer::kMinuteMs,
            response_headers_.CacheExpirationTimeMs());
  int64 new_date = orig_date + 1000;

  response_headers_.FixDateHeaders(new_date);
  EXPECT_EQ(new_date, response_headers_.date_ms());
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) != NULL);
  EXPECT_EQ(new_date + 5 * Timer::kMinuteMs,
            response_headers_.CacheExpirationTimeMs());
}

TEST_F(ResponseHeadersTest, FixupStaleDateWithMaxAge) {
  const GoogleString headers = StrCat(
      "HTTP/1.1 200 OK\r\n"
      "Date:    ", start_time_string_, "\r\n"
      "Cache-Control: max-age=300\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(headers);
  response_headers_.ComputeCaching();

  // Setting clock *forward* by 1 second *will* affect the date, and
  // also push the Expires along with it.
  int64 orig_date = response_headers_.date_ms();
  ASSERT_EQ(orig_date + 5 * Timer::kMinuteMs,
            response_headers_.CacheExpirationTimeMs());
  ASSERT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) == NULL);
  int64 new_date = orig_date + 1000;

  response_headers_.FixDateHeaders(new_date);
  EXPECT_EQ(new_date, response_headers_.date_ms());

  // Still no Expires entry, but the cache expiration time is still 5 minutes.
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) == NULL);
  EXPECT_EQ(new_date + 5 * Timer::kMinuteMs,
            response_headers_.CacheExpirationTimeMs());
}

TEST_F(ResponseHeadersTest, MissingDateRemoveExpires) {
  const GoogleString headers = StrCat(
      "HTTP/1.1 200 OK\r\n"
      "Expires: ", start_time_plus_5_minutes_string_, "\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(headers);
  response_headers_.ComputeCaching();

  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kDate) == NULL);
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) != NULL);
  response_headers_.FixDateHeaders(MockTimer::kApr_5_2010_ms);
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kDate) != NULL);
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) == NULL);
}

TEST_F(ResponseHeadersTest, TestSetCacheControlMaxAge) {
  response_headers_.SetStatusAndReason(HttpStatus::kOK);
  response_headers_.SetDate(MockTimer::kApr_5_2010_ms);
  response_headers_.Add(HttpAttributes::kCacheControl, "max-age=0, no-cache");
  response_headers_.ComputeCaching();

  response_headers_.SetCacheControlMaxAge(300000);

  const GoogleString expected_headers = StrCat(
      "HTTP/1.0 200 OK\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_5_minutes_string_, "\r\n"
      "Cache-Control: max-age=300,no-cache\r\n"
      "\r\n");
  EXPECT_EQ(expected_headers, response_headers_.ToString());

  response_headers_.RemoveAll(HttpAttributes::kCacheControl);
  response_headers_.ComputeCaching();

  response_headers_.SetCacheControlMaxAge(360000);
  GoogleString expected_headers2 = StrCat(
      "HTTP/1.0 200 OK\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360\r\n"
      "\r\n");
  EXPECT_EQ(expected_headers2, response_headers_.ToString());

  response_headers_.RemoveAll(HttpAttributes::kCacheControl);
  response_headers_.Add(HttpAttributes::kCacheControl,
                        "max-age=10,private,no-cache,max-age=20,max-age=30");
  response_headers_.ComputeCaching();

  response_headers_.SetCacheControlMaxAge(360000);
  GoogleString expected_headers3 = StrCat(
      "HTTP/1.0 200 OK\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360,private,no-cache\r\n"
      "\r\n");
  EXPECT_EQ(expected_headers3, response_headers_.ToString());
}

}  // namespace net_instaweb
