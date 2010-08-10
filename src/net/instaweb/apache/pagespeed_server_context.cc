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
#include "third_party/apache/apr/src/include/apr_pools.h"

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
  context->rewrite_driver_factory()->set_combine_css(config->combine_css);
  context->rewrite_driver_factory()->set_outline_css(config->outline_css);
  context->rewrite_driver_factory()->set_outline_javascript(
      config->outline_javascript);
  context->rewrite_driver_factory()->set_rewrite_images(config->rewrite_images);
  context->rewrite_driver_factory()->set_extend_cache(config->extend_cache);
  context->rewrite_driver_factory()->set_add_head(config->add_head);
  context->rewrite_driver_factory()->set_add_base_tag(config->add_base_tag);
  context->rewrite_driver_factory()->set_remove_quotes(config->remove_quotes);
  context->rewrite_driver_factory()->set_force_caching(config->force_caching);
  context->rewrite_driver_factory()->set_move_css_to_head(
      config->move_css_to_head);
  context->rewrite_driver_factory()->set_elide_attributes(
      config->elide_attributes);
  context->rewrite_driver_factory()->set_remove_comments(
      config->remove_comments);
  context->rewrite_driver_factory()->set_collapse_whitespace(
      config->collapse_whitespace);
  return true;
}

}  // namespace html_rewriter
