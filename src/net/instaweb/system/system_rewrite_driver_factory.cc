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

#include "base/logging.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/system/public/serf_url_async_fetcher.h"
#include "net/instaweb/system/public/system_caches.h"
#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/system/public/system_server_context.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/pthread_shared_mem.h"
#include "net/instaweb/util/public/shared_circular_buffer.h"
#include "net/instaweb/util/public/shared_mem_statistics.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/null_shared_mem.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/input_file_nonce_generator.h"

namespace net_instaweb {

namespace {

const char kShutdownCount[] = "child_shutdown_count";

}  // namespace

SystemRewriteDriverFactory::SystemRewriteDriverFactory(
    ThreadSystem* thread_system,
    StringPiece hostname,
    int port)
    : RewriteDriverFactory(thread_system),
#ifdef PAGESPEED_SUPPORT_POSIX_SHARED_MEM
      shared_mem_runtime_(new PthreadSharedMem()),
#else
      shared_mem_runtime_(new NullSharedMem()),
#endif
      statistics_frozen_(false),
      is_root_process_(true),
      hostname_identifier_(StrCat(hostname, ":", IntegerToString(port))),
      message_buffer_size_(0) {
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
Statistics* SystemRewriteDriverFactory::MakeGlobalSharedMemStatistics(
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
    server_context->ChildInit();
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
  //
  // This currently is a no-op except in Apache.
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

}  // namespace net_instaweb
