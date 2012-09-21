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

#include "net/instaweb/rewriter/public/rewrite_driver_pool.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/stl_util.h"

namespace net_instaweb {

RewriteDriverPool::RewriteDriverPool() {}

RewriteDriverPool::~RewriteDriverPool() {
  STLDeleteElements(&drivers_);
}

RewriteDriver* RewriteDriverPool::PopDriver() {
  if (!drivers_.empty()) {
    RewriteDriver* driver = drivers_.back();
    drivers_.pop_back();
    return driver;
  }
  return NULL;
}

void RewriteDriverPool::RecycleDriver(RewriteDriver* driver) {
  drivers_.push_back(driver);
  driver->Clear();
}

}  // namespace net_instaweb
