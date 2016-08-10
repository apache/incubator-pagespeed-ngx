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

extern "C" {
  #include <ngx_auto_config.h>
#if (NGX_THREADS)
  #include <ngx_thread.h>
#endif
  #include <ngx_core.h>
  #include <ngx_http.h>
  #include <ngx_config.h>
  #include <ngx_log.h>
}

#include <set>

#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/system/system_rewrite_driver_factory.h"

namespace net_instaweb {

class NgxMessageHandler;
class NgxRewriteOptions;
class NgxServerContext;
class NgxUrlAsyncFetcher;
class SharedCircularBuffer;
class SharedMemRefererStatistics;
class SlowWorker;
class Statistics;
class SystemThreadSystem;

enum ProcessScriptVariablesMode {
  kOff,
  kLegacyRestricted,
  kAll
};

class NgxRewriteDriverFactory : public SystemRewriteDriverFactory {
 public:
  // We take ownership of the thread system.
  explicit NgxRewriteDriverFactory(
      const ProcessContext& process_context,
      SystemThreadSystem* system_thread_system, StringPiece hostname, int port);
  virtual ~NgxRewriteDriverFactory();
  virtual Hasher* NewHasher();
  virtual UrlAsyncFetcher* AllocateFetcher(SystemRewriteOptions* config);
  virtual MessageHandler* DefaultHtmlParseMessageHandler();
  virtual MessageHandler* DefaultMessageHandler();
  virtual FileSystem* DefaultFileSystem();
  virtual Timer* DefaultTimer();
  virtual NamedLockManager* DefaultLockManager();
  // Create a new RewriteOptions.  In this implementation it will be an
  // NgxRewriteOptions, and it will have CoreFilters explicitly set.
  virtual RewriteOptions* NewRewriteOptions();
  virtual RewriteOptions* NewRewriteOptionsForQuery();
  virtual ServerContext* NewDecodingServerContext();
  // Check resolver configured or not.
  bool CheckResolver();

  // Initializes all the statistics objects created transitively by
  // NgxRewriteDriverFactory, including nginx-specific and
  // platform-independent statistics.
  static void InitStats(Statistics* statistics);
  NgxServerContext* MakeNgxServerContext(StringPiece hostname, int port);
  virtual ServerContext* NewServerContext();
  virtual void ShutDown();

  // Starts pagespeed threads if they've not been started already.  Must be
  // called after the caller has finished any forking it intends to do.
  void StartThreads();

  void SetServerContextMessageHandler(ServerContext* server_context,
                                      ngx_log_t* log);

  NgxMessageHandler* ngx_message_handler() { return ngx_message_handler_; }

  virtual void NonStaticInitStats(Statistics* statistics) {
    InitStats(statistics);
  }

  void SetMainConf(NgxRewriteOptions* main_conf);

  void set_resolver(ngx_resolver_t* resolver) {
    resolver_ = resolver;
  }
  void set_resolver_timeout(ngx_msec_t resolver_timeout) {
    resolver_timeout_ = resolver_timeout == NGX_CONF_UNSET_MSEC ?
        1000 : resolver_timeout;
  }
  bool use_native_fetcher() {
    return use_native_fetcher_;
  }
  void set_use_native_fetcher(bool x) {
    use_native_fetcher_ = x;
  }
  int native_fetcher_max_keepalive_requests() {
    return native_fetcher_max_keepalive_requests_;
  }
  void set_native_fetcher_max_keepalive_requests(int x) {
    native_fetcher_max_keepalive_requests_ = x;
  }
  ProcessScriptVariablesMode process_script_variables() {
    return process_script_variables_mode_;
  }

  void LoggingInit(ngx_log_t* log, bool may_install_crash_handler);

  virtual void ShutDownMessageHandlers();

  virtual void SetCircularBuffer(SharedCircularBuffer* buffer);

  bool SetProcessScriptVariables(ProcessScriptVariablesMode mode) {
    if (!process_script_variables_set_) {
      process_script_variables_mode_ = mode;
      process_script_variables_set_ = true;
      return true;
    }
    return false;
  }

  virtual void PrepareForkedProcess(const char* name);

  virtual void NameProcess(const char* name);

 private:
  Timer* timer_;

  bool threads_started_;
  NgxMessageHandler* ngx_message_handler_;
  NgxMessageHandler* ngx_html_parse_message_handler_;

  std::vector<NgxUrlAsyncFetcher*> ngx_url_async_fetchers_;
  ngx_log_t* log_;
  ngx_msec_t resolver_timeout_;
  ngx_resolver_t* resolver_;
  bool use_native_fetcher_;
  int native_fetcher_max_keepalive_requests_;

  typedef std::set<NgxMessageHandler*> NgxMessageHandlerSet;
  NgxMessageHandlerSet server_context_message_handlers_;

  // Owned by the superclass.
  // TODO(jefftk): merge the nginx and apache ways of doing this.
  SharedCircularBuffer* ngx_shared_circular_buffer_;

  GoogleString hostname_;
  int port_;
  ProcessScriptVariablesMode process_script_variables_mode_;
  bool process_script_variables_set_;
  bool shut_down_;

  DISALLOW_COPY_AND_ASSIGN(NgxRewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NGX_REWRITE_DRIVER_FACTORY_H_
