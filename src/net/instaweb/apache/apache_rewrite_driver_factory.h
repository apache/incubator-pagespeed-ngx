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
//
// Author: jmarantz@google.com (Joshua Marantz)
//         lsong@google.com (Libo Song)

#ifndef NET_INSTAWEB_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_
#define NET_INSTAWEB_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_

// Note: We must include apache_config.h to allow using ApacheConfig*
// return-types for functions that return RewriteOptions* in base class.
#include "net/instaweb/apache/apache_config.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/system/public/system_rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

struct apr_pool_t;
struct server_rec;

namespace net_instaweb {

class ApacheMessageHandler;
class ApacheServerContext;
class MessageHandler;
class ModSpdyFetchController;
class ProcessContext;
class QueuedWorkerPool;
class ServerContext;
class SharedCircularBuffer;
class SlowWorker;
class Statistics;
class Timer;

// Creates an Apache RewriteDriver.
class ApacheRewriteDriverFactory : public SystemRewriteDriverFactory {
 public:
  ApacheRewriteDriverFactory(const ProcessContext& process_context,
                             server_rec* server, const StringPiece& version);
  virtual ~ApacheRewriteDriverFactory();

  // Give access to apache_message_handler_ for the cases we need
  // to use ApacheMessageHandler rather than MessageHandler.
  // e.g. Use ApacheMessageHandler::Dump()
  // This is a better choice than cast from MessageHandler.
  ApacheMessageHandler* apache_message_handler() {
    return apache_message_handler_;
  }

  virtual void ChildInit();

  virtual void NonStaticInitStats(Statistics* statistics) {
    InitStats(statistics);
  }

  ApacheServerContext* MakeApacheServerContext(server_rec* server);

  void set_num_rewrite_threads(int x) { num_rewrite_threads_ = x; }
  int num_rewrite_threads() const { return num_rewrite_threads_; }
  void set_num_expensive_rewrite_threads(int x) {
    num_expensive_rewrite_threads_ = x;
  }
  int num_expensive_rewrite_threads() const {
    return num_expensive_rewrite_threads_;
  }

  virtual bool use_per_vhost_statistics() const {
    return use_per_vhost_statistics_;
  }

  void set_use_per_vhost_statistics(bool x) {
    use_per_vhost_statistics_ = x;
  }

  virtual bool enable_property_cache() const {
    return enable_property_cache_;
  }

  void set_enable_property_cache(bool x) {
    enable_property_cache_ = x;
  }

  // If true, virtual hosts should inherit global configuration.
  bool inherit_vhost_config() const {
    return inherit_vhost_config_;
  }

  void set_inherit_vhost_config(bool x) {
    inherit_vhost_config_ = x;
  }

  bool install_crash_handler() const {
    return install_crash_handler_;
  }

  void set_install_crash_handler(bool x) {
    install_crash_handler_ = x;
  }

  // mod_pagespeed uses a beacon handler to collect data for critical images,
  // css, etc., so filters should be configured accordingly.
  virtual bool UseBeaconResultsInFilters() const {
    return true;
  }

  // Notification of apache tearing down a context (vhost or top-level)
  // corresponding to given ApacheServerContext. Returns true if it was
  // the last context.
  bool PoolDestroyed(ApacheServerContext* rm);

  virtual ApacheConfig* NewRewriteOptions();

  // As above, but set a name on the ApacheConfig noting that it came from
  // a query.
  virtual ApacheConfig* NewRewriteOptionsForQuery();

  // Initializes all the statistics objects created transitively by
  // ApacheRewriteDriverFactory, including apache-specific and
  // platform-independent statistics.
  static void InitStats(Statistics* statistics);
  static void Initialize();
  static void Terminate();

  ModSpdyFetchController* mod_spdy_fetch_controller() {
    return mod_spdy_fetch_controller_.get();
  }

  // Needed by mod_instaweb.cc:ParseDirective().
  virtual void set_message_buffer_size(int x) {
    SystemRewriteDriverFactory::set_message_buffer_size(x);
  }

  // Override requests_per_host to take num_rewrite_threads_ into account.
  virtual int requests_per_host();

 protected:
  // Provide defaults.
  virtual MessageHandler* DefaultHtmlParseMessageHandler();
  virtual MessageHandler* DefaultMessageHandler();
  virtual Timer* DefaultTimer();
  virtual void SetupCaches(ServerContext* server_context);
  virtual QueuedWorkerPool* CreateWorkerPool(WorkerPoolCategory pool,
                                             StringPiece name);

  // Disable the Resource Manager's filesystem since we have a
  // write-through http_cache.
  virtual bool ShouldWriteResourcesToFileSystem() { return false; }

  virtual void ParentOrChildInit();

  virtual void SetupMessageHandlers();
  virtual void ShutDownMessageHandlers();
  virtual void ShutDownFetchers();

  virtual void SetCircularBuffer(SharedCircularBuffer* buffer);

  virtual ServerContext* NewDecodingServerContext();

 private:
  // Updates num_rewrite_threads_ and num_expensive_rewrite_threads_
  // with sensible values if they are not explicitly set.
  void AutoDetectThreadCounts();

  apr_pool_t* pool_;
  server_rec* server_rec_;
  scoped_ptr<SlowWorker> slow_worker_;

  // TODO(jmarantz): These options could be consolidated in a protobuf or
  // some other struct, which would keep them distinct from the rest of the
  // state.  Note also that some of the options are in the base class,
  // RewriteDriverFactory, so we'd have to sort out how that worked.
  GoogleString version_;

  // This will be assigned to message_handler_ when message_handler() or
  // html_parse_message_handler is invoked for the first time.
  // We keep an extra link because we need to refer them as
  // ApacheMessageHandlers rather than just MessageHandler in initialization
  // process.
  ApacheMessageHandler* apache_message_handler_;
  // This will be assigned to html_parse_message_handler_ when
  // html_parse_message_handler() is invoked for the first time.
  // Note that apache_message_handler_ and apache_html_parse_message_handler
  // writes to the same shared memory which is owned by the factory.
  ApacheMessageHandler* apache_html_parse_message_handler_;

  // If true, we'll have a separate statistics object for each vhost
  // (along with a global aggregate), rather than just a single object
  // aggregating all of them.
  bool use_per_vhost_statistics_;

  // Enable the property cache.
  bool enable_property_cache_;

  // Inherit configuration from global context into vhosts.
  bool inherit_vhost_config_;

  // If true, we'll install a signal handler that prints backtraces.
  bool install_crash_handler_;

  // true iff we ran through AutoDetectThreadCounts()
  bool thread_counts_finalized_;

  // These are <= 0 if we should autodetect.
  int num_rewrite_threads_;
  int num_expensive_rewrite_threads_;

  int max_mod_spdy_fetch_threads_;

  // Helps coordinate direct-to-mod_spdy fetches.
  scoped_ptr<ModSpdyFetchController> mod_spdy_fetch_controller_;

  DISALLOW_COPY_AND_ASSIGN(ApacheRewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_
