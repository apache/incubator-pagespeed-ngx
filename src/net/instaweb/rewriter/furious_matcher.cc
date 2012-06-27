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

#include "net/instaweb/rewriter/public/furious_matcher.h"

#include "net/instaweb/rewriter/public/furious_util.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"  // for int64

namespace net_instaweb {

FuriousMatcher::~FuriousMatcher() { }

bool FuriousMatcher::ClassifyIntoExperiment(
    const RequestHeaders& headers, RewriteOptions* options) {
  int furious_value = furious::kFuriousNotSet;
  bool need_cookie = false;
  if (!furious::GetFuriousCookieState(headers, &furious_value)) {
    furious_value = furious::DetermineFuriousState(options);
    need_cookie = true;
  }
  options->SetFuriousState(furious_value);
  return need_cookie;
}

void FuriousMatcher::StoreExperimentData(
    int state, const StringPiece& url, int64 now_ms, ResponseHeaders* headers) {
  furious::SetFuriousCookie(headers, state, url, now_ms);
}


}  // namespace net_instaweb
