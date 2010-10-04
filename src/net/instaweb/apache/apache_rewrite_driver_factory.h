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
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"

struct apr_pool_t;
struct server_rec;

namespace html_rewriter {
class PageSpeedServerContext;
}  // namespace html_rewriter.

namespace net_instaweb {

class SerfUrlAsyncFetcher;
class SerfUrlFetcher;

// Creates an Apache RewriteDriver.
class ApacheRewriteDriverFactory : public RewriteDriverFactory {
 public:
  explicit ApacheRewriteDriverFactory(
      html_rewriter::PageSpeedServerContext* context);
  virtual ~ApacheRewriteDriverFactory();

  RewriteDriver* GetRewriteDriver();
  void ReleaseRewriteDriver(RewriteDriver* rewrite_driver);

  virtual Hasher* NewHasher();
  virtual AbstractMutex* NewMutex();

  SerfUrlAsyncFetcher* serf_url_async_fetcher() {
    return serf_url_async_fetcher_;
  }

  void set_lru_cache_kb_per_process(int64 x) { lru_cache_kb_per_process_ = x; }
  void set_lru_cache_byte_limit(int64 x) { lru_cache_byte_limit_ = x; }

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
  virtual bool ShouldWriteResourcesToFileSystem() {
    return false;
  }

  // Release all the resources. It also calls the base class ShutDown to release
  // the base class resources.
  void ShutDown();

 private:
  html_rewriter::PageSpeedServerContext* context_;
  apr_pool_t* pool_;
  scoped_ptr<AbstractMutex> cache_mutex_;
  scoped_ptr<AbstractMutex> rewrite_drivers_mutex_;
  std::vector<RewriteDriver*> available_rewrite_drivers_;
  std::set<RewriteDriver*> active_rewrite_drivers_;
  SerfUrlFetcher* serf_url_fetcher_;
  SerfUrlAsyncFetcher* serf_url_async_fetcher_;
  int64 lru_cache_kb_per_process_;
  int64 lru_cache_byte_limit_;

  DISALLOW_COPY_AND_ASSIGN(ApacheRewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_
