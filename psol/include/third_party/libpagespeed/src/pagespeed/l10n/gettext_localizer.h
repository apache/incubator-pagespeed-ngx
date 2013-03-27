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

#ifndef PAGESPEED_L10N_GETTEXT_LOCALIZER_H_
#define PAGESPEED_L10N_GETTEXT_LOCALIZER_H_

#include <locale>
#include <map>
#include <sstream>

#include "base/memory/scoped_ptr.h"
#include "pagespeed/l10n/localizer.h"

namespace pagespeed {

namespace l10n {

// Parses a locale string into its three parts (language, country, and
// encoding), returning them in the out parameters.
void ParseLocaleString(const std::string& locale, std::string* language_out,
                       std::string* country_out, std::string* encoding_out);

/**
 * A localizer that looks up translations of strings in copies of gettext .po
 * files compiled into the binary.  Uses std::locale for localizing numbers,
 * etc.
 */
class GettextLocalizer : public Localizer {
 public:
  // Create and return a new GettextLocalizer for the given locale, or NULL if
  // the locale cannot be instantiated.  Caller owns the returned object.
  // locale should be of the form "<language>_<country>.<encoding>"
  // (e.g. "en_UK.utf-8" signifies the dialect of English spoken in the UK, in
  // utf-8 encoding).  The country and encoding portions are optional (any
  // non-utf8 encodings will fail).  A locale string will match the most
  // specific locale available --- e.g. "en_UK" will return "en_UK" if
  // available, or "en" if not.  Locale matching is case-insensitive.
  static GettextLocalizer* Create(const std::string& locale);

  virtual const char* GetLocale() const;
  virtual bool LocalizeString(const std::string& val, std::string* out) const;
  virtual bool LocalizeInt(int64 val, std::string* out) const;
  virtual bool LocalizeUrl(const std::string& url, std::string* out) const;
  virtual bool LocalizeBytes(int64 bytes, std::string* out) const;
  virtual bool LocalizeTimeDuration(int64 ms, std::string* out) const;
  virtual bool LocalizePercentage(int64 percent, std::string* out) const;

 private:
  GettextLocalizer(const std::string& locale, const char** locale_string_table);

  const std::string locale_;

  // Pointer to the string table for the chosen locale
  const char** locale_string_table_;

  // It's >2x faster to reuse the same std::ostringstream object rather than
  // construct a new one for each call that needs it.
  void ClearOstream() const;
  mutable std::ostringstream ostream_;

  DISALLOW_COPY_AND_ASSIGN(GettextLocalizer);
};

} // namespace l10n

} // namespace pagespeed

#endif  // PAGESPEED_L10N_GETTEXT_LOCALIZER_H_
