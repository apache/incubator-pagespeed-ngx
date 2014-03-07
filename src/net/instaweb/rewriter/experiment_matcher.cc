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

#include "net/instaweb/rewriter/public/experiment_matcher.h"

#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"  // for int64

namespace net_instaweb {

ExperimentMatcher::~ExperimentMatcher() { }

bool ExperimentMatcher::ClassifyIntoExperiment(
    const RequestHeaders& headers, RewriteOptions* options) {
  bool need_cookie;
  int experiment_value = experiment::kExperimentNotSet;
  experiment::GetExperimentCookieState(headers, &experiment_value);

  if (options->enroll_experiment_id() == experiment::kExperimentNotSet) {
    // Forcing kExperimentNotSet means "reassign this user".  While normally we
    // don't set any cookies if all percentages are 0%, here we do because they
    // may be trying to clear a test cookie for a 0% experiment.
    experiment_value = experiment::DetermineExperimentState(options);
    need_cookie = true;
  } else if (
      options->enroll_experiment_id() == experiment::kNoExperiment ||
      options->GetExperimentSpec(options->enroll_experiment_id()) != NULL) {
    // Only allow people to force experiment ids that are actually defined
    // plus kNoExperiment.
    experiment_value = options->enroll_experiment_id();
    need_cookie = true;
  } else if (experiment_value == experiment::kNoExperiment) {
    // TODO(jefftk): They're assigned to the control group, but we don't handle
    // this right because we don't know if the cookie is stale.  For example,
    // they may have run one experiment on 5% of visitors and now be running one
    // on 50% but that 95% who originally got put into "No-Experiment"
    // (PageSpeedExperiment=0) will be excluded until their cookies expire.
    need_cookie = false;
  } else if (options->GetExperimentSpec(experiment_value) == NULL) {
    // Either:
    //  * They're not yet assigned to an experiment grouping.
    //  * They were assigned, but that experiment isn't running anymore.
    //
    // Only set cookies if there are active experiments.  This avoids the
    // problem where when someone is preparing to run experiments by testing
    // configuration on a live site all the visitors start getting put in the
    // "no experiment" group.  Not only does that reduce the sample available
    // for experimentation, but it adds a bias away from repeat visitors.
    if (experiment::AnyActiveExperiments(options)) {
      experiment_value = experiment::DetermineExperimentState(options);
      need_cookie = true;
    } else {
      need_cookie = false;
    }
  } else {
    // They're in an experiment, there's nothing wrong with it, all is well.
    need_cookie = false;
  }

  options->SetExperimentState(experiment_value);
  return need_cookie;
}

void ExperimentMatcher::StoreExperimentData(
    int state, const StringPiece& url, int64 expiration_time_ms,
    ResponseHeaders* headers) {
  experiment::SetExperimentCookie(headers, state, url, expiration_time_ms);
}


}  // namespace net_instaweb
