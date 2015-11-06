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

// Author: morlovich@google.com (Maksim Orlovich)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_POOL_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_POOL_H_

#include <vector>

#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

class RewriteDriver;
class RewriteOptions;

// A class for managing recycling of RewriteDrivers with standard options.
// Note that this class by itself is not threadsafe, as ServerContext
// takes care of that.
class RewriteDriverPool  {
 public:
  RewriteDriverPool();

  // Deletes all drivers in the pool.
  virtual ~RewriteDriverPool();

  virtual const RewriteOptions* TargetOptions() const = 0;

  // Return a driver from freelist, or NULL.
  RewriteDriver* PopDriver();

  // Stores the driver on freelist, and Clear()s it for reuse.
  void RecycleDriver(RewriteDriver* driver);

 private:
  std::vector<RewriteDriver*> drivers_;

  // Don't allow more than this many drivers in the pool. The pool is an
  // optimisation to save the cost of constructing a RewriteDriver, but keeping
  // a lot of them lying around winds up wasting a lot of memory instead.
  static const int kMaxDriversInPool = 50;

  DISALLOW_COPY_AND_ASSIGN(RewriteDriverPool);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_POOL_H_
