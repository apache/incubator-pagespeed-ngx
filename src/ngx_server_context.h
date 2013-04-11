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

// Manage pagespeed state across requests.  Compare to ApacheResourceManager.

#ifndef NGX_SERVER_CONTEXT_H_
#define NGX_SERVER_CONTEXT_H_

#include "net/instaweb/system/public/system_server_context.h"

namespace net_instaweb {

class NgxRewriteDriverFactory;
class NgxRewriteOptions;
class RewriteStats;
class SharedMemStatistics;
class Statistics;

class NgxServerContext : public SystemServerContext {
 public:
  explicit NgxServerContext(NgxRewriteDriverFactory* factory);
  virtual ~NgxServerContext();

  // We expect to use ProxyFetch with HTML.
  virtual bool ProxiesHtml() const { return true; }

  // Call only when you need an NgxRewriteOptions.  If you don't need
  // nginx-specific behavior, call global_options() instead which doesn't
  // downcast.
  NgxRewriteOptions* config();
  // Should be called after the child process is forked.
  void ChildInit();
  // Initialize this ServerContext to have its own statistics domain.
  // Must be called after global_statistics has been created and had
  // ::Initialize called on it.
  void CreateLocalStatistics(Statistics* global_statistics);
  static void InitStats(Statistics* statistics);
  virtual void ApplySessionFetchers(const RequestContextPtr& req,
                                    RewriteDriver* driver);
  bool initialized() const { return initialized_; }
  GoogleString hostname_identifier() { return hostname_identifier_; }
  void set_hostname_identifier(GoogleString x) { hostname_identifier_ = x; }
  NgxRewriteDriverFactory* ngx_rewrite_driver_factory() { return ngx_factory_; }

 private:
  NgxRewriteDriverFactory* ngx_factory_;
  // hostname_identifier_ is used to distinguish the name of shared memory
  // segments associated with this ServerContext
  GoogleString hostname_identifier_;
  bool initialized_;

  // Non-NULL if we have per-vhost stats.
  scoped_ptr<Statistics> split_statistics_;

  // May be NULL. Owned by *split_statistics_.
  SharedMemStatistics* local_statistics_;
  // These are non-NULL if we have per-vhost stats.
  scoped_ptr<RewriteStats> local_rewrite_stats_;

  DISALLOW_COPY_AND_ASSIGN(NgxServerContext);
};

}  // namespace net_instaweb

#endif  // NGX_SERVER_CONTEXT_H_
