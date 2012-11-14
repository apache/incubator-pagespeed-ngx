// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: oschaaf@gmail.com (Otto van der Schaaf)

#ifndef NGX_CACHE_H_
#define NGX_CACHE_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

  class NgxRewriteOptions;
  class NgxRewriteDriverFactory;
  class CacheInterface;
  class FileCache;
  class FileSystemLockManager;
  class MessageHandler;
  class NamedLockManager;
  class SharedMemLockManager;

  // The NgxCache encapsulates a cache-sharing model where a user specifies
  // a file-cache path per virtual-host.  With each file-cache object we keep
  // a locking mechanism and an optional per-process LRUCache.
  class NgxCache {
 public:
    static const char kFileCache[];
    static const char kLruCache[];

    NgxCache(const StringPiece& path,
                const NgxRewriteOptions& config,
                NgxRewriteDriverFactory* factory);
    ~NgxCache();
    CacheInterface* l1_cache() { return l1_cache_.get(); }
    CacheInterface* l2_cache() { return l2_cache_.get(); }
    NamedLockManager* lock_manager() { return lock_manager_; }

    void RootInit();
    void ChildInit();
    void GlobalCleanup(MessageHandler* handler);  // only called in root process

 private:
    void FallBackToFileBasedLocking();

    GoogleString path_;

    NgxRewriteDriverFactory* factory_;
    scoped_ptr<SharedMemLockManager> shared_mem_lock_manager_;
    scoped_ptr<FileSystemLockManager> file_system_lock_manager_;
    NamedLockManager* lock_manager_;
    FileCache* file_cache_;  // owned by l2 cache
    scoped_ptr<CacheInterface> l1_cache_;
    scoped_ptr<CacheInterface> l2_cache_;
  };

  // CACHE_STATISTICS is #ifdef'd to facilitate experiments with whether
  // tracking the detailed stats & histograms has a QPS impact.  Set it
  // to 0 to turn it off.
  #define CACHE_STATISTICS 1

}  // namespace net_instaweb

#endif  // NGX_CACHE_H_
