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

#ifndef NET_INSTAWEB_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_
#define NET_INSTAWEB_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_

#include <stdio.h>
#include <set>
#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"

struct apr_pool_t;
struct server_rec;

namespace net_instaweb {

class AprStatistics;
class SerfUrlAsyncFetcher;
class SerfUrlFetcher;

// Creates an Apache RewriteDriver.
class ApacheRewriteDriverFactory : public RewriteDriverFactory {
 public:
  explicit ApacheRewriteDriverFactory(apr_pool_t* pool, server_rec* server,
                                      const StringPiece& version);
  virtual ~ApacheRewriteDriverFactory();

  virtual Hasher* NewHasher();
  virtual AbstractMutex* NewMutex();

  SerfUrlAsyncFetcher* serf_url_async_fetcher() {
    return serf_url_async_fetcher_;
  }

  void set_lru_cache_kb_per_process(int64 x) { lru_cache_kb_per_process_ = x; }
  void set_lru_cache_byte_limit(int64 x) { lru_cache_byte_limit_ = x; }
  void set_slurp_flush_limit(int64 x) { slurp_flush_limit_ = x; }
  int slurp_flush_limit() const { return slurp_flush_limit_; }
  void set_file_cache_clean_interval_ms(int64 x) {
    file_cache_clean_interval_ms_ = x;
  }
  void set_file_cache_clean_size_kb(int64 x) { file_cache_clean_size_kb_ = x; }
  void set_fetcher_time_out_ms(int64 x) { fetcher_time_out_ms_ = x; }
  void set_file_cache_path(const StringPiece& x) {
    x.CopyToString(&file_cache_path_);
  }
  void set_fetcher_proxy(const StringPiece& x) {
    x.CopyToString(&fetcher_proxy_);
  }
  void set_enabled(bool x) { enabled_ = x; }
  bool enabled() { return enabled_; }

  StringPiece file_cache_path() { return file_cache_path_; }
  int64 file_cache_clean_size_kb() { return file_cache_clean_size_kb_; }
  int64 fetcher_time_out_ms() { return fetcher_time_out_ms_; }
  AprStatistics* statistics() { return statistics_; }
  void set_statistics(AprStatistics* x) { statistics_ = x; }

 protected:
  virtual UrlFetcher* DefaultUrlFetcher();
  virtual UrlAsyncFetcher* DefaultAsyncUrlFetcher();

  // Provide defaults.
  virtual MessageHandler* DefaultHtmlParseMessageHandler();
  virtual MessageHandler* DefaultMessageHandler();
  virtual FileSystem* DefaultFileSystem();
  virtual HtmlParse* DefaultHtmlParse();
  virtual Timer* DefaultTimer();
  virtual CacheInterface* DefaultCacheInterface();
  virtual AbstractMutex* cache_mutex() { return cache_mutex_.get(); }
  virtual AbstractMutex* rewrite_drivers_mutex() {
    return rewrite_drivers_mutex_.get(); }

  // Disable the Resource Manager's filesystem since we have a
  // write-through http_cache.
  virtual bool ShouldWriteResourcesToFileSystem() { return false; }

  // When computing the resource manager for Apache, be sure to set up
  // the statistics.
  virtual ResourceManager* ComputeResourceManager();

  // Release all the resources. It also calls the base class ShutDown to release
  // the base class resources.
  void ShutDown();

 private:
  apr_pool_t* pool_;
  server_rec* server_rec_;
  scoped_ptr<AbstractMutex> cache_mutex_;
  scoped_ptr<AbstractMutex> rewrite_drivers_mutex_;
  SerfUrlFetcher* serf_url_fetcher_;
  SerfUrlAsyncFetcher* serf_url_async_fetcher_;
  AprStatistics* statistics_;

  // TODO(jmarantz): These options could be consolidated in a protobuf or
  // some other struct, which would keep them distinct from the rest of the
  // state.  Note also that some of the options are in the base class,
  // RewriteDriverFactory, so we'd have to sort out how that worked.
  int64 lru_cache_kb_per_process_;
  int64 lru_cache_byte_limit_;
  int64 file_cache_clean_interval_ms_;
  int64 file_cache_clean_size_kb_;
  int64 fetcher_time_out_ms_;
  int64 slurp_flush_limit_;
  std::string file_cache_path_;
  std::string fetcher_proxy_;
  std::string version_;
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(ApacheRewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_
