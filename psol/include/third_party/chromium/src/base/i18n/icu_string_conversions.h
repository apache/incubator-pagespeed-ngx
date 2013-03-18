// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICU_STRING_CONVERSIONS_H_
#define BASE_I18N_ICU_STRING_CONVERSIONS_H_

#include <string>

#include "base/i18n/base_i18n_export.h"
#include "base/string16.h"

namespace base {

// Defines the error handling modes of UTF16ToCodepage, CodepageToUTF16,
// WideToCodepage and CodepageToWide.
class OnStringConversionError {
 public:
  enum Type {
    // The function will return failure. The output buffer will be empty.
    FAIL,

    // The offending characters are skipped and the conversion will proceed as
    // if they did not exist.
    SKIP,

    // When converting to Unicode, the offending byte sequences are substituted
    // by Unicode replacement character (U+FFFD). When converting from Unicode,
    // this is the same as SKIP.
    SUBSTITUTE,
  };

 private:
  OnStringConversionError();
};

// Names of codepages (charsets) understood by icu.
BASE_I18N_EXPORT extern const char kCodepageLatin1[];  // a.k.a. ISO 8859-1
BASE_I18N_EXPORT extern const char kCodepageUTF8[];
BASE_I18N_EXPORT extern const char kCodepageUTF16BE[];
BASE_I18N_EXPORT extern const char kCodepageUTF16LE[];

// Converts between UTF-16 strings and the encoding specified.  If the
// encoding doesn't exist or the encoding fails (when on_error is FAIL),
// returns false.
BASE_I18N_EXPORT bool UTF16ToCodepage(const string16& utf16,
                                      const char* codepage_name,
                                      OnStringConversionError::Type on_error,
                                      std::string* encoded);
BASE_I18N_EXPORT bool CodepageToUTF16(const std::string& encoded,
                                      const char* codepage_name,
                                      OnStringConversionError::Type on_error,
                                      string16* utf16);

// Converts between wide strings and the encoding specified.  If the
// encoding doesn't exist or the encoding fails (when on_error is FAIL),
// returns false.
BASE_I18N_EXPORT bool WideToCodepage(const std::wstring& wide,
                                     const char* codepage_name,
                                     OnStringConversionError::Type on_error,
                                     std::string* encoded);
BASE_I18N_EXPORT bool CodepageToWide(const std::string& encoded,
                                     const char* codepage_name,
                                     OnStringConversionError::Type on_error,
                                     std::wstring* wide);

// Converts from any codepage to UTF-8 and ensures the resulting UTF-8 is
// normalized.
BASE_I18N_EXPORT bool ConvertToUtf8AndNormalize(const std::string& text,
                                                const std::string& charset,
                                                std::string* result);

}  // namespace base

#endif  // BASE_I18N_ICU_STRING_CONVERSIONS_H_
