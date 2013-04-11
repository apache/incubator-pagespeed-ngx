// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_BASE_JNI_REGISTRAR_H_
#define BASE_ANDROID_BASE_JNI_REGISTRAR_H_

#include <jni.h>

namespace base {
namespace android {

// Register all JNI bindings necessary for base.
bool RegisterJni(JNIEnv* env);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_BASE_JNI_REGISTRAR_H_
