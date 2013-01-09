// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CACHE_TYPE_H_
#define NET_BASE_CACHE_TYPE_H_
#pragma once

namespace net {

// The types of caches that can be created.
enum CacheType {
  DISK_CACHE,  // Disk is used as the backing storage.
  MEMORY_CACHE,  // Data is stored only in memory.
  MEDIA_CACHE,  // Optimized to handle media files.
  APP_CACHE  // Backing store for an AppCache.
};

}  // namespace disk_cache

#endif  // NET_BASE_CACHE_TYPE_H_
