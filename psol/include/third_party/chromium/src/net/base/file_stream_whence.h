// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FILE_STREAM_WHENCE_H_
#define NET_BASE_FILE_STREAM_WHENCE_H_

namespace net {

// TODO(darin): Move this to a more generic location.
// This explicit mapping matches both FILE_ on Windows and SEEK_ on Linux.
enum Whence {
  FROM_BEGIN   = 0,
  FROM_CURRENT = 1,
  FROM_END     = 2
};

}  // namespace net

#endif  // NET_BASE_FILE_STREAM_WHENCE_H_
