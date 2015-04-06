// Copyright 2013 Google Inc. All Rights Reserved.
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

// Author: morlovich@google.com (Maksim Orlovich)

#ifndef PAGESPEED_KERNEL_HTTP_USER_AGENT_NORMALIZER_H_
#define PAGESPEED_KERNEL_HTTP_USER_AGENT_NORMALIZER_H_

#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/util/re2.h"

namespace net_instaweb {

// Base class for user agent string normalizer. The idea is that UA strings,
// sometimes include irrelevant information, so this provides a way of stripping
// it, to improve cache hit-rates when the normalized UAs are used in cache
// keys.
class UserAgentNormalizer {
 public:
  UserAgentNormalizer() {}
  virtual ~UserAgentNormalizer();

  virtual GoogleString Normalize(const GoogleString& in) const = 0;

  // Helper that applies all the normalizers in the ua_normalizers list
  // to ua_in.
  static GoogleString NormalizeWithAll(
      const std::vector<const UserAgentNormalizer*>& ua_normalizers,
      const GoogleString& ua_in);

 private:
  DISALLOW_COPY_AND_ASSIGN(UserAgentNormalizer);
};

// This normalizes some common UA strings for Android devices
// by dropping the device & build names from them.
class AndroidUserAgentNormalizer : public UserAgentNormalizer {
 public:
  AndroidUserAgentNormalizer();
  virtual ~AndroidUserAgentNormalizer();

  virtual GoogleString Normalize(const GoogleString& in) const;

 private:
  RE2 dalvik_ua_;
  RE2 chrome_android_ua_;
  RE2 android_browser_ua_;
};

// This normalizes UA strings for MSIE, by dropping the long list of
// installed extensions (like .NET) it likes to include, and just keeping the
// important info like browser, OS, and chromeframe version.
class IEUserAgentNormalizer : public UserAgentNormalizer {
 public:
  IEUserAgentNormalizer();
  virtual ~IEUserAgentNormalizer();

  virtual GoogleString Normalize(const GoogleString& in) const;

 private:
  RE2 ie_ua_;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTTP_USER_AGENT_NORMALIZER_H_
