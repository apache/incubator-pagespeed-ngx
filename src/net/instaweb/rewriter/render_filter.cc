/*
 * Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/render_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"

namespace net_instaweb {

RenderFilter::RenderFilter(RewriteDriver* driver)
    : driver_(driver) {
}

RenderFilter::~RenderFilter() {
}

void RenderFilter::Flush() {
  // TODO(jmarantz): Call some method supplied by the environment to allow
  // rewrites to finish.  E.g. in apache we could call SerfUrlAsyncFetcher.
  // Dependening on the caching implementation, this method could add
  // constrained delays to allow fetches to complete so that cached rewrites
  // can be rendered.
  driver_->Render();
}

}  // namespace net_instaweb
