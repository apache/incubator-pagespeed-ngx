// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_PROGRESS_H_
#define NET_BASE_UPLOAD_PROGRESS_H_

#include "base/basictypes.h"

namespace net {

class UploadProgress {
 public:
  UploadProgress() : size_(0), position_(0) {}
  UploadProgress(uint64 position, uint64 size)
      : size_(size), position_(position) {}

  uint64 size() const { return size_; }
  uint64 position() const { return position_; }

 private:
  uint64 size_;
  uint64 position_;
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_PROGRESS_H_
