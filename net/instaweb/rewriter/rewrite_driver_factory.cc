/*
 * Copyright 2010 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"

#include "base/logging.h"
#include "net/instaweb/config/rewrite_options_manager.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_dump_url_async_writer.h"
#include "net/instaweb/http/public/http_dump_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/public/beacon_critical_images_finder.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/critical_selector_finder.h"
#include "net/instaweb/rewriter/public/experiment_matcher.h"
#include "net/instaweb/rewriter/public/process_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/rewriter/public/usage_data_reporter.h"
#include "net/instaweb/util/public/property_store.h"
#include "pagespeed/controller/central_controller.h"
#include "pagespeed/controller/compatible_central_controller.h"
#include "pagespeed/controller/in_process_central_controller.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/checking_thread_system.h"
#include "pagespeed/kernel/base/dynamic_annotations.h"  // RunningOnValgrind
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/hostname_util.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/sha1_signature.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/cache_batcher.h"
#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/kernel/http/user_agent_normalizer.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"
#include "pagespeed/kernel/thread/scheduler.h"
#include "pagespeed/kernel/util/file_system_lock_manager.h"
#include "pagespeed/kernel/util/nonce_generator.h"

namespace net_instaweb {

RewriteDriverFactory::RewriteDriverFactory(
    const ProcessContext& process_context, ThreadSystem* thread_system)
    : url_async_fetcher_(NULL),
      js_tokenizer_patterns_(process_context.js_tokenizer_patterns()),
      force_caching_(false),
      slurp_read_only_(false),
      slurp_print_urls_(false),
#ifdef NDEBUG
      // For release binaries, use the thread-system directly.
      thread_system_(thread_system),
#else
      // When compiling for debug, interpose a layer that CHECKs for clean mutex
      // semantics.
      thread_system_(new CheckingThreadSystem(thread_system)),
#endif
      server_context_mutex_(thread_system_->NewMutex()),
      statistics_(&null_statistics_),
      worker_pools_(kNumWorkerPools, NULL),
      hostname_(GetHostname()) {
  // Pre-initializes the default options.  IMPORTANT: subclasses overridding
  // NewRewriteOptions() should re-call this method from their constructor
  // so that the correct rewrite_options_ object gets reset.
  InitializeDefaultOptions();
}

void RewriteDriverFactory::InitializeDefaultOptions() {
  default_options_.reset(NewRewriteOptions());
  InitializeDefaultOptions(default_options_.get());
  // Note that we do not need to compute a signature on the default options.
  // We will never be serving requests with these options: they are just used
  // as a source for merging.
}

void RewriteDriverFactory::InitializeDefaultOptions(RewriteOptions* options) {
  // We default to using the "core filters". Note that this is not
  // the only place the default is applied --- for directories with .htaccess
  // files it is given in create_dir_config in mod_instaweb.cc
  options->SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  options->DisallowTroublesomeResources();
}

void RewriteDriverFactory::reset_default_options(RewriteOptions* new_defaults) {
    default_options_.reset(new_defaults);
}

RewriteDriverFactory::~RewriteDriverFactory() {
  ShutDown();

  {
    ScopedMutex lock(server_context_mutex_.get());
    STLDeleteElements(&server_contexts_);
  }

  for (int c = 0; c < kNumWorkerPools; ++c) {
    delete worker_pools_[c];
    worker_pools_[c] = NULL;
  }

  // Avoid double-destructing the url fetchers if they were not overridden
  // programmatically
  if ((url_async_fetcher_ != NULL) &&
      (url_async_fetcher_ != base_url_async_fetcher_.get())) {
    delete url_async_fetcher_;
  }
  url_async_fetcher_ = NULL;

  for (int i = 0, n = deferred_cleanups_.size(); i < n; ++i) {
    deferred_cleanups_[i]->CallRun();
  }

  // Delete the lock-manager before we delete the scheduler.
  lock_manager_.reset(NULL);
}

void RewriteDriverFactory::set_html_parse_message_handler(
    MessageHandler* message_handler) {
  html_parse_message_handler_.reset(message_handler);
}

void RewriteDriverFactory::set_message_handler(
    MessageHandler* message_handler) {
  message_handler_.reset(message_handler);
}

bool RewriteDriverFactory::FetchersComputed() const {
  return (url_async_fetcher_ != NULL);
}

void RewriteDriverFactory::set_slurp_directory(const StringPiece& dir) {
  CHECK(!FetchersComputed())
      << "Cannot call set_slurp_directory "
      << " after ComputeUrl*Fetcher has been called";
  dir.CopyToString(&slurp_directory_);
}

void RewriteDriverFactory::set_slurp_read_only(bool read_only) {
  CHECK(!FetchersComputed())
      << "Cannot call set_slurp_read_only "
      << " after ComputeUrl*Fetcher has been called";
  slurp_read_only_ = read_only;
}

void RewriteDriverFactory::set_slurp_print_urls(bool print_urls) {
  CHECK(!FetchersComputed())
      << "Cannot call set_slurp_print_urls "
      << " after ComputeUrl*Fetcher has been called";
  slurp_print_urls_ = print_urls;
}

void RewriteDriverFactory::set_file_system(FileSystem* file_system) {
  file_system_.reset(file_system);
}

void RewriteDriverFactory::set_base_url_async_fetcher(
    UrlAsyncFetcher* url_async_fetcher) {
  CHECK(!FetchersComputed())
      << "Cannot call set_base_url_async_fetcher "
      << " after ComputeUrlAsyncFetcher has been called";
  base_url_async_fetcher_.reset(url_async_fetcher);
}

void RewriteDriverFactory::set_hasher(Hasher* hasher) {
  hasher_.reset(hasher);
}

void RewriteDriverFactory::set_signature(SHA1Signature* signature) {
  signature_.reset(signature);
}

void RewriteDriverFactory::set_timer(Timer* timer) {
  timer_.reset(timer);
}

void RewriteDriverFactory::set_nonce_generator(NonceGenerator* gen) {
  nonce_generator_.reset(gen);
}

void RewriteDriverFactory::set_url_namer(UrlNamer* url_namer) {
  url_namer_.reset(url_namer);
}

void RewriteDriverFactory::set_usage_data_reporter(
    UsageDataReporter* reporter) {
  usage_data_reporter_.reset(reporter);
}

MessageHandler* RewriteDriverFactory::html_parse_message_handler() {
  if (html_parse_message_handler_ == NULL) {
    html_parse_message_handler_.reset(DefaultHtmlParseMessageHandler());
  }
  return html_parse_message_handler_.get();
}

MessageHandler* RewriteDriverFactory::message_handler() {
  if (message_handler_ == NULL) {
    message_handler_.reset(DefaultMessageHandler());
  }
  return message_handler_.get();
}

FileSystem* RewriteDriverFactory::file_system() {
  if (file_system_ == NULL) {
    file_system_.reset(DefaultFileSystem());
  }
  return file_system_.get();
}

NonceGenerator* RewriteDriverFactory::nonce_generator() {
  if (nonce_generator_ == NULL) {
    nonce_generator_.reset(DefaultNonceGenerator());
  }
  return nonce_generator_.get();
}

NonceGenerator* RewriteDriverFactory::DefaultNonceGenerator() {
  // By default return NULL (no nonce generator).
  return NULL;
}

Timer* RewriteDriverFactory::DefaultTimer() {
  return thread_system()->NewTimer();
}

Timer* RewriteDriverFactory::timer() {
  if (timer_ == NULL) {
    timer_.reset(DefaultTimer());
  }
  return timer_.get();
}

UrlNamer* RewriteDriverFactory::url_namer() {
  if (url_namer_ == NULL) {
    url_namer_.reset(DefaultUrlNamer());
  }
  return url_namer_.get();
}

UserAgentMatcher* RewriteDriverFactory::user_agent_matcher() {
  if (user_agent_matcher_ == NULL) {
    user_agent_matcher_.reset(DefaultUserAgentMatcher());
  }
  return user_agent_matcher_.get();
}

StaticAssetManager* RewriteDriverFactory::static_asset_manager() {
  if (static_asset_manager_ == NULL) {
    static_asset_manager_.reset(DefaultStaticAssetManager());
    InitStaticAssetManager(static_asset_manager_.get());
  }
  return static_asset_manager_.get();
}

RewriteOptionsManager* RewriteDriverFactory::NewRewriteOptionsManager() {
  return new RewriteOptionsManager;
}

Scheduler* RewriteDriverFactory::scheduler() {
  if (scheduler_ == NULL) {
    scheduler_.reset(CreateScheduler());
  }
  return scheduler_.get();
}

Hasher* RewriteDriverFactory::hasher() {
  if (hasher_ == NULL) {
    hasher_.reset(NewHasher());
  }
  return hasher_.get();
}

SHA1Signature* RewriteDriverFactory::signature() {
  if (signature_ == NULL) {
    signature_.reset(DefaultSignature());
  }
  return signature_.get();
}

UsageDataReporter* RewriteDriverFactory::usage_data_reporter() {
  if (usage_data_reporter_ == NULL) {
    usage_data_reporter_.reset(DefaultUsageDataReporter());
  }
  return usage_data_reporter_.get();
}

const std::vector<const UserAgentNormalizer*>&
    RewriteDriverFactory::user_agent_normalizers() {
  if (user_agent_normalizers_.empty()) {
    // Note: it's possible that we may want separate lists of normalizers for
    // different applications in the future. For now, though, we centralize
    // one list, because:
    // a) It's simpler b) Regexp compilation isn't free.
    AndroidUserAgentNormalizer* an = new AndroidUserAgentNormalizer();
    IEUserAgentNormalizer* ien = new IEUserAgentNormalizer();
    TakeOwnership(an);
    TakeOwnership(ien);
    user_agent_normalizers_.push_back(an);
    user_agent_normalizers_.push_back(ien);
    AddPlatformSpecificUserAgentNormalizers(&user_agent_normalizers_);
  }
  return user_agent_normalizers_;
}

NamedLockManager* RewriteDriverFactory::DefaultLockManager() {
  return new FileSystemLockManager(file_system(), LockFilePrefix(),
                                   scheduler(), message_handler());
}

UrlNamer* RewriteDriverFactory::DefaultUrlNamer() {
  return new UrlNamer();
}

UserAgentMatcher* RewriteDriverFactory::DefaultUserAgentMatcher() {
  return new UserAgentMatcher();
}

StaticAssetManager* RewriteDriverFactory::DefaultStaticAssetManager() {
  return new StaticAssetManager(url_namer()->proxy_domain(),
                                thread_system(),
                                hasher(),
                                message_handler());
}

CriticalImagesFinder* RewriteDriverFactory::DefaultCriticalImagesFinder(
    ServerContext* server_context) {
  // TODO(pulkitg): Don't create BeaconCriticalImagesFinder if beacon cohort is
  // not added.
  return new BeaconCriticalImagesFinder(
      server_context->beacon_cohort(), nonce_generator(), statistics());
}

CriticalSelectorFinder* RewriteDriverFactory::DefaultCriticalSelectorFinder(
    ServerContext* server_context) {
  if (server_context->beacon_cohort() != NULL) {
    return new BeaconCriticalSelectorFinder(server_context->beacon_cohort(),
                                            nonce_generator(), statistics());
  }
  return NULL;
}

SHA1Signature* RewriteDriverFactory::DefaultSignature() {
  return new SHA1Signature();
}

UsageDataReporter* RewriteDriverFactory::DefaultUsageDataReporter() {
  return new UsageDataReporter;
}

QueuedWorkerPool* RewriteDriverFactory::CreateWorkerPool(
    WorkerPoolCategory pool, StringPiece name) {
  return new QueuedWorkerPool(1, name, thread_system());
}

int RewriteDriverFactory::LowPriorityLoadSheddingThreshold() const {
  return QueuedWorkerPool::kNoLoadShedding;
}

Scheduler* RewriteDriverFactory::CreateScheduler() {
  return new Scheduler(thread_system(), timer());
}

NamedLockManager* RewriteDriverFactory::lock_manager() {
  if (lock_manager_ == NULL) {
    lock_manager_.reset(DefaultLockManager());
  }
  return lock_manager_.get();
}

QueuedWorkerPool* RewriteDriverFactory::WorkerPool(WorkerPoolCategory pool) {
  if (worker_pools_[pool] == NULL) {
    StringPiece name;
    switch (pool) {
      case kHtmlWorkers:
        name = "html";
        break;
      case kRewriteWorkers:
        name = "rewrite";
        break;
      case kLowPriorityRewriteWorkers:
        name = "slow_rewrite";
        break;
      default:
        LOG(DFATAL) << "Unhandled enum value " << pool;
        name = "unknown_worker";
        break;
    }

    worker_pools_[pool] = CreateWorkerPool(pool, name);
    worker_pools_[pool]->set_queue_size_stat(
        rewrite_stats()->thread_queue_depth(pool));
    if (pool == kLowPriorityRewriteWorkers) {
      worker_pools_[pool]->SetLoadSheddingThreshold(
          LowPriorityLoadSheddingThreshold());
    }
  }

  return worker_pools_[pool];
}

bool RewriteDriverFactory::set_filename_prefix(StringPiece p) {
  p.CopyToString(&filename_prefix_);
  if (file_system()->IsDir(filename_prefix_.c_str(),
                           message_handler()).is_true()) {
    return true;
  }

  if (!file_system()->RecursivelyMakeDir(filename_prefix_, message_handler())) {
    message_handler()->FatalError(
        filename_prefix_.c_str(), 0,
        "Directory does not exist and cannot be created");
    return false;
  }

  AddCreatedDirectory(filename_prefix_);
  return true;
}

StringPiece RewriteDriverFactory::filename_prefix() {
  return filename_prefix_;
}

ServerContext* RewriteDriverFactory::CreateServerContext() {
  ServerContext* server_context = NewServerContext();
  InitServerContext(server_context);
  return server_context;
}

void RewriteDriverFactory::InitServerContext(ServerContext* server_context) {
  ScopedMutex lock(server_context_mutex_.get());

  server_context->ComputeSignature(server_context->global_options());
  server_context->set_scheduler(scheduler());
  server_context->set_timer(timer());
  if (server_context->statistics() == NULL) {
    server_context->set_statistics(statistics());
  }
  if (server_context->rewrite_stats() == NULL) {
    server_context->set_rewrite_stats(rewrite_stats());
  }
  SetupCaches(server_context);
  if (server_context->lock_manager() == NULL) {
    server_context->set_lock_manager(lock_manager());
  }
  if (!server_context->has_default_system_fetcher()) {
    server_context->set_default_system_fetcher(ComputeUrlAsyncFetcher());
  }

  server_context->set_central_controller(
      GetCentralController(server_context->lock_manager()));
  if (server_context->url_namer() == nullptr) {
    server_context->set_url_namer(url_namer());
  }
  if (server_context->rewrite_options_manager() == nullptr) {
    server_context->SetRewriteOptionsManager(NewRewriteOptionsManager());
  }
  server_context->set_user_agent_matcher(user_agent_matcher());
  server_context->set_file_system(file_system());
  server_context->set_filename_prefix(filename_prefix_);
  server_context->set_hasher(hasher());
  server_context->set_signature(signature());
  server_context->set_message_handler(message_handler());
  server_context->set_static_asset_manager(static_asset_manager());
  server_context->set_critical_images_finder(
      DefaultCriticalImagesFinder(server_context));
  server_context->set_critical_selector_finder(
      DefaultCriticalSelectorFinder(server_context));
  server_context->set_hostname(hostname_);
  server_context->PostInitHook();
  InitDecodingDriver(server_context);
  server_contexts_.insert(server_context);

  // Make sure that all lazy state gets initialized, even if we don't copy it to
  // ServerContext
  user_agent_normalizers();
  // Fetch the remote options so that they will be cached.
  HttpOptions fetch_options;
  fetch_options.implicit_cache_ttl_ms =
      server_context->global_options()->implicit_cache_ttl_ms();
  fetch_options.respect_vary = false;
  RequestContextPtr request_ctx(new RequestContext(
      fetch_options, server_context->thread_system()->NewMutex(),
      server_context->timer()));
  scoped_ptr<RewriteOptions> remote_options(
      server_context->global_options()->Clone());
  server_context->GetRemoteOptions(remote_options.get(),
                                   true /* startup fetch */);
}

