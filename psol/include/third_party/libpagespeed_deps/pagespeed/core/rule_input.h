// Copyright 2009 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_CORE_RULE_INPUT_H_
#define PAGESPEED_CORE_RULE_INPUT_H_

#include <map>
#include <string>

#include "base/basictypes.h"

namespace pagespeed {

class PagespeedInput;
class Resource;

class RuleInput {
 public:
  explicit RuleInput(const PagespeedInput& pagespeed_input);
  void Init();

  const PagespeedInput& pagespeed_input() const { return *pagespeed_input_; }

  // Determine how many bytes would the response body be if it were gzipped
  // (whether or not the resource actually was gzipped).  For resources that
  // aren't compressible (e.g. PNGs), yields the original request body size.
  // Return true on success, false on error.  This method is memoized, so it is
  // cheap to call.
  bool GetCompressedResponseBodySize(const Resource& resource,
                                     int* output) const;

 private:
  const PagespeedInput* pagespeed_input_;
  mutable std::map<const Resource*, int> compressed_response_body_sizes_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(RuleInput);
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_RULE_INPUT_H_
