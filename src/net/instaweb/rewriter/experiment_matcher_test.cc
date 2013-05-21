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

// Author: mukerjee@google.com (Matt Mukerjee)

// Unit-test for ExperimentMatcher

#include "net/instaweb/rewriter/public/experiment_matcher.h"

#include "net/instaweb/http/public/meta_data.h"  // for HttpAttributes, etc
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_options_test_base.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/time_util.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace net_instaweb {

class ExperimentMatcherTest : public RewriteOptionsTestBase<RewriteOptions> {
 protected:
  ExperimentMatcher experiment_matcher_;
};

// Test that the experiment utils are working together correctly. First tests
// that we can add an experiment spec then classifies the client into an
// experiment. Then manually inserts a cookie and checks that client will not
// ask for another cookie. Then we remove this cookie and ask for classification
// again. We then have the experiment frameworka store what side of the
// experiment we ended on in a cookie for us, which we also check.
TEST_F(ExperimentMatcherTest, ClassifyIntoExperiment) {
  RequestHeaders req_headers;
  RewriteOptions options(thread_system_.get());
  options.set_running_experiment(true);
  NullMessageHandler handler;
  ASSERT_TRUE(options.AddExperimentSpec("id=1;percent=100", &handler));
  ASSERT_EQ(1, options.num_experiments());
  bool need_cookie = experiment_matcher_.ClassifyIntoExperiment(req_headers,
                                                                &options);

  // We expect 1 here because we set up an experiment above (id=1;percent=100)
  // that takes 100% of the traffic and puts it into an experiment with id=1.
  ASSERT_EQ(1, options.experiment_id());
  ASSERT_TRUE(need_cookie);

  req_headers.Add(HttpAttributes::kCookie, "PageSpeedExperiment=1");
  need_cookie = experiment_matcher_.ClassifyIntoExperiment(
      req_headers, &options);
  ASSERT_FALSE(need_cookie);

  experiment::RemoveExperimentCookie(&req_headers);
  need_cookie = experiment_matcher_.ClassifyIntoExperiment(
      req_headers, &options);

  // Same as above comment.
  ASSERT_EQ(1, options.experiment_id());
  ASSERT_TRUE(need_cookie);

  // Same test used for experiment::SetExperimentCookie in experiment_util_test.
  ResponseHeaders resp_headers;
  GoogleString url = "http://www.test.com/stuff/some_page.html";
  experiment_matcher_.StoreExperimentData(
      options.experiment_id(), url, 0, &resp_headers);
  ASSERT_TRUE(resp_headers.Has(HttpAttributes::kSetCookie));
  ConstStringStarVector v;
  EXPECT_TRUE(resp_headers.Lookup(HttpAttributes::kSetCookie, &v));
  ASSERT_EQ(1, v.size());
  GoogleString expires;
  ConvertTimeToString(0, &expires);
  GoogleString expected = StringPrintf(
      "PageSpeedExperiment=1; Expires=%s; Domain=.www.test.com; Path=/",
      expires.c_str());
  EXPECT_EQ(expected, *v[0]);
}

TEST_F(ExperimentMatcherTest, ClassifyIntoExperimentStaleCookie) {
  RequestHeaders req_headers;
  RewriteOptions options(thread_system_.get());
  options.set_running_experiment(true);
  NullMessageHandler handler;
  ASSERT_TRUE(options.AddExperimentSpec("id=1;percent=100", &handler));

  // Test that a new cookie is set when the incoming cookie has an invalid id.
  req_headers.Add(HttpAttributes::kCookie, "PageSpeedExperiment=4");
  bool need_cookie =
      experiment_matcher_.ClassifyIntoExperiment(req_headers, &options);
  ASSERT_TRUE(need_cookie);
}

TEST_F(ExperimentMatcherTest, ClassifyIntoExperimentNoExptCookie) {
  RequestHeaders req_headers;
  RewriteOptions options(thread_system_.get());
  options.set_running_experiment(true);
  NullMessageHandler handler;
  ASSERT_TRUE(options.AddExperimentSpec("id=1;percent=100", &handler));

  // Test that a new cookie is not assigned when the incoming cookie has the
  // no-expt cookie.
  req_headers.Add(HttpAttributes::kCookie, "PageSpeedExperiment=0");
  bool need_cookie =
      experiment_matcher_.ClassifyIntoExperiment(req_headers, &options);
  ASSERT_FALSE(need_cookie);
}

}  // namespace net_instaweb