std::shared_ptr<CentralController> RewriteDriverFactory::GetCentralController(
    NamedLockManager* lock_manager) {
  return std::make_shared<CompatibleCentralController>(
      default_options()->image_max_rewrites_at_once(), statistics(),
      thread_system(), lock_manager);
}

void RewriteDriverFactory::RebuildDecodingDriverForTests(
    ServerContext* server_context) {
  decoding_driver_.reset(NULL);
  InitDecodingDriver(server_context);
}

void RewriteDriverFactory::InitDecodingDriver(ServerContext* server_context) {
  if (decoding_driver_.get() == NULL) {
    decoding_server_context_.reset(NewDecodingServerContext());
    // decoding_driver_ takes ownership.
    RewriteOptions* options = default_options_->Clone();
    options->ComputeSignature();
    decoding_driver_.reset(
        decoding_server_context_->NewUnmanagedRewriteDriver(
            NULL, options, RequestContextPtr(NULL)));
    decoding_driver_->set_externally_managed(true);

    // Apply platform configuration mutation for consistency's sake.
    ApplyPlatformSpecificConfiguration(decoding_driver_.get());
    // Inserts platform-specific rewriters into the resource_filter_map_, so
    // that the decoding process can recognize those rewriter ids.
    AddPlatformSpecificDecodingPasses(decoding_driver_.get());
    // This call is for backwards compatibility.  When adding new platform
    // specific rewriters to implementations of RewriteDriverFactory, please
    // do not rely on this call to include them in the decoding process.
    // Instead, add them to your implementation of
    // AddPlatformSpecificDecodingPasses.
    AddPlatformSpecificRewritePasses(decoding_driver_.get());
    decoding_server_context_->set_decoding_driver(decoding_driver_.get());
  }
  server_context->set_decoding_driver(decoding_driver_.get());
}

