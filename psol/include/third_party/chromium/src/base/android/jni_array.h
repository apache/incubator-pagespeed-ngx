// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JNI_ARRAY_H_
#define BASE_ANDROID_JNI_ARRAY_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/string16.h"

namespace base {
namespace android {

// Returns a new Java byte array converted from the given bytes array.
ScopedJavaLocalRef<jbyteArray> ToJavaByteArray(
    JNIEnv* env, const uint8* bytes, size_t len);

// Returns a array of Java byte array converted from |v|.
ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfByteArray(
    JNIEnv* env, const std::vector<std::string>& v);

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStrings(
    JNIEnv* env,  const std::vector<std::string>& v);

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStrings(
    JNIEnv* env,  const std::vector<string16>& v);

// Converts a Java string array to a native array.
void AppendJavaStringArrayToStringVector(JNIEnv* env,
                                         jobjectArray array,
                                         std::vector<string16>* out);

void AppendJavaStringArrayToStringVector(JNIEnv* env,
                                         jobjectArray array,
                                         std::vector<std::string>* out);

// Appends the Java bytes in |bytes_array| onto the end of |out|.
void AppendJavaByteArrayToByteVector(JNIEnv* env,
                                     jbyteArray byte_array,
                                     std::vector<uint8>* out);

// Replaces the content of |out| with the Java bytes in |bytes_array|.
void JavaByteArrayToByteVector(JNIEnv* env,
                               jbyteArray byte_array,
                               std::vector<uint8>* out);

// Replaces the content of |out| with the Java ints in |int_array|.
void JavaIntArrayToIntVector(JNIEnv* env,
                             jintArray int_array,
                             std::vector<int>* out);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_JNI_ARRAY_H_
