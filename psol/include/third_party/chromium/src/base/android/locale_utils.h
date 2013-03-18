// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_LOCALE_UTILS_H_
#define BASE_ANDROID_LOCALE_UTILS_H_

#include <jni.h>

#include <string>

#include "base/string16.h"

namespace base {
namespace android {

// Return the current default locale of the device.
std::string GetDefaultLocale();

string16 GetDisplayNameForLocale(const std::string& locale,
                                 const std::string& display_locale);

bool RegisterLocaleUtils(JNIEnv* env);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_LOCALE_UTILS_H_