void RewriteDriverFactory::InitStubDecodingServerContext(ServerContext* sc) {
  sc->set_timer(timer());
  sc->set_url_namer(url_namer());
  sc->set_hasher(hasher());
  sc->set_message_handler(message_handler());
  NullStatistics* null_stats = new NullStatistics();
  TakeOwnership(null_stats);
  InitStats(null_stats);
  sc->set_statistics(null_stats);
  sc->set_hasher(hasher());
  sc->set_signature(signature());
  sc->PostInitHook();
}

void RewriteDriverFactory::AddPlatformSpecificDecodingPasses(
    RewriteDriver* driver) {
}

void RewriteDriverFactory::AddPlatformSpecificRewritePasses(
    RewriteDriver* driver) {
}

void RewriteDriverFactory::ApplyPlatformSpecificConfiguration(
    RewriteDriver* driver) {
}

void RewriteDriverFactory::AddPlatformSpecificUserAgentNormalizers(
    std::vector<const UserAgentNormalizer*>* out) {
}

UrlAsyncFetcher* RewriteDriverFactory::ComputeUrlAsyncFetcher() {
  if (url_async_fetcher_ == NULL) {
    // Run any hooks like setting up slurp directory.
    FetcherSetupHooks();
    if (slurp_directory_.empty()) {
      if (base_url_async_fetcher_.get() == NULL) {
        url_async_fetcher_ = DefaultAsyncUrlFetcher();
      } else {
        url_async_fetcher_ = base_url_async_fetcher_.get();
      }
    } else {
      SetupSlurpDirectories();
    }
  }
  return url_async_fetcher_;
}

