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

// Unit-test the experiment utilities.

#include "net/instaweb/rewriter/public/experiment_util.h"

#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_options_test_base.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace net_instaweb {

namespace experiment {

class ExperimentUtilTest : public RewriteOptionsTestBase<RewriteOptions> {
};

TEST_F(ExperimentUtilTest, GetCookieState) {
  RequestHeaders req_headers;
  int state;
  // Empty headers, cookie not set.
  EXPECT_FALSE(GetExperimentCookieState(req_headers, &state));
  EXPECT_EQ(kExperimentNotSet, state);

  // Headers with malformed experiment cookie, cookie not set.
  req_headers.Add(HttpAttributes::kCookie, "PageSpeedExperiment=absdfkjs");
  EXPECT_FALSE(GetExperimentCookieState(req_headers, &state));
  EXPECT_EQ(kExperimentNotSet, state);

  // Headers with valid experiment cookie in None (i.e. not in experiment)
  // state set.
  req_headers.Clear();
  req_headers.Add(HttpAttributes::kCookie, "PageSpeedExperiment=0");
  EXPECT_TRUE(GetExperimentCookieState(req_headers, &state));
  EXPECT_EQ(0, state);

  // Headers with valid experiment cookie in experiment 1.
  req_headers.Clear();
  req_headers.Add(HttpAttributes::kCookie, "PageSpeedExperiment=1");
  EXPECT_TRUE(GetExperimentCookieState(req_headers, &state));
  EXPECT_EQ(1, state);

  // Headers with valid experiment cookie in experiment 2.
  req_headers.Clear();
  req_headers.Add(HttpAttributes::kCookie, "PageSpeedExperiment=2");
  EXPECT_TRUE(GetExperimentCookieState(req_headers, &state));
  EXPECT_EQ(2, state);

  // Headers with valid experiment cookie in experiment 2.
  req_headers.Clear();
  req_headers.Add(HttpAttributes::kCookie,
                  "cookie=a;PageSpeedExperiment=2;something=foo");
  EXPECT_TRUE(GetExperimentCookieState(req_headers, &state));
  EXPECT_EQ(2, state);
}

// Test that we remove the PageSpeedExperiment cookie when it's there, and leave
// the remainder of the cookies in tact.
TEST_F(ExperimentUtilTest, RemoveExperimentCookie) {
  RequestHeaders req_headers;
  req_headers.Add(HttpAttributes::kAcceptEncoding, "gzip");
  req_headers.Add(HttpAttributes::kCookie,
                  "something=random;PageSpeedExperiment=18:2;another=cookie");
  RemoveExperimentCookie(&req_headers);

  GoogleString expected = "something=random;another=cookie";
  EXPECT_EQ(expected, req_headers.Lookup1(HttpAttributes::kCookie));

  req_headers.Clear();
  req_headers.Add(HttpAttributes::kCookie, "abd=123;jsjsj=4444");
  RemoveExperimentCookie(&req_headers);

  expected = "abd=123;jsjsj=4444";
  EXPECT_EQ(expected, req_headers.Lookup1(HttpAttributes::kCookie));
}

// Check that DetermineExperimentState behaves vaguely as expected.
TEST_F(ExperimentUtilTest, DetermineExperimentState) {
  RewriteOptions options(thread_system_.get());
  options.set_running_experiment(true);
  NullMessageHandler handler;
  ASSERT_TRUE(options.AddExperimentSpec("id=1;percent=35", &handler));
  ASSERT_TRUE(options.AddExperimentSpec("id=2;percent=35", &handler));
  ASSERT_EQ(2, options.num_experiments());
  int none = 0;
  int in_a = 0;
  int in_b = 0;
  int runs = 1000000;
  // In 100000000 runs, with 70% of the traffic in an experiment, we should
  // get some of each.
  for (int i = 0; i < runs; ++i) {
    int state = DetermineExperimentState(&options);
    switch (state) {
      case kNoExperiment:  // explicitly not in experiment
        ++none;
        break;
      case 1:  // in id=1
        ++in_a;
        break;
      case 2:  // in id=2
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

TEST_F(ExperimentUtilTest, AnyActiveExperiments) {
  RewriteOptions options(thread_system_.get());
  options.set_running_experiment(true);
  NullMessageHandler handler;
  ASSERT_TRUE(options.AddExperimentSpec("id=2;percent=0", &handler));
  ASSERT_TRUE(options.AddExperimentSpec("id=8;percent=0", &handler));
  EXPECT_FALSE(AnyActiveExperiments(&options));
  ASSERT_TRUE(options.AddExperimentSpec("id=1;percent=1", &handler));
  EXPECT_TRUE(AnyActiveExperiments(&options));
}

// Check that SetExperimentCookie sets the cookie on the appropriate
// domain, and with the correct expiration.
TEST_F(ExperimentUtilTest, SetExperimentCookie) {
  ResponseHeaders resp_headers;
  GoogleString url = "http://www.test.com/stuff/some_page.html";
  SetExperimentCookie(&resp_headers, 1, url, Timer::kWeekMs);
  EXPECT_TRUE(resp_headers.Has(HttpAttributes::kSetCookie));
  ConstStringStarVector v;
  EXPECT_TRUE(resp_headers.Lookup(HttpAttributes::kSetCookie, &v));
  ASSERT_EQ(1, v.size());
  GoogleString expires;
  ConvertTimeToString(Timer::kWeekMs, &expires);
  GoogleString expected = StringPrintf(
      "PageSpeedExperiment=1; Expires=%s; Domain=.www.test.com; Path=/",
      expires.c_str());
  EXPECT_EQ(expected, *v[0]);
}

}  // namespace experiment

}  // namespace net_instaweb
