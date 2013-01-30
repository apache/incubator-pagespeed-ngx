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

// Author: jefftk@google.com (Jeff Kaufman)

#include "ngx_server_context.h"

#include "ngx_cache.h"
#include "ngx_rewrite_options.h"
#include "ngx_rewrite_driver_factory.h"

namespace net_instaweb {

NgxServerContext::NgxServerContext(NgxRewriteDriverFactory* factory)
    : ServerContext(factory),
      ngx_factory_(factory),
      initialized_(false) {
}

NgxServerContext::~NgxServerContext() {
}

NgxRewriteOptions* NgxServerContext::config() {
  return NgxRewriteOptions::DynamicCast(global_options());
}

void NgxServerContext::ChildInit() {
  DCHECK(!initialized_);
  if (!initialized_) {
    initialized_ = true;
    NgxCache* cache = ngx_factory_->GetCache(config());
    set_lock_manager(cache->lock_manager());
    ngx_factory_->InitServerContext(this);
  }
}

}  // namespace net_instaweb