void RewriteDriverFactory::SetupSlurpDirectories() {
  CHECK(!FetchersComputed());
  if (slurp_read_only_) {
    CHECK(!FetchersComputed());
    HttpDumpUrlFetcher* dump_fetcher = new HttpDumpUrlFetcher(
        slurp_directory_, file_system(), timer());
    dump_fetcher->set_print_urls(slurp_print_urls_);
    url_async_fetcher_ = dump_fetcher;
  } else {
    // Check to see if the factory already had set_base_url_async_fetcher
    // called on it.  If so, then we'll want to use that fetcher
    // as the mechanism for the dump-writer to retrieve missing
    // content from the internet so it can be saved in the slurp
    // directory.
    url_async_fetcher_ = base_url_async_fetcher_.get();
    if (url_async_fetcher_ == NULL) {
      url_async_fetcher_ = DefaultAsyncUrlFetcher();
    }
    HttpDumpUrlAsyncWriter* dump_writer = new HttpDumpUrlAsyncWriter(
        slurp_directory_, url_async_fetcher_, file_system(), timer());
    dump_writer->set_print_urls(slurp_print_urls_);
    url_async_fetcher_ = dump_writer;
  }
}

void RewriteDriverFactory::FetcherSetupHooks() {
}

StringPiece RewriteDriverFactory::LockFilePrefix() {
  return filename_prefix_;
}

