// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: No header guards are used, since this file is intended to be expanded
// directly within a block where the SOURCE_TYPE macro is defined.

// Used for global events which don't correspond to a particular entity.
SOURCE_TYPE(NONE, 0)
SOURCE_TYPE(URL_REQUEST, 1)
SOURCE_TYPE(SOCKET_STREAM, 2)
SOURCE_TYPE(INIT_PROXY_RESOLVER, 3)
SOURCE_TYPE(CONNECT_JOB, 4)
SOURCE_TYPE(SOCKET, 5)
SOURCE_TYPE(SPDY_SESSION, 6)
SOURCE_TYPE(HOST_RESOLVER_IMPL_REQUEST, 7)
SOURCE_TYPE(HOST_RESOLVER_IMPL_JOB, 8)
SOURCE_TYPE(DISK_CACHE_ENTRY, 9)
SOURCE_TYPE(MEMORY_CACHE_ENTRY, 10)
SOURCE_TYPE(HTTP_STREAM_JOB, 11)
SOURCE_TYPE(EXPONENTIAL_BACKOFF_THROTTLING, 12)

SOURCE_TYPE(COUNT, 13)  // Always keep this as the last entry.
