// Copyright 2010 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_CORE_INPUT_CAPABILITIES_H_
#define PAGESPEED_CORE_INPUT_CAPABILITIES_H_

#include <string>

#include "base/basictypes.h"

namespace pagespeed {

// InputCapabilities enumerates the types of input data that a Rule
// instance may require. Certain types of data, such as response
// headers and status code, are always required and thus not
// enumerated here.
class InputCapabilities {
 public:
  static const uint32
    NONE                       = 0,
    DOM                        = 1<<0,
    // JS_CALLS_DOCUMENT_WRITE    = 1<<1,  // deprecated
    ONLOAD                     = 1<<2,
    // PARENT_CHILD_RESOURCE_MAP  = 1<<3,  // deprecated
    REQUEST_HEADERS            = 1<<4,
    RESPONSE_BODY              = 1<<5,
    REQUEST_START_TIMES        = 1<<6,
    TIMELINE_DATA              = 1<<7,
    DEPENDENCY_DATA            = 1<<8,
    ALL                        = ~0;

  InputCapabilities() : capabilities_mask_(NONE) {}
  explicit InputCapabilities(uint32 mask) : capabilities_mask_(mask) {}

  // Add additional capabilities for this InputCapabilities instance.
  void add(uint32 mask) { capabilities_mask_ |= mask; }

  // Get the capabilities mask for this InputCapabilities instance.
  uint32 capabilities_mask() const { return capabilities_mask_; }

  // Does the given InputCapabilities provide all the capabilities
  // specified in other InputCapabilities instance? For instance, if
  // other is (DOM|RESPONSE_BODY), this method will return true only
  // if the capabilities mask for this instance also has those bits
  // set.
  bool satisfies(const InputCapabilities& other) const {
    return
        (other.capabilities_mask_ & capabilities_mask_) ==
        other.capabilities_mask_;
  }

  bool equals(const InputCapabilities& other) const {
    return capabilities_mask_ == other.capabilities_mask_;
  }

  // Create a human-readable string describing this InputCapabilities in detail.
  std::string DebugString() const;

 private:
  // NOTE: this class provides a public default copy constructor since
  // it's just a POD type. Do not add non-POD data types here since the
  // default copy constructor will fail to copy them correctly.
  uint32 capabilities_mask_;
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_INPUT_CAPABILITIES_H_
