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

#ifndef NGX_REWRITE_DRIVER_FACTORY_H_
#define NGX_REWRITE_DRIVER_FACTORY_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "apr_pools.h"

namespace net_instaweb {

class AbstractSharedMem;
class SlowWorker;
class StaticJavaScriptManager;
class NgxServerContext;
class AprMemCache;
class NgxCache;
class NgxRewriteOptions;
class AprMemCache;

class NgxRewriteDriverFactory : public RewriteDriverFactory {
 public:
  static const char kStaticJavaScriptPrefix[];

  NgxRewriteDriverFactory();
  virtual ~NgxRewriteDriverFactory();
  virtual Hasher* NewHasher();
  virtual UrlFetcher* DefaultUrlFetcher();
  virtual UrlAsyncFetcher* DefaultAsyncUrlFetcher();
  virtual MessageHandler* DefaultHtmlParseMessageHandler();
  virtual MessageHandler* DefaultMessageHandler();
  virtual FileSystem* DefaultFileSystem();
  virtual Timer* DefaultTimer();
  virtual NamedLockManager* DefaultLockManager();
  virtual void SetupCaches(ServerContext* resource_manager);
  virtual Statistics* statistics();
  // Create a new RewriteOptions.  In this implementation it will be an
  // NgxRewriteOptions.
  virtual RewriteOptions* NewRewriteOptions();
  // Initializes the StaticJavascriptManager.
  virtual void InitStaticJavascriptManager(
      StaticJavascriptManager* static_js_manager);

  AbstractSharedMem* shared_mem_runtime() const {
    return shared_mem_runtime_.get();
  }

  SlowWorker* slow_worker() { return slow_worker_.get(); }

  // Create a new AprMemCache from the given hostname[:port] specification.
  AprMemCache* NewAprMemCache(const GoogleString& spec);

  // Finds a Cache for the file_cache_path in the config.  If none exists,
  // creates one, using all the other parameters in the ApacheConfig.
  // Currently, no checking is done that the other parameters (e.g. cache
  // size, cleanup interval, etc.) are consistent.
  NgxCache* GetCache(NgxRewriteOptions* config);
  
 private:
  SimpleStats simple_stats_;
  Timer* timer_;
  apr_pool_t* pool_;
  scoped_ptr<SlowWorker> slow_worker_;
  scoped_ptr<AbstractSharedMem> shared_mem_runtime_;
  typedef std::map<GoogleString, NgxCache*> PathCacheMap;
  PathCacheMap path_cache_map_;
  
  DISALLOW_COPY_AND_ASSIGN(NgxRewriteDriverFactory);
};

} // namespace net_instaweb

#endif  // NGX_REWRITE_DRIVER_FACTORY_H_
