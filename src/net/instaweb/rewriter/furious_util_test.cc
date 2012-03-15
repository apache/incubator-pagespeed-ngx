/*
 * Copyright 2012 Google Inc.
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

// Author: nforman@google.com (Naomi Forman)

// Unit-test the furious utilities.

#include "net/instaweb/rewriter/public/furious_util.h"

#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace furious {

class FuriousUtilTest : public testing::Test {
 protected:
  FuriousUtilTest() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(FuriousUtilTest);
};

TEST_F(FuriousUtilTest, GetCookieState) {
  RequestHeaders req_headers;
  int state;
  // Empty headers, cookie not set.
  EXPECT_FALSE(GetFuriousCookieState(req_headers, &state));
  EXPECT_EQ(kFuriousNotSet, state);

  // Headers with malformed furious cookie, cookie not set.
  req_headers.Add(HttpAttributes::kCookie, "_GFURIOUS=absdfkjs");
  EXPECT_FALSE(GetFuriousCookieState(req_headers, &state));
  EXPECT_EQ(kFuriousNotSet, state);

  // Headers with valid furious cookie in None (i.e. not in experiment)
  // state set.
  req_headers.Clear();
  req_headers.Add(HttpAttributes::kCookie, "_GFURIOUS=0");
  EXPECT_TRUE(GetFuriousCookieState(req_headers, &state));
  EXPECT_EQ(0, state);

  // Headers with valid furious cookie in experiment 1.
  req_headers.Clear();
  req_headers.Add(HttpAttributes::kCookie, "_GFURIOUS=1");
  EXPECT_TRUE(GetFuriousCookieState(req_headers, &state));
  EXPECT_EQ(1, state);

  // Headers with valid furious cookie in experiment 2.
  req_headers.Clear();
  req_headers.Add(HttpAttributes::kCookie, "_GFURIOUS=2");
  EXPECT_TRUE(GetFuriousCookieState(req_headers, &state));
  EXPECT_EQ(2, state);

  // Headers with valid furious cookie in experiment 2.
  req_headers.Clear();
  req_headers.Add(HttpAttributes::kCookie,
                  "cookie=a;_GFURIOUS=2;something=foo");
  EXPECT_TRUE(GetFuriousCookieState(req_headers, &state));
  EXPECT_EQ(2, state);
}

// Test that we remove the _GFURIOUS cookie when it's there, and leave
// the remainder of the cookies in tact.
TEST_F(FuriousUtilTest, RemoveFuriousCookie) {
  RequestHeaders req_headers;
  req_headers.Add(HttpAttributes::kAcceptEncoding, "gzip");
  req_headers.Add(HttpAttributes::kCookie,
                  "something=random;_GFURIOUS=18:2;another=cookie");
  RemoveFuriousCookie(&req_headers);

  GoogleString expected = "something=random;another=cookie";
  EXPECT_EQ(expected, req_headers.Lookup1(HttpAttributes::kCookie));

  req_headers.Clear();
  req_headers.Add(HttpAttributes::kCookie, "abd=123;jsjsj=4444");
  RemoveFuriousCookie(&req_headers);

  expected = "abd=123;jsjsj=4444";
  EXPECT_EQ(expected, req_headers.Lookup1(HttpAttributes::kCookie));
}

// Check that DetermineFuriousState behaves vaguely as expected.
TEST_F(FuriousUtilTest, DetermineFuriousState) {
  RewriteOptions options;
  options.set_running_furious_experiment(true);
  options.set_furious_percent(70);
  NullMessageHandler handler;
  ASSERT_TRUE(options.AddFuriousSpec("id=7;", &handler));
  ASSERT_EQ(1, options.num_furious_experiments());
  int none = 0;
  int in_a = 0;
  int in_b = 0;
  int runs = 1000000;
  // In 100000000 runs, with 70% of the traffic in an experiment, we should
  // get some of each.
  for (int i = 0; i < runs; ++i) {
    int state = DetermineFuriousState(&options);
    switch (state) {
      case kFuriousNoExperiment:  // explicitly not in experiment
        ++none;
        break;
      case kFuriousControl:  // in control
        ++in_a;
        break;
      case 7:  // in id=7
        ++in_b;
        break;
      default:
        ASSERT_TRUE(0) << "Got unknown state";
    }
  }
  // Make sure they're all in a reasonable range.  Since we do this
  // randomly, they'll probably never be exactly 35/35/30.
  // This gives us a 10% buffer in each direction for each bucket.
  EXPECT_TRUE(none < .4 * runs);
  EXPECT_TRUE(none > .2 * runs);
  EXPECT_TRUE(in_a < .45 * runs);
  EXPECT_TRUE(in_a > .25 * runs);
  EXPECT_TRUE(in_b < .45 * runs);
  EXPECT_TRUE(in_b > .25 * runs);
}

// Check that SetFuriousCookie sets the cookie on the appropriate
// domain, and with the correct expiration.
TEST_F(FuriousUtilTest, SetFuriousCookie) {
  ResponseHeaders resp_headers;
  GoogleString url = "http://www.test.com/stuff/some_page.html";
  int64 now = 0;
  SetFuriousCookie(&resp_headers, 1, url.c_str(), now);
  EXPECT_TRUE(resp_headers.Has(HttpAttributes::kSetCookie));
  ConstStringStarVector v;
  EXPECT_TRUE(resp_headers.Lookup(HttpAttributes::kSetCookie, &v));
  ASSERT_EQ(1, v.size());
  GoogleString expires;
  ConvertTimeToString(Timer::kWeekMs, &expires);
  GoogleString expected = StringPrintf(
      "_GFURIOUS=1; Expires=%s; Domain=.www.test.com; Path=/", expires.c_str());
  EXPECT_EQ(expected, *v[0]);
}

}  // namespace furious

}  // namespace net_instaweb
