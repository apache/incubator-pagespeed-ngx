// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File error statistics gathering.

#ifndef NET_BASE_FILE_STREAM_METRICS_H_
#define NET_BASE_FILE_STREAM_METRICS_H_

namespace net {

enum FileErrorSource {
  FILE_ERROR_SOURCE_OPEN = 0,
  FILE_ERROR_SOURCE_WRITE,
  FILE_ERROR_SOURCE_READ,
  FILE_ERROR_SOURCE_SEEK,
  FILE_ERROR_SOURCE_FLUSH,
  FILE_ERROR_SOURCE_SET_EOF,
  FILE_ERROR_SOURCE_GET_SIZE,
  FILE_ERROR_SOURCE_COUNT,
};

// UMA error statistics gathering.
// Put the error value into a bucket.
int GetFileErrorUmaBucket(int error);

// The largest bucket number, plus 1.
int MaxFileErrorUmaBucket();

// The highest error value we want to individually report.
int MaxFileErrorUmaValue();

// |error| is a platform-specific error (Windows or Posix).
// |source| indicates the operation that resulted in the error.
// |record| is a flag indicating that we are interested in this error.
void RecordFileError(int error, FileErrorSource source, bool record);

// Gets a description for the source of a file error.
const char* GetFileErrorSourceName(FileErrorSource source);

}  // namespace net

#endif  // NET_BASE_FILE_STREAM_METRICS_H_
