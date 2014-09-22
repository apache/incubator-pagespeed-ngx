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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_EXPERIMENT_MATCHER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_EXPERIMENT_MATCHER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class RequestHeaders;
class ResponseHeaders;
class RewriteOptions;
class UserAgentMatcher;

// Provides a way to replace the mapping of clients/sessions to experiments.
//
// The default implementation of the experiment framework uses cookies to send
// clients to the same experiment consistently. This implementation can be
// overridden to divide clients/sessions into experiments using a different
// mechanism.
class ExperimentMatcher {
 public:
  ExperimentMatcher() { }
  virtual ~ExperimentMatcher();

  // Decides which experiment to place the current client/session into.
  // Returns true if the mapping needs to be stored.
  virtual bool ClassifyIntoExperiment(const RequestHeaders& headers,
                                      const UserAgentMatcher& matcher,
                                      RewriteOptions* options);

  // Stores the client/session -> experiment mapping for the domain indicated
  // by url. The experiment id is indicated by state. The default
  // implementation stores this in a cookie in the response headers, setting it
  // to expire at expiration_time_ms (specified as ms since the epoch).
  virtual void StoreExperimentData(int state, const StringPiece& url,
      int64 expiration_time_ms, ResponseHeaders* headers);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExperimentMatcher);
};

}  // namespace net_instaweb
#endif  // NET_INSTAWEB_REWRITER_PUBLIC_EXPERIMENT_MATCHER_H_
