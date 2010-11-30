/**
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/wildcard_group.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/wildcard.h"

namespace net_instaweb {

WildcardGroup::~WildcardGroup() {
  STLDeleteElements(&wildcards_);
}

void WildcardGroup::Allow(const StringPiece& expr) {
  Wildcard* wildcard = new Wildcard(expr);
  wildcards_.push_back(wildcard);
  allow_.push_back(true);
}

void WildcardGroup::Disallow(const StringPiece& expr) {
  Wildcard* wildcard = new Wildcard(expr);
  wildcards_.push_back(wildcard);
  allow_.push_back(false);
}

bool WildcardGroup::Match(const StringPiece& str) const {
  bool allow = true;
  CHECK_EQ(wildcards_.size(), allow_.size());
  for (int i = 0, n = wildcards_.size(); i < n; ++i) {
    // Do not bother to execute the wildcard match if a match would
    // not change the current 'allow' status.  E.g. once we have found
    // an 'allow' match, we can ignore all subsequent 'allow' tests
    // until a rule disallows the match.
    if ((allow != allow_[i]) && wildcards_[i]->Match(str)) {
      allow = !allow;
    }
  }
  return allow;
}

void WildcardGroup::CopyFrom(const WildcardGroup& src) {
  wildcards_.clear();
  allow_.clear();
  CHECK_EQ(src.wildcards_.size(), src.allow_.size());
  for (int i = 0, n = src.wildcards_.size(); i < n; ++i) {
    wildcards_.push_back(src.wildcards_[i]->Duplicate());
    allow_.push_back(src.allow_[i]);
  }
}

}  // namespace net_instaweb
