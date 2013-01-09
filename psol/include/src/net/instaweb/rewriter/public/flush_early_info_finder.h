/*
 * Copyright 2012 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: mmohabey@google.com (Megha Mohabey)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_FLUSH_EARLY_INFO_FINDER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_FLUSH_EARLY_INFO_FINDER_H_

#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class FlushEarlyRenderInfo;
class RewriteDriver;

// Finds a subset of flush early information which may be used by
// FlushEarlyFlow. This includes information like privately cacheable resources,
// charset.
class FlushEarlyInfoFinder {
 public:
  static const char kFlushEarlyRenderPropertyName[];

  FlushEarlyInfoFinder() {}
  virtual ~FlushEarlyInfoFinder();

  // Checks whether GetCharset will return meaningful result. The default
  // implementation does not, but classes inheriting likely do. Users of
  // GetCharset should check this function and supply a default behavior if
  // IsMeaningful returns false.
  virtual bool IsMeaningful() const {
    return false;
  }

  // Gets the flush early info and update the RewriteDriver.
  virtual void UpdateFlushEarlyInfoInDriver(RewriteDriver* driver);

  // Computes the flush early info.
  virtual void ComputeFlushEarlyInfo(RewriteDriver* driver);

  // Gets the charset of the html document. Users of this function should also
  // check IsMeaningful() to see if the implementation of this function returns
  // meaningful results and provide a default behavior if it does not.
  virtual const char* GetCharset(const RewriteDriver* driver);

  virtual const char* GetCohort() const = 0;

  virtual int64 cache_expiration_time_ms() const = 0;

 protected:
  void UpdateFlushEarlyInfoCacheEntry(
      RewriteDriver* driver,
      FlushEarlyRenderInfo* flush_early_render_info);

 private:
  DISALLOW_COPY_AND_ASSIGN(FlushEarlyInfoFinder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FLUSH_EARLY_INFO_FINDER_H_
