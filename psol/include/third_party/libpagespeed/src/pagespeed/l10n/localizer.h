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

#ifndef PAGESPEED_L10N_LOCALIZER_H_
#define PAGESPEED_L10N_LOCALIZER_H_

#include <string>

#include "base/basictypes.h"

namespace pagespeed {

namespace l10n {

/** Interface for a class that provides localization.
 *
 * Provides methods for localizing generic constants (such as strings and ints),
 * as well as converting values such as byte counts and time durations into a
 * localized, human-readable format.
 *
 * Each of the Localize* methods takes a value and an std::string pointer out.
 * The Localizer should attempt to localize the value, and return true if
 * successful, putting the localized value in *out.  The Localizer should put a
 * reasonable value in *out even if localization fails.
 */
class Localizer {
 public:
  virtual ~Localizer();

  // Return the current locale
  virtual const char* GetLocale() const = 0;

  // Localize a string constant into the current locale
  virtual bool
      LocalizeString(const std::string& val, std::string* out) const = 0;

  // Localize an integer constant into the current locale
  virtual bool LocalizeInt(int64 val, std::string* out) const = 0;

  // Localize a URL into the current locale
  virtual bool LocalizeUrl(const std::string& url, std::string* out) const = 0;

  // Localize a byte count into a human-readable string in the current locale
  virtual bool LocalizeBytes(int64 bytes, std::string* out) const = 0;

  // Localize a time duration (in ms) into a human-readable string in the
  // current locale
  virtual bool LocalizeTimeDuration(int64 ms, std::string* out) const = 0;

  // Localize a percentage into a human-readable string in the current locale
  virtual bool LocalizePercentage(int64 percent, std::string* out) const = 0;
};

/**
 * A localizer that localizes to English (doesn't change constants, and
 * humanizes byte counts and time durations in English).
 */
class BasicLocalizer : public Localizer {
 public:
  virtual const char* GetLocale() const;
  virtual bool LocalizeString(const std::string& val, std::string* out) const;
  virtual bool LocalizeInt(int64 val, std::string* out) const;
  virtual bool LocalizeUrl(const std::string& url, std::string* out) const;
  virtual bool LocalizeBytes(int64 bytes, std::string* out) const;
  virtual bool LocalizeTimeDuration(int64 ms, std::string* out) const;
  virtual bool LocalizePercentage(int64 percent, std::string* out) const;
};

/**
 * A localizer that does nothing (just converts all values into strings).
 */
class NullLocalizer : public Localizer {
 public:
  virtual const char* GetLocale() const;
  virtual bool LocalizeString(const std::string& val, std::string* out) const;
  virtual bool LocalizeInt(int64 val, std::string* out) const;
  virtual bool LocalizeUrl(const std::string& url, std::string* out) const;
  virtual bool LocalizeBytes(int64 bytes, std::string* out) const;
  virtual bool LocalizeTimeDuration(int64 ms, std::string* out) const;
  virtual bool LocalizePercentage(int64 percent, std::string* out) const;
};

} // namespace l10n

} // namespace pagespeed

#endif  // PAGESPEED_L10N_LOCALIZER_H_
