// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_CASE_CONVERSION_H_
#define BASE_I18N_CASE_CONVERSION_H_
#pragma once

#include "base/string16.h"

namespace base {
namespace i18n {

// Returns the lower case equivalent of string. Uses ICU's default locale.
string16 ToLower(const string16& string);

// Returns the upper case equivalent of string. Uses ICU's default locale.
string16 ToUpper(const string16& string);

}  // namespace i18n
}  // namespace base

#endif  // BASE_I18N_CASE_CONVERSION_H_
