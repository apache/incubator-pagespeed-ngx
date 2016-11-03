// Copyright 2013 Google Inc.
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
// Author: jefftk@google.com (Jeff Kaufman)

#include "pagespeed/system/system_rewrite_driver_factory.h"

#include <sys/prctl.h>
#include <algorithm>  // for min
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <utility>  // for pair

#include "apr_general.h"
#include "base/logging.h"
#include "net/instaweb/http/public/http_dump_url_async_writer.h"
#include "net/instaweb/http/public/http_dump_url_fetcher.h"
#include "net/instaweb/http/public/rate_controller.h"
#include "net/instaweb/http/public/rate_controlling_url_async_fetcher.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "pagespeed/controller/central_controller_rpc_client.h"
#include "pagespeed/controller/central_controller_rpc_server.h"
#include "pagespeed/controller/popularity_contest_schedule_rewrite_controller.h"
#include "pagespeed/controller/queued_expensive_operation_controller.h"
#include "pagespeed/system/controller_manager.h"
#include "pagespeed/system/controller_process.h"
#include "pagespeed/system/in_place_resource_recorder.h"
#include "pagespeed/system/serf_url_async_fetcher.h"
#include "pagespeed/system/system_caches.h"
#include "pagespeed/system/system_rewrite_options.h"
#include "pagespeed/system/system_server_context.h"
#include "pagespeed/system/system_thread_system.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/null_shared_mem.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/sharedmem/shared_circular_buffer.h"
#include "pagespeed/kernel/sharedmem/shared_mem_statistics.h"
#include "pagespeed/kernel/thread/pthread_shared_mem.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"
#include "pagespeed/kernel/util/input_file_nonce_generator.h"
#include "pagespeed/kernel/util/nonce_generator.h"

