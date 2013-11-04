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

#include "net/instaweb/system/public/system_rewrite_driver_factory.h"

#include <map>
#include <set>
#include <utility>  // for pair

#include "base/logging.h"
#include "net/instaweb/http/public/http_dump_url_async_writer.h"
#include "net/instaweb/http/public/http_dump_url_fetcher.h"
#include "net/instaweb/http/public/rate_controller.h"
#include "net/instaweb/http/public/rate_controlling_url_async_fetcher.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/system/public/in_place_resource_recorder.h"
#include "net/instaweb/system/public/serf_url_async_fetcher.h"
#include "net/instaweb/system/public/system_caches.h"
#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/system/public/system_server_context.h"
#include "net/instaweb/system/public/system_thread_system.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/posix_timer.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/pthread_shared_mem.h"
#include "net/instaweb/util/public/shared_circular_buffer.h"
#include "net/instaweb/util/public/shared_mem_statistics.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/null_shared_mem.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/input_file_nonce_generator.h"

namespace net_instaweb {

class NonceGenerator;

namespace {

const char kShutdownCount[] = "child_shutdown_count";

}  // namespace

SystemRewriteDriverFactory::SystemRewriteDriverFactory(
    SystemThreadSystem* thread_system,
    AbstractSharedMem* shared_mem_runtime, /* may be null */
    StringPiece hostname, int port)
    : RewriteDriverFactory(thread_system),
      statistics_frozen_(false),
      is_root_process_(true),
      hostname_identifier_(StrCat(hostname, ":", IntegerToString(port))),
      message_buffer_size_(0),
      track_original_content_length_(false),
      list_outstanding_urls_on_error_(false),
      system_thread_system_(thread_system) {
  if (shared_mem_runtime == NULL) {
#ifdef PAGESPEED_SUPPORT_POSIX_SHARED_MEM
    shared_mem_runtime = new PthreadSharedMem();
#else
    shared_mem_runtime = new NullSharedMem();
#endif
  }
  shared_mem_runtime_.reset(shared_mem_runtime);
  // Some implementations, such as Apache, call caches.set_thread_limit in
  // their constructors and override this limit.
  int thread_limit = 1;
  caches_.reset(
      new SystemCaches(this, shared_mem_runtime_.get(), thread_limit));
}

SystemRewriteDriverFactory::~SystemRewriteDriverFactory() {
  shared_mem_statistics_.reset(NULL);
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
  SystemCaches::InitStats(statistics);
  PropertyCache::InitCohortStats(RewriteDriver::kBeaconCohort, statistics);
  PropertyCache::InitCohortStats(RewriteDriver::kDomCohort, statistics);
  InPlaceResourceRecorder::InitStats(statistics);
  RateController::InitStats(statistics);

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
  caches_->SetupCaches(server_context);
  server_context->set_enable_property_cache(enable_property_cache());
  PropertyCache* pcache = server_context->page_property_cache();

  const PropertyCache::Cohort* cohort =
      server_context->AddCohort(RewriteDriver::kBeaconCohort, pcache);
  server_context->set_beacon_cohort(cohort);

  cohort = server_context->AddCohort(RewriteDriver::kDomCohort, pcache);
  server_context->set_dom_cohort(cohort);
}

void SystemRewriteDriverFactory::ParentOrChildInit() {
  SharedCircularBufferInit(is_root_process_);
}

void SystemRewriteDriverFactory::RootInit() {
  ParentOrChildInit();

  // Let SystemCaches know about the various paths we have in configuration
  // first, as well as the memcached instances.
  for (SystemServerContextSet::iterator
           p = uninitialized_server_contexts_.begin(),
           e = uninitialized_server_contexts_.end(); p != e; ++p) {
    SystemServerContext* server_context = *p;
    caches_->RegisterConfig(server_context->system_rewrite_options());
  }

  caches_->RootInit();
}

void SystemRewriteDriverFactory::ChildInit() {
  is_root_process_ = false;
  system_thread_system_->PermitThreadStarting();

  ParentOrChildInit();

  SetupMessageHandlers();

  if (shared_mem_statistics_.get() != NULL) {
    shared_mem_statistics_->Init(false, message_handler());
  }

  caches_->ChildInit();

  for (SystemServerContextSet::iterator
           p = uninitialized_server_contexts_.begin(),
           e = uninitialized_server_contexts_.end(); p != e; ++p) {
    SystemServerContext* server_context = *p;
    server_context->ChildInit(this);
  }
  uninitialized_server_contexts_.clear();
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

void SystemRewriteDriverFactory::PostConfig(
    const std::vector<SystemServerContext*>& server_contexts,
    GoogleString* error_message,
    int* error_index,
    Statistics** global_statistics) {
  for (int i = 0, n = server_contexts.size(); i < n; ++i) {
    server_contexts[i]->CollapseConfigOverlaysAndComputeSignatures();
    SystemRewriteOptions* options =
        server_contexts[i]->system_rewrite_options();
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
    message_handler()->Message(kInfo, "Shutting down PageSpeed child");
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
              "\nhttps: ", https_options_,
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
  serf->SetHttpsOptions(https_options_);
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

bool SystemRewriteDriverFactory::SetHttpsOptions(StringPiece directive,
                                                 GoogleString* error_message) {
  directive.CopyToString(&https_options_);
  return SerfUrlAsyncFetcher::ValidateHttpsOptions(directive, error_message);
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

}  // namespace net_instaweb
