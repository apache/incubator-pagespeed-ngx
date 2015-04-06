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

// Author: morlovich@google.com (Maksim Orlovich)
//

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_RESULT_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_RESULT_H_

namespace net_instaweb {

// Used to signal whether optimization was successful or not to
// RewriteContext::RewriteDone.
enum RewriteResult {
  kRewriteFailed,  // rewrite is impossible or undesirable
  kRewriteOk,  // rewrite went fine
  kTooBusy   // the system is temporarily too busy to handle this
             // rewrite request; no conclusion can be drawn on whether
             // it's worth trying again or not.
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_RESULT_H_
