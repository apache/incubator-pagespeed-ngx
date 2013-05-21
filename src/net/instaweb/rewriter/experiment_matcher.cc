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
  int experiment_value = experiment::kExperimentNotSet;
  bool need_cookie = false;
  if (!experiment::GetExperimentCookieState(headers, &experiment_value) ||
      (experiment_value != experiment::kNoExperiment &&
       options->GetExperimentSpec(experiment_value) == NULL)) {
    // TODO(anupama): We currently do not handle "No-Experiment"
    // (PageSpeedExperiment=0) cookies well because we do not know whether these
    // are stale or new. Implement the grouping approach suggested in
    // http://b/6831327 for fixing this.
    experiment_value = experiment::DetermineExperimentState(options);
    need_cookie = true;
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
