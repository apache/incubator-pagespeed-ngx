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

// Author: jmarantz@google.com (Joshua Marantz)

#include <vector>
#include "base/logging.h"
#include "net/instaweb/util/public/wildcard_group.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/wildcard.h"

namespace net_instaweb {

WildcardGroup::~WildcardGroup() {
  Clear();
}

void WildcardGroup::Clear() {
  STLDeleteElements(&wildcards_);
  allow_.clear();
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

bool WildcardGroup::Match(const StringPiece& str, bool allow) const {
  CHECK_EQ(wildcards_.size(), allow_.size());
  for (int i = wildcards_.size() - 1; i >= 0; --i) {
    // Match from last-inserted to first-inserted, returning status of
    // last-inserted match found.
    if (wildcards_[i]->Match(str)) {
      return allow_[i];
    }
  }
  return allow;
}

void WildcardGroup::CopyFrom(const WildcardGroup& src) {
  Clear();
  AppendFrom(src);
}

void WildcardGroup::AppendFrom(const WildcardGroup& src) {
  CHECK_EQ(src.wildcards_.size(), src.allow_.size());
  for (int i = 0, n = src.wildcards_.size(); i < n; ++i) {
    wildcards_.push_back(src.wildcards_[i]->Duplicate());
    allow_.push_back(src.allow_[i]);
  }
}

GoogleString WildcardGroup::Signature() const {
  GoogleString signature;
  for (int i = 0, n = wildcards_.size(); i < n; ++i) {
    StrAppend(&signature, wildcards_[i]->spec(), (allow_[i] ? "A" : "D"), ",");
  }
  return signature;
}

}  // namespace net_instaweb
