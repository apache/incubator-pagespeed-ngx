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

#ifndef PAGESPEED_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_
#define PAGESPEED_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_

// Note: We must include apache_config.h to allow using ApacheConfig*
// return-types for functions that return RewriteOptions* in base class.
#include "pagespeed/apache/apache_config.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/system/system_rewrite_driver_factory.h"

struct apr_pool_t;
struct server_rec;

namespace net_instaweb {

class ApacheMessageHandler;
class ApacheServerContext;
class MessageHandler;
class ProcessContext;
class ServerContext;
class SchedulerThread;
class SharedCircularBuffer;
class SlowWorker;
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

  virtual void NonStaticInitStats(Statistics* statistics) {
    InitStats(statistics);
  }

  ApacheServerContext* MakeApacheServerContext(server_rec* server);

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

  // Called by any ApacheServerContext whose configuration requires use of
  // a scheduler thread. This will actually start one, so should only be
  // called from child processes.
  void SetNeedSchedulerThread();

  // Needed by mod_instaweb.cc:ParseDirective().
  virtual void set_message_buffer_size(int x) {
    SystemRewriteDriverFactory::set_message_buffer_size(x);
  }

  virtual bool IsServerThreaded();
  virtual int LookupThreadLimit();

 protected:
  // Provide defaults.
  virtual MessageHandler* DefaultHtmlParseMessageHandler();
  virtual MessageHandler* DefaultMessageHandler();
  virtual Timer* DefaultTimer();
  virtual void SetupCaches(ServerContext* server_context);

  // Disable the Resource Manager's filesystem since we have a
  // write-through http_cache.
  virtual bool ShouldWriteResourcesToFileSystem() { return false; }

  virtual void ParentOrChildInit();

  virtual void SetupMessageHandlers();
  virtual void ShutDownMessageHandlers();

  virtual void SetCircularBuffer(SharedCircularBuffer* buffer);

  virtual ServerContext* NewDecodingServerContext();

 private:

  apr_pool_t* pool_;
  server_rec* server_rec_;
  scoped_ptr<SlowWorker> slow_worker_;
  SchedulerThread* scheduler_thread_;  // cleaned up with defer_cleanup

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

  DISALLOW_COPY_AND_ASSIGN(ApacheRewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_
