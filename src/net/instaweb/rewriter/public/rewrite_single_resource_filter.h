/*
 * Copyright 2010 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_SINGLE_RESOURCE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_SINGLE_RESOURCE_FILTER_H_

#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class RewriteDriver;

// This class no longer has any functionality and exists only as a transition
// aid to avoid making a single change too large.
class RewriteSingleResourceFilter : public RewriteFilter {
 public:
  explicit RewriteSingleResourceFilter(RewriteDriver* driver)
      : RewriteFilter(driver) {
  }
  virtual ~RewriteSingleResourceFilter();

  enum RewriteResult {
    kRewriteFailed,  // rewrite is impossible or undesirable
    kRewriteOk,  // rewrite went fine
    kTooBusy   // the system is temporarily too busy to handle this
               // rewrite request; no conclusion can be drawn on whether
               // it's worth trying again or not.
  };

 protected:
  DISALLOW_COPY_AND_ASSIGN(RewriteSingleResourceFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_SINGLE_RESOURCE_FILTER_H_
