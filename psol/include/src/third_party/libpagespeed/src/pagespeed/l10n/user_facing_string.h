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

// Author: aoates@google.com (Andrew Oates)

#ifndef PAGESPEED_L10N_LOCALIZED_STRING_H_
#define PAGESPEED_L10N_LOCALIZED_STRING_H_

#include <string>

namespace pagespeed {

// A user-facing string (a string that will be presented to the user).
// User-facing strings have to be marked for localization (or explicitly
// excluded).  All functions and methods that generate user-facing strings take
// a UserFacingString, which are only created when a string is passed through
// the _(...) localization markup macro in l10n.h.  This allows checking at
// compile-time that all user-facing strings are appropriately marked.  Each
// string is marked with a flag should_localize --- localized strings (marked
// with the _(...) macro) should be passed through the localization process
// (i.e.  translated and put in string tables), while non-localized strings
// (marked with the non_localized(...) macro) should not.  This allows us to
// skip lookups on non-translatable strings like "$1".
class UserFacingString {
 public:
  UserFacingString() : value_(NULL) {}
  // This should NEVER be called, except by the macros in l10n.h
  UserFacingString(const char* s, bool should_localize)
      : value_(s), should_localize_(should_localize) {}

  // Returns true if the string should be localized before presentation.
  bool ShouldLocalize() const { return should_localize_; }

  // Conversion operator that allows UserFacingString to be cast into a
  // const char* (e.g. for use passing to a method that takes a const char*)
  operator const char*() const { return value_; }

 private:
  const char* value_;
  bool should_localize_;
};

} // namespace pagespeed

#endif  // PAGESPEED_L10N_LOCALIZED_STRING_H_