namespace net_instaweb {

class ProcessContext;

namespace {

const char kShutdownCount[] = "child_shutdown_count";

const char kStaticAssetPrefix[] = "StaticAssetPrefix";
const char kUsePerVHostStatistics[] = "UsePerVHostStatistics";
const char kInstallCrashHandler[] = "InstallCrashHandler";
const char kNumRewriteThreads[] = "NumRewriteThreads";
const char kNumExpensiveRewriteThreads[] = "NumExpensiveRewriteThreads";
const char kForceCaching[] = "ForceCaching";
const char kListOutstandingUrlsOnError[] = "ListOutstandingUrlsOnError";
const char kMessageBufferSize[] = "MessageBufferSize";
const char kTrackOriginalContentLength[] = "TrackOriginalContentLength";
const char kCreateSharedMemoryMetadataCache[] =
    "CreateSharedMemoryMetadataCache";

}  // namespace

SystemRewriteDriverFactory::SystemRewriteDriverFactory(
    const ProcessContext& process_context,
    SystemThreadSystem* thread_system,
    AbstractSharedMem* shared_mem_runtime, /* may be null */
    StringPiece hostname, int port)
    : RewriteDriverFactory(process_context, thread_system),
      statistics_frozen_(false),
      is_root_process_(true),
      hostname_identifier_(StrCat(hostname, ":", IntegerToString(port))),
      message_buffer_size_(0),
      track_original_content_length_(false),
      list_outstanding_urls_on_error_(false),
      static_asset_prefix_("/pagespeed_static/"),
      system_thread_system_(thread_system),
      use_per_vhost_statistics_(true),
      install_crash_handler_(false),
      thread_counts_finalized_(false),
      num_rewrite_threads_(-1),
      num_expensive_rewrite_threads_(-1) {
  if (shared_mem_runtime == NULL) {
#ifdef PAGESPEED_SUPPORT_POSIX_SHARED_MEM
    shared_mem_runtime = new PthreadSharedMem();
#else
    shared_mem_runtime = new NullSharedMem();
#endif
  }
  shared_mem_runtime_.reset(shared_mem_runtime);
}

// We need an Init() method to finish construction because we want to call
// virtual methods that subclasses can override.
void SystemRewriteDriverFactory::Init() {
  // Note: in Apache this must run after mod_pagespeed_register_hooks has
  // completed.  See http://httpd.apache.org/docs/2.4/developer/new_api_2_4.html
  // and search for ap_mpm_query.
  AutoDetectThreadCounts();

  int thread_limit = LookupThreadLimit();
  thread_limit += num_rewrite_threads() + num_expensive_rewrite_threads();
  caches_.reset(
      new SystemCaches(this, shared_mem_runtime_.get(), thread_limit));
}

SystemRewriteDriverFactory::~SystemRewriteDriverFactory() {
  shared_mem_statistics_.reset(NULL);
}

void SystemRewriteDriverFactory::InitApr() {
  apr_initialize();
  atexit(apr_terminate);
}

// Initializes global statistics object if needed, using factory to
// help with the settings if needed.
// Note: does not call set_statistics() on the factory.
Statistics* SystemRewriteDriverFactory::SetUpGlobalSharedMemStatistics(
    const SystemRewriteOptions& options) {
  if (shared_mem_statistics_.get() == NULL) {
    shared_mem_statistics_.reset(AllocateAndInitSharedMemStatistics(
        false /* not local */, "global", options));
  }
  DCHECK(!statistics_frozen_);
  statistics_frozen_ = true;
  SetStatistics(shared_mem_statistics_.get());
  return shared_mem_statistics_.get();
}

SharedMemStatistics* SystemRewriteDriverFactory::
    AllocateAndInitSharedMemStatistics(
        bool local,
        const StringPiece& name,
        const SystemRewriteOptions& options) {
  // Note that we create the statistics object in the parent process, and
  // it stays around in the kids but gets reinitialized for them
  // inside ChildInit(), called from pagespeed_child_init.
  GoogleString log_filename;
  bool logging_enabled = false;
  if (!options.log_dir().empty()) {
    // Only enable statistics logging if a log_dir() is actually specified.
    log_filename = StrCat(options.log_dir(), "/stats_log_", name);
    logging_enabled = options.statistics_logging_enabled();
  }
  SharedMemStatistics* stats = new SharedMemStatistics(
      options.statistics_logging_interval_ms(),
      options.statistics_logging_max_file_size_kb(),
      log_filename, logging_enabled,
      // TODO(jmarantz): it appears that filename_prefix() is not actually
      // established at the time of this construction, calling into question
      // whether we are naming our shared-memory segments correctly.
      StrCat(filename_prefix(), name), shared_mem_runtime(),
      message_handler(), file_system(), timer());
  NonStaticInitStats(stats);
  bool init_ok = stats->Init(true, message_handler());
  if (local && init_ok) {
    local_shm_stats_segment_names_.push_back(stats->SegmentName());
  }
  return stats;
}

void SystemRewriteDriverFactory::InitStats(Statistics* statistics) {
  // Init standard PSOL stats.
  RewriteDriverFactory::InitStats(statistics);

  // Init System-specific stats.
  SerfUrlAsyncFetcher::InitStats(statistics);
  StdioFileSystem::InitStats(statistics);
  SystemCaches::InitStats(statistics);
  PropertyCache::InitCohortStats(RewriteDriver::kBeaconCohort, statistics);
  PropertyCache::InitCohortStats(RewriteDriver::kDomCohort, statistics);
  PropertyCache::InitCohortStats(RewriteDriver::kDependenciesCohort,
                                 statistics);
  InPlaceResourceRecorder::InitStats(statistics);
  RateController::InitStats(statistics);
  CentralControllerRpcClient::InitStats(statistics);

  statistics->AddVariable(kShutdownCount);
}

NonceGenerator* SystemRewriteDriverFactory::DefaultNonceGenerator() {
  MessageHandler* handler = message_handler();
  FileSystem::InputFile* random_file =
      file_system()->OpenInputFile("/dev/urandom", handler);
  CHECK(random_file != NULL) << "Couldn't open /dev/urandom";
  // Now use the key to construct an InputFileNonceGenerator.  Passing in a NULL
  // random_file here will create a generator that will fail on first access.
  return new InputFileNonceGenerator(random_file, file_system(),
                                     thread_system()->NewMutex(), handler);
}

void SystemRewriteDriverFactory::SetupCaches(ServerContext* server_context) {
  caches_->SetupCaches(server_context, enable_property_cache());
}

void SystemRewriteDriverFactory::InitStaticAssetManager(
    StaticAssetManager* static_asset_manager) {
  static_asset_manager->set_library_url_prefix(static_asset_prefix_);
}

QueuedWorkerPool* SystemRewriteDriverFactory::CreateWorkerPool(
    WorkerPoolCategory pool, StringPiece name) {
  switch (pool) {
    case kHtmlWorkers:
      // In Apache this will effectively be 0, as it doesn't use HTML threads.
      return new QueuedWorkerPool(1, name, thread_system());
    case kRewriteWorkers:
      return new QueuedWorkerPool(num_rewrite_threads_, name, thread_system());
    case kLowPriorityRewriteWorkers:
      return new QueuedWorkerPool(num_expensive_rewrite_threads_,
                                  name,
                                  thread_system());
    default:
      return RewriteDriverFactory::CreateWorkerPool(pool, name);
  }
}

void SystemRewriteDriverFactory::ParentOrChildInit() {
  SharedCircularBufferInit(is_root_process_);
}

void SystemRewriteDriverFactory::NameProcess(const char* name) {
  // Set the process status.  This is what /proc/PID/status shows and what
  // "ps -a" gives you.  With PR_SET_NAME there's a max of 16 characters, so
  // abbreviate pagespeed as ps to be terse.
  char name_for_prctl[16];
  snprintf(name_for_prctl, sizeof(name_for_prctl), "ps-%s", name);
  prctl(PR_SET_NAME, name_for_prctl);

  // It's also possible to change argv[0], but this is a pain so currently we
  // only do this in nginx where they've written ngx_setproctitle to make it
  // easy.
}

void SystemRewriteDriverFactory::PrepareForkedProcess(const char* name) {
  is_root_process_ = false;
  NameProcess(name);
}

void SystemRewriteDriverFactory::PrepareControllerProcess() {
  system_thread_system_->PermitThreadStarting();
  ParentOrChildInit();
  SetupMessageHandlers();
}

void SystemRewriteDriverFactory::StartController(
    const SystemRewriteOptions& options) {
  if (!options.controller_port().empty()) {
    std::unique_ptr<CentralControllerRpcServer> controller(
        new CentralControllerRpcServer(
            options.controller_port(), new QueuedExpensiveOperationController(
                                           options.image_max_rewrites_at_once(),
                                           thread_system(), statistics()),
            new PopularityContestScheduleRewriteController(
                thread_system(), statistics(), timer(),
                options.popularity_contest_max_inflight_requests(),
                options.popularity_contest_max_queue_size()),
            message_handler()));
    // In the forked process, this call starts a new event loop and never
    // returns.
    ControllerManager::ForkControllerProcess(
        std::move(controller), this, system_thread_system_, message_handler());
  }
}

void SystemRewriteDriverFactory::RootInit() {
  ParentOrChildInit();

  // Let SystemCaches know about the various paths we have in configuration
  // first, as well as external cache instances.
  for (SystemServerContextSet::iterator
           p = uninitialized_server_contexts_.begin(),
           e = uninitialized_server_contexts_.end(); p != e; ++p) {
    SystemServerContext* server_context = *p;
    caches_->RegisterConfig(server_context->global_system_rewrite_options());
  }

  caches_->RootInit();

  // These options are for StartController, so we only need process scope conf.
  SystemRewriteOptions* process_options =
      SystemRewriteOptions::DynamicCast(default_options());
  if (process_options != nullptr) {
    StartController(*process_options);
  }
}

void SystemRewriteDriverFactory::ChildInit() {
  const SystemRewriteOptions* conf =
      SystemRewriteOptions::DynamicCast(default_options());
  CHECK(conf != NULL);

  StdioFileSystem* fs = dynamic_cast<StdioFileSystem*>(file_system());
  DCHECK(fs != NULL) << "Expected StdioFileSystem so we can call TrackTiming";
  if (fs != NULL) {
    fs->TrackTiming(conf->slow_file_latency_threshold_us(),
                    timer(), statistics(),
                    message_handler());
  }

  is_root_process_ = false;
  system_thread_system_->PermitThreadStarting();

  ParentOrChildInit();

  SetupMessageHandlers();

  if (shared_mem_statistics_.get() != NULL) {
    shared_mem_statistics_->Init(false, message_handler());
  }

  caches_->ChildInit();

  // Static asset config is process-global.
  if (conf->has_static_assets_to_cdn()) {
    StaticAssetConfig out_conf;
    conf->FillInStaticAssetCDNConf(&out_conf);
    static_asset_manager()->ServeAssetsFromGStatic(
        conf->static_assets_cdn_base());
    static_asset_manager()->ApplyGStaticConfiguration(
        out_conf,
        StaticAssetManager::kInitialConfiguration);
  }

  for (SystemServerContextSet::iterator
           p = uninitialized_server_contexts_.begin(),
           e = uninitialized_server_contexts_.end(); p != e; ++p) {
    SystemServerContext* server_context = *p;
    server_context->ChildInit(this);
  }
  uninitialized_server_contexts_.clear();
}

std::shared_ptr<CentralController>
SystemRewriteDriverFactory::GetCentralController(
    NamedLockManager* lock_manager) {
  const SystemRewriteOptions* conf =
      SystemRewriteOptions::DynamicCast(default_options());
  if (conf->controller_port().empty()) {
    return RewriteDriverFactory::GetCentralController(lock_manager);
  }

  if (central_controller_ == nullptr) {
    central_controller_ = std::make_shared<CentralControllerRpcClient>(
        conf->controller_port(),
        conf->popularity_contest_max_queue_size() +
            conf->popularity_contest_max_inflight_requests(),
        thread_system(), timer(), statistics(), message_handler());
  }
  return central_controller_;
}

// TODO(jmarantz): make this per-vhost.
void SystemRewriteDriverFactory::SharedCircularBufferInit(bool is_root) {
  // Set buffer size to 0 means turning it off
  if (shared_mem_runtime() != NULL && (message_buffer_size_ != 0)) {
    // TODO(jmarantz): it appears that filename_prefix() is not actually
    // established at the time of this construction, calling into question
    // whether we are naming our shared-memory segments correctly.
    shared_circular_buffer_.reset(new SharedCircularBuffer(
        shared_mem_runtime(),
        message_buffer_size_,
        filename_prefix().as_string(),
        hostname_identifier()));
    if (shared_circular_buffer_->InitSegment(is_root, message_handler())) {
      SetCircularBuffer(shared_circular_buffer_.get());
     }
  }
}

RewriteOptions::OptionSettingResult
SystemRewriteDriverFactory::ParseAndSetOption1(StringPiece option,
                                               StringPiece arg,
                                               bool process_scope,
                                               GoogleString* msg,
                                               MessageHandler* handler) {
  // First check the scope.
  if (StringCaseEqual(option, kStaticAssetPrefix) ||
      StringCaseEqual(option, kUsePerVHostStatistics) ||
      StringCaseEqual(option, kInstallCrashHandler) ||
      StringCaseEqual(option, kNumRewriteThreads) ||
      StringCaseEqual(option, kNumExpensiveRewriteThreads)) {
    if (!process_scope) {
      *msg = StrCat("'", option, "' is global and can't be set at this scope.");
      return RewriteOptions::kOptionValueInvalid;
    }
  } else if (StringCaseEqual(option, kForceCaching) ||
             StringCaseEqual(option, kListOutstandingUrlsOnError) ||
             StringCaseEqual(option, kMessageBufferSize) ||
             StringCaseEqual(option, kTrackOriginalContentLength)) {
    if (!process_scope) {
      // msg is only printed to the user on error, so warnings must be logged.
      handler->Message(
          kWarning, "'%s' is global and is ignored at this scope",
          option.as_string().c_str());
      // OK here means "move on" not "accepted and applied".
      return RewriteOptions::kOptionOk;
    }
  } else {
    return RewriteOptions::kOptionNameUnknown;
  }

  // Scope is ok and option is known.  Parse and apply.

  if (StringCaseEqual(option, kStaticAssetPrefix)) {
    set_static_asset_prefix(arg);
    return RewriteOptions::kOptionOk;
  }

  // Most of our options take booleans, so just parse once.
  bool is_on = false;
  RewriteOptions::OptionSettingResult parsed_as_bool =
      RewriteOptions::ParseFromString(arg, &is_on) ?
      RewriteOptions::kOptionOk : RewriteOptions::kOptionValueInvalid;
  if (StringCaseEqual(option, kUsePerVHostStatistics)) {
    set_use_per_vhost_statistics(is_on);
    return parsed_as_bool;
  } else if (StringCaseEqual(option, kForceCaching)) {
    set_force_caching(is_on);
    return parsed_as_bool;
  } else if (StringCaseEqual(option, kInstallCrashHandler)) {
    set_install_crash_handler(is_on);
    return parsed_as_bool;
  } else if (StringCaseEqual(option, kListOutstandingUrlsOnError)) {
    list_outstanding_urls_on_error(is_on);
    return parsed_as_bool;
  } else if (StringCaseEqual(option, kTrackOriginalContentLength)) {
    set_track_original_content_length(is_on);
    return parsed_as_bool;
  }

  // Others take an integer >= 0.
  //
  // Values of 0 have special meanings:
  //   Num(Expensive)RewriteThreads: autodetect (see AutoDetectThreadCounts())
  //   MessageBufferSize: disable the message buffer
  int int_value = 0;
  RewriteOptions::OptionSettingResult parsed_as_int =
      RewriteOptions::ParseFromString(arg, &int_value) ?
      RewriteOptions::kOptionOk : RewriteOptions::kOptionValueInvalid;
  if (StringCaseEqual(option, kNumRewriteThreads)) {
    set_num_rewrite_threads(int_value);
    return parsed_as_int;
  } else if (StringCaseEqual(option, kNumExpensiveRewriteThreads)) {
    set_num_expensive_rewrite_threads(int_value);
    return parsed_as_int;
  } else if (StringCaseEqual(option, kMessageBufferSize)) {
    set_message_buffer_size(int_value);
    return parsed_as_int;
  }

  LOG(FATAL) << "Unknown options should have been handled in scope checking.";
  return RewriteOptions::kOptionNameUnknown;
}

RewriteOptions::OptionSettingResult
SystemRewriteDriverFactory::ParseAndSetOption2(StringPiece option,
                                               StringPiece arg1,
                                               StringPiece arg2,
                                               bool process_scope,
                                               GoogleString* msg,
                                               MessageHandler* handler) {
  if (StringCaseEqual(option, kCreateSharedMemoryMetadataCache)) {
    if (!process_scope) {
      // msg is only printed to the user on error, so warnings must be logged.
      handler->Message(
          kWarning, "'%s' is global and is ignored at this scope",
          option.as_string().c_str());
      // OK here means "move on" not "accepted and applied".
      return RewriteOptions::kOptionOk;
    }

    int64 kb = 0;
    if (!StringToInt64(arg2, &kb) || kb < 0) {
      *msg = "size_kb must be a positive 64-bit integer";
      return RewriteOptions::kOptionValueInvalid;
    }
    bool ok = caches()->CreateShmMetadataCache(arg1, kb, msg);
    return ok ? RewriteOptions::kOptionOk : RewriteOptions::kOptionValueInvalid;
  }
  return RewriteOptions::kOptionNameUnknown;
}

void SystemRewriteDriverFactory::PostConfig(
    const std::vector<SystemServerContext*>& server_contexts,
    GoogleString* error_message,
    int* error_index,
    Statistics** global_statistics) {
  for (int i = 0, n = server_contexts.size(); i < n; ++i) {
    server_contexts[i]->CollapseConfigOverlaysAndComputeSignatures();
    SystemRewriteOptions* options =
        server_contexts[i]->global_system_rewrite_options();
    if (options->unplugged()) {
      continue;
    }

    if (options->enabled()) {
      GoogleString file_cache_path = options->file_cache_path();
      if (file_cache_path.empty()) {
        *error_index = i;
        *error_message = "FileCachePath must not be empty";
        return;
      }
    }

    if (options->statistics_enabled()) {
      // Lazily create shared-memory statistics if enabled in any config, even
      // when PageSpeed is totally disabled.  This allows statistics to work if
      // PageSpeed gets turned on via .htaccess or query param.
      if (*global_statistics == NULL) {
        *global_statistics = SetUpGlobalSharedMemStatistics(*options);
      }

      // If we have per-vhost statistics on as well, then set it up.
      if (use_per_vhost_statistics()) {
        server_contexts[i]->CreateLocalStatistics(*global_statistics, this);
      }
    }
  }
}

void SystemRewriteDriverFactory::StopCacheActivity() {
  RewriteDriverFactory::StopCacheActivity();
  caches_->StopCacheActivity();
}

void SystemRewriteDriverFactory::ShutDown() {
  if (!is_root_process_) {
    Variable* child_shutdown_count = statistics()->GetVariable(kShutdownCount);
    child_shutdown_count->Add(1);
    message_handler()->MessageS(kInfo, "Shutting down PageSpeed child");
  }

  StopCacheActivity();

  // Next, we shutdown the fetchers before killing the workers in
  // RewriteDriverFactory::ShutDown; this is so any rewrite jobs in progress
  // can quickly wrap up.
  for (FetcherMap::iterator p = fetcher_map_.begin(), e = fetcher_map_.end();
       p != e; ++p) {
    UrlAsyncFetcher* fetcher = p->second;
    fetcher->ShutDown();
    defer_cleanup(new Deleter<UrlAsyncFetcher>(fetcher));
  }
  fetcher_map_.clear();
  ShutDownFetchers();

  RewriteDriverFactory::ShutDown();

  caches_->ShutDown(message_handler());

  ShutDownMessageHandlers();

  // Must be freed before the thread_system, but we still want it around for
  // RewriteDriverFactory::ShutDown.
  central_controller_.reset();

  if (is_root_process_) {
    // Cleanup statistics.
    // TODO(morlovich): This looks dangerous with async.
    if (shared_mem_statistics_.get() != NULL) {
      shared_mem_statistics_->GlobalCleanup(message_handler());
    }

    // Likewise for local ones. We no longer have the objects here
    // (since SplitStats destroyed them), but we saved the segment names.
    for (int i = 0, n = local_shm_stats_segment_names_.size(); i < n; ++i) {
      SharedMemStatistics::GlobalCleanup(shared_mem_runtime_.get(),
                                         local_shm_stats_segment_names_[i],
                                         message_handler());
    }

    // Cleanup SharedCircularBuffer.
    // Use GoogleMessageHandler instead of SystemMessageHandler.
    // As we are cleaning SharedCircularBuffer, we do not want to write to its
    // buffer and passing SystemMessageHandler here may cause infinite loop.
    GoogleMessageHandler handler;
    if (shared_circular_buffer_ != NULL) {
      shared_circular_buffer_->GlobalCleanup(&handler);
    }
  }
}

GoogleString SystemRewriteDriverFactory::GetFetcherKey(
    bool include_slurping_config, const SystemRewriteOptions* config) {
  // Include all the fetcher parameters in the fetcher key, one per line.
  GoogleString key;
  if (config->unplugged()) {
    key = "unplugged";
  } else {
    key = StrCat(
        list_outstanding_urls_on_error_ ? "list_errors\n" : "no_errors\n",
        config->fetcher_proxy(), "\n",
        config->fetch_with_gzip() ? "fetch_with_gzip\n": "no_gzip\n",
        track_original_content_length_ ? "track_content_length\n" : "no_track\n"
        "timeout: ", Integer64ToString(config->blocking_fetch_timeout_ms()),
        "\n");
    if (config->slurping_enabled() && include_slurping_config) {
      if (config->slurp_read_only()) {
        StrAppend(&key, "R", config->slurp_directory(), "\n");
      } else {
        StrAppend(&key, "W", config->slurp_directory(), "\n");
      }
    }
    StrAppend(&key,
              "\nhttps: ", config->https_options(),
              "\ncert_dir: ", config->ssl_cert_directory(),
              "\ncert_file: ", config->ssl_cert_file());
  }

  return key;
}

UrlAsyncFetcher* SystemRewriteDriverFactory::GetFetcher(
    SystemRewriteOptions* config) {
  // Include all the fetcher parameters in the fetcher key, one per line.
  GoogleString key = GetFetcherKey(true, config);
  std::pair<FetcherMap::iterator, bool> result = fetcher_map_.insert(
      std::make_pair(key, static_cast<UrlAsyncFetcher*>(NULL)));
  FetcherMap::iterator iter = result.first;
  if (result.second) {
    UrlAsyncFetcher* fetcher = NULL;
    if (config->slurping_enabled()) {
      if (config->slurp_read_only()) {
        HttpDumpUrlFetcher* dump_fetcher = new HttpDumpUrlFetcher(
            config->slurp_directory(), file_system(), timer());
        fetcher = dump_fetcher;
      } else {
        UrlAsyncFetcher* base_fetcher = GetBaseFetcher(config);
        HttpDumpUrlAsyncWriter* dump_writer = new HttpDumpUrlAsyncWriter(
            config->slurp_directory(), base_fetcher, file_system(), timer());
        fetcher = dump_writer;
      }
    } else {
      fetcher = GetBaseFetcher(config);
      if (config->rate_limit_background_fetches()) {
        // Unfortunately, we need stats for load-shedding.
        if (config->statistics_enabled()) {
          TakeOwnership(fetcher);
          fetcher = new RateControllingUrlAsyncFetcher(
              fetcher, max_queue_size(), requests_per_host(), queued_per_host(),
              thread_system(), statistics());
        } else {
          message_handler()->Message(
              kError, "Can't enable fetch rate-limiting without statistics");
        }
      }
    }
    iter->second = fetcher;
  }
  return iter->second;
}

UrlAsyncFetcher* SystemRewriteDriverFactory::AllocateFetcher(
    SystemRewriteOptions* config) {
  SerfUrlAsyncFetcher* serf = new SerfUrlAsyncFetcher(
      config->fetcher_proxy().c_str(),
      NULL,  // Do not use the Factory pool so we can control deletion.
      thread_system(), statistics(), timer(),
      config->blocking_fetch_timeout_ms(),
      message_handler());
  serf->set_list_outstanding_urls_on_error(list_outstanding_urls_on_error_);
  serf->set_fetch_with_gzip(config->fetch_with_gzip());
  serf->set_track_original_content_length(track_original_content_length_);
  serf->SetHttpsOptions(config->https_options());
  serf->SetSslCertificatesDir(config->ssl_cert_directory());
  serf->SetSslCertificatesFile(config->ssl_cert_file());
  return serf;
}


UrlAsyncFetcher* SystemRewriteDriverFactory::GetBaseFetcher(
    SystemRewriteOptions* config) {
  GoogleString cache_key = GetFetcherKey(false, config);
  std::pair<FetcherMap::iterator, bool> result = base_fetcher_map_.insert(
      std::make_pair(cache_key, static_cast<UrlAsyncFetcher*>(NULL)));
  FetcherMap::iterator iter = result.first;
  if (result.second) {
    iter->second = AllocateFetcher(config);
  }
  return iter->second;
}

UrlAsyncFetcher* SystemRewriteDriverFactory::DefaultAsyncUrlFetcher() {
  LOG(DFATAL) << "The fetchers are not global, but kept in a map.";
  return NULL;
}

FileSystem* SystemRewriteDriverFactory::DefaultFileSystem() {
  return new StdioFileSystem();
}

Hasher* SystemRewriteDriverFactory::NewHasher() {
  return new MD5Hasher();
}

Timer* SystemRewriteDriverFactory::DefaultTimer() {
  return new PosixTimer();
}

NamedLockManager* SystemRewriteDriverFactory::DefaultLockManager() {
  LOG(DFATAL) << "Locks are owned by SystemCachePath, not the factory";
  return NULL;
}

ServerContext* SystemRewriteDriverFactory::NewServerContext() {
  LOG(DFATAL) << "Use implementation-specific MakeXServerXContext() instead";
  return NULL;
}

int SystemRewriteDriverFactory::requests_per_host() {
  CHECK(thread_counts_finalized_);
  return std::min(4, num_rewrite_threads_);
}

void SystemRewriteDriverFactory::AutoDetectThreadCounts() {
  if (thread_counts_finalized_) {
    return;
  }

  if (IsServerThreaded()) {
    if (num_rewrite_threads_ <= 0) {
      num_rewrite_threads_ = 4;
    }
    if (num_expensive_rewrite_threads_ <= 0) {
      num_expensive_rewrite_threads_ = 4;
    }
    message_handler()->Message(
        kInfo, "Detected threaded server."
        " Own threads: %d Rewrite, %d Expensive Rewrite.",
        num_rewrite_threads_, num_expensive_rewrite_threads_);

  } else {
    if (num_rewrite_threads_ <= 0) {
      num_rewrite_threads_ = 1;
    }
    if (num_expensive_rewrite_threads_ <= 0) {
      num_expensive_rewrite_threads_ = 1;
    }
    message_handler()->Message(
        kInfo, "No threading detected."
        " Own threads: %d Rewrite, %d Expensive Rewrite.",
        num_rewrite_threads_, num_expensive_rewrite_threads_);
  }

  thread_counts_finalized_ = true;
}

}  // namespace net_instaweb
