// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICU_UTIL_H_
#define BASE_I18N_ICU_UTIL_H_

#include "base/i18n/base_i18n_export.h"

namespace icu_util {

// Call this function to load ICU's data tables for the current process.  This
// function should be called before ICU is used.
BASE_I18N_EXPORT bool Initialize();

}  // namespace icu_util

#endif  // BASE_I18N_ICU_UTIL_H_
