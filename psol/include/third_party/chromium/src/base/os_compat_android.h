// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OS_COMPAT_ANDROID_H_
#define BASE_OS_COMPAT_ANDROID_H_
#pragma once

#include <fcntl.h>
#include <sys/types.h>
#include <utime.h>

// Not implemented in Bionic. See platform_file_android.cc.
extern "C" int futimes(int fd, const struct timeval tv[2]);

// The prototype of mkdtemp is missing.
extern "C" char* mkdtemp(char* path);

// The lockf() function is not available on Android; we translate to flock().
#define F_LOCK LOCK_EX
#define F_ULOCK LOCK_UN
inline int lockf(int fd, int cmd, off_t ignored_len) {
  return flock(fd, cmd);
}

#endif  // BASE_OS_COMPAT_ANDROID_H_
