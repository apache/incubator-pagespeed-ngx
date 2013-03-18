// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_PATH_UTILS_H_
#define BASE_ANDROID_PATH_UTILS_H_

#include <jni.h>

class FilePath;

namespace base {
namespace android {

// Retrieves the absolute path to the data directory of the current
// application. The result is placed in the FilePath pointed to by 'result'.
// This method is dedicated for base_paths_android.c, Using
// PathService::Get(base::DIR_ANDROID_APP_DATA, ...) gets the data dir.
bool GetDataDirectory(FilePath* result);

// Retrieves the absolute path to the cache directory. The result is placed in
// the FilePath pointed to by 'result'. This method is dedicated for
// base_paths_android.c, Using PathService::Get(base::DIR_CACHE, ...) gets the
// cache dir.
bool GetCacheDirectory(FilePath* result);

// Retrieves the path to the public downloads directory. The result is placed
// in the FilePath pointed to by 'result'.
bool GetDownloadsDirectory(FilePath* result);

// Retrieves the path to the native JNI libraries via
// ApplicationInfo.nativeLibraryDir on the Java side. The result is placed in
// the FilePath pointed to by 'result'.
bool GetNativeLibraryDirectory(FilePath* result);

// Retrieves the absolute path to the external storage directory. The result
// is placed in the FilePath pointed to by 'result'.
bool GetExternalStorageDirectory(FilePath* result);

bool RegisterPathUtils(JNIEnv* env);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_PATH_UTILS_H_
