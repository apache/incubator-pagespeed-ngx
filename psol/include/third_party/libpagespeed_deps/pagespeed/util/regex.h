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

#ifndef PAGESPEED_UTIL_REGEX_H_
#define PAGESPEED_UTIL_REGEX_H_

#ifdef _WIN32
// On Windows, we use tr1 regex, which is available in Visual Studio
// 2008 and later. This includes Visual Studio 2008 express. In the
// non-express version, you need to install a feature pack in order to
// get tr1:
// http://www.microsoft.com/downloads/details.aspx?FamilyId=D466226B-8DAB-445F-A7B4-448B326C48E7
#include <regex>
#else
// Compilers based on older standards (e.g. NaCl) may require us to #include
// <sys/types.h> before #including <regex.h>.  Refer to:
//   http://opengroup.org/onlinepubs/007908775/xsh/regexec.html
#include <sys/types.h>  // Required for <regex.h> by POSIX 1997.
#include <regex.h>
#endif

namespace pagespeed {

// Simple regex class that delegates to regex_t by default, and uses
// TR1 regex on Windows (where regex_t is unavailable).
class RE {
 public:
  explicit RE();
  ~RE();

  // Returns false if he pattterns is an invalid regex or if the RE
  // has already been initialized.
  bool Init(const char *pattern);

  bool is_valid() const { return is_valid_; }

  // Should not be called with an uninitialized or invalid RE.
  bool PartialMatch(const char *str) const;

private:
#ifdef _WIN32
  std::tr1::regex regex_;
#else
  regex_t regex_;
#endif
  bool is_initialized_;
  bool is_valid_;
};

}  // namespace pagespeed

#endif  // PAGESPEED_UTIL_REGEX_H_
