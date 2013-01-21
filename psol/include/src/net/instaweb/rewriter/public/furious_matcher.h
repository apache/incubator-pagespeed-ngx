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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_FURIOUS_MATCHER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_FURIOUS_MATCHER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class RequestHeaders;
class ResponseHeaders;
class RewriteOptions;

// Provides a way to replace the mapping of clients/sessions to furious
// experiments.
//
// Furious is the A/B experiment framework that enables us to track
// page speed statistics and correlate them with different sets of
// rewriters. The default implementation uses cookies to send clients
// to the same experiment consistently. This implementation can be
// overridden to divide clients/sessions into experiments using a
// different mechanism.
class FuriousMatcher {
 public:
  FuriousMatcher() { }
  virtual ~FuriousMatcher();

  // Decides which experiment to place the current client/session into.
  // Returns true if the mapping needs to be stored.
  virtual bool ClassifyIntoExperiment(const RequestHeaders& headers,
                                      RewriteOptions* options);

  // Stores the client/session -> experiment mapping for the domain indicated
  // by url. The experiment id is indicated by state. The default
  // implementation stores this in a cookie in the response headers, setting it
  // to expire one week from now_ms.
  virtual void StoreExperimentData(int state, const StringPiece& url,
                                   int64 now_ms, ResponseHeaders* headers);

 private:
  DISALLOW_COPY_AND_ASSIGN(FuriousMatcher);
};

}  // namespace net_instaweb
#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FURIOUS_MATCHER_H_
