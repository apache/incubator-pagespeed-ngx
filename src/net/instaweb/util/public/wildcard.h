// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jmarantz@google.com (Joshua Marantz)
//         jmaessen@google.com (Jan-Willem Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_WILDCARD_H_
#define NET_INSTAWEB_UTIL_PUBLIC_WILDCARD_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class Wildcard {
 public:
  static const char kMatchAny;  // *
  static const char kMatchOne;  // ?

  // Create a wildcard object with the specification using * and ?
  // as wildcards.  There is currently no way to quote * or ?.
  explicit Wildcard(const StringPiece& wildcard_spec);

  // Determines whether a string matches the wildcard.
  bool Match(const StringPiece& str) const;

  // Determines whether this wildcard is just a simple name, lacking
  // any wildcard characters.
  bool IsSimple() const { return is_simple_; }

  // Returns the original wildcard specification.
  const StringPiece spec() const {
    return StringPiece(storage_.data(), storage_.size() - 1);
  }

  // Makes a duplicate copy of the wildcard object.
  Wildcard* Duplicate() const;

 private:
  Wildcard(const GoogleString& storage, int num_blocks,
           int last_block_offset, bool is_simple);

  void InitFromSpec(const StringPiece& wildcard_spec);

  GoogleString storage_;
  int num_blocks_;
  int last_block_offset_;
  bool is_simple_;
  DISALLOW_COPY_AND_ASSIGN(Wildcard);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_WILDCARD_H_
