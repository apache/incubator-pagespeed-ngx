// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_REQUEST_PRIORITY_H__
#define NET_BASE_REQUEST_PRIORITY_H__

namespace net {

// Prioritization used in various parts of the networking code such
// as connection prioritization and resource loading prioritization.
enum RequestPriority {
  MINIMUM_PRIORITY = 0,
  IDLE = 0,
  LOWEST,
  LOW,
  MEDIUM,
  HIGHEST,
  NUM_PRIORITIES,
};

}  // namespace net

#endif  // NET_BASE_REQUEST_PRIORITY_H__
