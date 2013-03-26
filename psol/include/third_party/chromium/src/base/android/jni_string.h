// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JNI_STRING_H_
#define BASE_ANDROID_JNI_STRING_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/string_piece.h"

namespace base {
namespace android {

// Convert a Java string to UTF8. Returns a std string.
std::string ConvertJavaStringToUTF8(JNIEnv* env, jstring str);
std::string ConvertJavaStringToUTF8(const JavaRef<jstring>& str);

// Convert a std string to Java string.
ScopedJavaLocalRef<jstring> ConvertUTF8ToJavaString(
    JNIEnv* env,
    const base::StringPiece& str);

// Convert a Java string to UTF16. Returns a string16.
string16 ConvertJavaStringToUTF16(JNIEnv* env, jstring str);
string16 ConvertJavaStringToUTF16(const JavaRef<jstring>& str);

// Convert a string16 to a Java string.
ScopedJavaLocalRef<jstring> ConvertUTF16ToJavaString(
    JNIEnv* env,
    const base::StringPiece16& str);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_JNI_STRING_H_
