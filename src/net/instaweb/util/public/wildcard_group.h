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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SELECTOR_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SELECTOR_H_

#include <vector>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class Wildcard;

// This forms the basis of a wildcard selection mechanism, allowing
// a user to issue sequence of commands like:
//
//   1. allow *.cc
//   2. allow *.h
//   3. disallow a*.h
//   4. allow ab*.h
//   5. disallow c*.cc
//
// This sequence would yield the following results whnen
//   Match("x.cc") --> true  due to rule #1
//   Match("c.cc") --> false due to rule #5 which overrides rule #1
//   Match("y.h")  --> true  due to rule #2
//   Match("a.h")  --> false due to rule #3 which overrides rule #2
//   Match("ab.h") --> true  due to rule #4 which overrides rule #3
// So order matters.
class WildcardGroup {
 public:
  WildcardGroup() {}
  ~WildcardGroup();

  // Determines whether a string matches the wildcard group.
  bool Match(const StringPiece& str) const;

  // Add an expression to Allow, potentially overriding previous calls to
  // Disallow.
  void Allow(const StringPiece& wildcard);

  // Add an expression to Disallow, potentially overriding previous calls to
  // Allow.
  void Disallow(const StringPiece& wildcard);

  void CopyFrom(const WildcardGroup& src);
  void AppendFrom(const WildcardGroup& src);

 private:
  bool MatchHelper(int piece_index, const StringPiece& str);

  // To avoid having to new another structure we use two parallel
  // vectors.  Note that vector<bool> is special-case implemented
  // in STL to be bit-packed.
  std::vector<Wildcard*> wildcards_;
  std::vector<bool> allow_;  // parallel array (actually a bitvector)
  DISALLOW_COPY_AND_ASSIGN(WildcardGroup);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SELECTOR_H_
