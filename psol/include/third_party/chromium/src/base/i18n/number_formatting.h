// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_NUMBER_FORMATTING_H_
#define BASE_I18N_NUMBER_FORMATTING_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"

namespace base {

// Return a number formatted with separators in the user's locale.
// Ex: FormatNumber(1234567)
//         => "1,234,567" in English, "1.234.567" in German
string16 FormatNumber(int64 number);

// Return a number formatted with separators in the user's locale.
// Ex: FormatDouble(1234567.8, 1)
//         => "1,234,567.8" in English, "1.234.567,8" in German
string16 FormatDouble(double number, int fractional_digits);

namespace testing {

// Causes cached formatters to be discarded and recreated. Only useful for
// testing.
void ResetFormatters();

}  // namespace testing

}  // namespace base

#endif  // BASE_I18N_NUMBER_FORMATTING_H_