void RewriteDriverFactory::StopCacheActivity() {
  ScopedMutex lock(server_context_mutex_.get());

  // Make sure we tell HTTP cache not to write out fetch failures, as
  // fetcher shutdown may create artificial ones, and we don't want to
  // remember those.
  //
  // Note that we also cannot access our own http_cache_ since it may be
  // NULL in case like Apache where server contexts get their own.
  for (ServerContextSet::iterator p = server_contexts_.begin();
       p != server_contexts_.end(); ++p) {
    HTTPCache* cache = (*p)->http_cache();
    if (cache != NULL) {
      cache->SetIgnoreFailurePuts();
    }
  }

  // Similarly stop metadata cache writes.
  for (ServerContextSet::iterator p = server_contexts_.begin();
       p != server_contexts_.end(); ++p) {
    ServerContext* server_context = *p;
    server_context->set_shutting_down();
  }
}

bool RewriteDriverFactory::TerminateServerContext(ServerContext* sc) {
  ScopedMutex lock(server_context_mutex_.get());
  server_contexts_.erase(sc);
  return server_contexts_.empty();
}

void RewriteDriverFactory::ShutDown() {
  StopCacheActivity();  // Maybe already stopped, but no harm stopping it twice.

  // We first shutdown the low-priority rewrite threads, as they're meant to
  // be robust against cancellation, and it will make the jobs wrap up
  // much quicker.
  if (worker_pools_[kLowPriorityRewriteWorkers] != NULL) {
    worker_pools_[kLowPriorityRewriteWorkers]->ShutDown();
  }

  // Now get active RewriteDrivers for each manager to wrap up.
  int timeout_secs = RunningOnValgrind() ? 20 : 5;
  int64 cutoff_time_ms = timer_->NowMs() + timeout_secs * Timer::kSecondMs;

  for (ServerContextSet::iterator p = server_contexts_.begin();
       p != server_contexts_.end(); ++p) {
    ServerContext* server_context = *p;
    server_context->central_controller()->ShutDown();
    server_context->ShutDownDrivers(cutoff_time_ms);
  }

  // Shut down the remaining worker threads, to quiesce the system while
  // leaving the QueuedWorkerPool & QueuedWorkerPool::Sequence objects
  // live.  The QueuedWorkerPools will be deleted when the ServerContext
  // is destructed.
  for (int i = 0, n = worker_pools_.size(); i < n; ++i) {
    QueuedWorkerPool* worker_pool = worker_pools_[i];
    if (worker_pool != NULL) {
      worker_pool->ShutDown();
    }
  }

  // Make sure we destroy the decoding driver here, before any of the
  // server contexts get destroyed, since it's tied to one. Also clear
  // all of the references to it.
  for (ServerContextSet::iterator p = server_contexts_.begin();
       p != server_contexts_.end(); ++p) {
    ServerContext* server_context = *p;
    server_context->set_decoding_driver(NULL);
  }
  decoding_driver_.reset(NULL);
}

