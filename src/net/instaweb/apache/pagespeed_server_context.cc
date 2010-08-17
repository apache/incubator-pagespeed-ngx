// Copyright 2010 Google Inc.
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

#include "net/instaweb/apache/pagespeed_server_context.h"

#include "net/instaweb/apache/apr_mutex.h"
#include "apr_pools.h"

using net_instaweb::ApacheRewriteDriverFactory;

namespace html_rewriter {

PageSpeedServerContext::PageSpeedServerContext(apr_pool_t* pool,
                                               PageSpeedConfig* config)
    : pool_(pool),
      config_(config) {
  rewrite_driver_factory_ = NULL;
}

PageSpeedServerContext::~PageSpeedServerContext() {
  delete rewrite_driver_factory_;
}

bool CreatePageSpeedServerContext(apr_pool_t* pool, PageSpeedConfig* config) {
  if (config->context != NULL) {
    // Already created. Don't created again.
    return false;
  }
  PageSpeedServerContext* context = new PageSpeedServerContext(pool, config);
  config->context = context;
  context->set_rewrite_driver_factory(new ApacheRewriteDriverFactory(context));
  context->rewrite_driver_factory()->set_num_shards(config->num_shards);
  context->rewrite_driver_factory()->set_use_http_cache(config->use_http_cache);
  context->rewrite_driver_factory()->set_use_threadsafe_cache(
      config->use_threadsafe_cache);
  context->rewrite_driver_factory()->SetEnabledFilters(config->rewriters);
  context->rewrite_driver_factory()->set_force_caching(config->force_caching);
  return true;
}

}  // namespace html_rewriter
