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

#ifndef NET_INSTAWEB_APACHE_PAGESPEED_SERVER_CONTEXT_H_
#define NET_INSTAWEB_APACHE_PAGESPEED_SERVER_CONTEXT_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/apache/apache_rewrite_driver_factory.h"

// Forward declaration.
struct apr_pool_t;

namespace html_rewriter {

class PageSpeedServerContext;
struct PageSpeedConfig {
  PageSpeedServerContext* context;
  bool pagespeed_enable;
  const char* rewrite_url_prefix;
  const char* fetch_proxy;
  const char* generated_file_prefix;
  const char* file_cache_path;
  int64 file_cache_clean_interval_ms;
  int64 file_cache_size_kb;
  int64 fetcher_timeout_ms;
  int64 resource_timeout_ms;
  int num_shards;
  int outline_threshold;
  bool use_http_cache;
  int64 lru_cache_kb_per_process;
  int64 lru_cache_byte_limit;
  bool force_caching;
  const char* rewriters;
  const char* slurp_directory;
  bool slurp_write;
};

class PageSpeedServerContext {
 public:
  explicit PageSpeedServerContext(apr_pool_t* pool, PageSpeedConfig* config);
  ~PageSpeedServerContext();
  void set_rewrite_driver_factory(
      net_instaweb::ApacheRewriteDriverFactory* factory) {
    delete rewrite_driver_factory_;
    rewrite_driver_factory_ = factory;
//    rewrite_driver_factory_.reset(factory);
  }
  net_instaweb::ApacheRewriteDriverFactory* rewrite_driver_factory() {
    return rewrite_driver_factory_;
//    return rewrite_driver_factory_.get();
  }
  apr_pool_t* pool() { return pool_; }
  const PageSpeedConfig* config() const { return config_; }

 private:
  apr_pool_t* pool_;
  PageSpeedConfig* config_;
// scoped_ptr<net_instaweb::ApacheRewriteDriverFactory> rewrite_driver_factory_;
  net_instaweb::ApacheRewriteDriverFactory* rewrite_driver_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageSpeedServerContext);
};

bool CreatePageSpeedServerContext(apr_pool_t* pool, PageSpeedConfig* config);

}  // namespace html_rewriter

#endif  // NET_INSTAWEB_APACHE_PAGESPEED_SERVER_CONTEXT_H_