void RewriteDriverFactory::AddCreatedDirectory(const GoogleString& dir) {
  created_directories_.insert(dir);
}

void RewriteDriverFactory::InitStats(Statistics* statistics) {
  HTTPCache::InitStats(statistics);
  RewriteDriver::InitStats(statistics);
  RewriteStats::InitStats(statistics);
  CacheBatcher::InitStats(statistics);
  InProcessCentralController::InitStats(statistics);
  CriticalImagesFinder::InitStats(statistics);
  CriticalSelectorFinder::InitStats(statistics);
  PropertyStoreGetCallback::InitStats(statistics);
}

void RewriteDriverFactory::Initialize() {
  RewriteDriver::Initialize();
}

void RewriteDriverFactory::Terminate() {
  RewriteDriver::Terminate();
}

void RewriteDriverFactory::SetStatistics(Statistics* statistics) {
  statistics_ = statistics;
  rewrite_stats_.reset(NULL);
}

RewriteStats* RewriteDriverFactory::rewrite_stats() {
  if (rewrite_stats_.get() == NULL) {
    rewrite_stats_.reset(new RewriteStats(
        HasWaveforms(), statistics_, thread_system_.get(), timer()));
  }
  return rewrite_stats_.get();
}

RewriteOptions* RewriteDriverFactory::NewRewriteOptions() {
  return new RewriteOptions(thread_system());
}

RewriteOptions* RewriteDriverFactory::NewRewriteOptionsForQuery() {
  return NewRewriteOptions();
}

ExperimentMatcher* RewriteDriverFactory::NewExperimentMatcher() {
  return new ExperimentMatcher;
}

}  // namespace net_instaweb
