/**
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
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/cache_url_async_fetcher.h"
#include "net/instaweb/util/public/cache_url_fetcher.h"
#include "net/instaweb/util/public/fake_url_async_fetcher.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/file_system_lock_manager.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/http_cache.h"
#include "net/instaweb/util/public/http_dump_url_fetcher.h"
#include "net/instaweb/util/public/http_dump_url_writer.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

const char kInstawebResource404Count[] = "resource_404_count";
const char kInstawebSlurp404Count[] = "slurp_404_count";

RewriteDriverFactory::RewriteDriverFactory()
    : url_fetcher_(NULL),
      url_async_fetcher_(NULL),
      html_parse_(NULL),
      filename_prefix_(""),
      force_caching_(false),
      slurp_read_only_(false),
      slurp_print_urls_(false),
      resource_404_count_(NULL),
      slurp_404_count_(NULL) {
}

RewriteDriverFactory::~RewriteDriverFactory() {
  ShutDown();
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
  return (url_fetcher_ != NULL) || (url_async_fetcher_ != NULL);
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

// TODO(jmarantz): Change this to set_base_url_fetcher
void RewriteDriverFactory::set_base_url_fetcher(UrlFetcher* url_fetcher) {
  CHECK(!FetchersComputed())
      << "Cannot call set_base_url_fetcher "
      << " after ComputeUrl*Fetcher has been called";
  CHECK(base_url_async_fetcher_.get() == NULL)
      << "Only call one of set_base_url_fetcher and set_base_url_async_fetcher";
  base_url_fetcher_.reset(url_fetcher);
}

void RewriteDriverFactory::set_base_url_async_fetcher(
    UrlAsyncFetcher* url_async_fetcher) {
  CHECK(!FetchersComputed())
      << "Cannot call set_base_url_fetcher "
      << " after ComputeUrl*Fetcher has been called";
  CHECK(base_url_fetcher_.get() == NULL)
      << "Only call one of set_base_url_fetcher and set_base_url_async_fetcher";
  base_url_async_fetcher_.reset(url_async_fetcher);
}

void RewriteDriverFactory::set_hasher(Hasher* hasher) {
  hasher_.reset(hasher);
}

void RewriteDriverFactory::set_timer(Timer* timer) {
  timer_.reset(timer);
}

void RewriteDriverFactory::set_filename_encoder(FilenameEncoder* e) {
  filename_encoder_.reset(e);
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

Timer* RewriteDriverFactory::timer() {
  if (timer_ == NULL) {
    timer_.reset(DefaultTimer());
  }
  return timer_.get();
}

Hasher* RewriteDriverFactory::hasher() {
  if (hasher_ == NULL) {
    hasher_.reset(NewHasher());
  }
  return hasher_.get();
}

FilenameEncoder* RewriteDriverFactory::filename_encoder() {
  if (filename_encoder_ == NULL) {
    filename_encoder_.reset(new FilenameEncoder);
  }
  return filename_encoder_.get();
}

NamedLockManager* RewriteDriverFactory::lock_manager() {
  if (lock_manager_ == NULL) {
    lock_manager_.reset(
        new FileSystemLockManager(file_system(), timer(), message_handler()));
  }
  return lock_manager_.get();
}

bool RewriteDriverFactory::set_filename_prefix(StringPiece p) {
  p.CopyToString(&filename_prefix_);
  if (!file_system()->RecursivelyMakeDir(filename_prefix_, message_handler())) {
    message_handler()->FatalError(
        filename_prefix_.c_str(), 0,
        "Directory does not exist and cannot be created");
    return false;
  }
  return true;
}

StringPiece RewriteDriverFactory::filename_prefix() {
  return filename_prefix_;
}

HTTPCache* RewriteDriverFactory::http_cache() {
  if (http_cache_ == NULL) {
    CacheInterface* cache = DefaultCacheInterface();
    http_cache_.reset(new HTTPCache(cache, timer()));
    http_cache_->set_force_caching(force_caching_);
  }
  return http_cache_.get();
}

ResourceManager* RewriteDriverFactory::ComputeResourceManager() {
  if (resource_manager_ == NULL) {
    CHECK(!filename_prefix_.empty())
        << "Must specify --filename_prefix or call "
        << "RewriteDriverFactory::set_filename_prefix.";
    resource_manager_.reset(new ResourceManager(
        filename_prefix_, file_system(), filename_encoder(),
        ComputeUrlAsyncFetcher(), hasher(),
        http_cache(), lock_manager()));
    resource_manager_->set_store_outputs_in_file_system(
        ShouldWriteResourcesToFileSystem());
  }
  return resource_manager_.get();
}

// TODO(jmaessen): Note that we *could* re-structure the
// rewrite_driver freelist code as follows: Keep a
// std::vector<RewriteDriver*> of all rewrite drivers.  Have each
// driver hold its index in the vector (as a number or iterator).
// Keep index of first in use.  To free, swap with first in use,
// adjusting indexes, and increment first in use.  To allocate,
// decrement first in use and return that driver.  If first in use was
// 0, allocate a fresh driver and push it.

RewriteDriver* RewriteDriverFactory::NewCustomRewriteDriver(
    const RewriteOptions& options) {
  RewriteDriver* rewrite_driver =  new RewriteDriver(
      message_handler(), file_system(), ComputeUrlAsyncFetcher(), options);
  rewrite_driver->SetResourceManager(ComputeResourceManager());
  AddPlatformSpecificRewritePasses(rewrite_driver);
  rewrite_driver->AddFilters();
  return rewrite_driver;
}


RewriteDriver* RewriteDriverFactory::NewRewriteDriver() {
  ScopedMutex lock(rewrite_drivers_mutex());
  RewriteDriver* rewrite_driver = NULL;
  if (!available_rewrite_drivers_.empty()) {
    rewrite_driver = available_rewrite_drivers_.back();
    available_rewrite_drivers_.pop_back();
  } else {
    rewrite_driver = NewCustomRewriteDriver(options_);
  }
  active_rewrite_drivers_.insert(rewrite_driver);
  return rewrite_driver;
}

void RewriteDriverFactory::ReleaseRewriteDriver(
    RewriteDriver* rewrite_driver) {
  ScopedMutex lock(rewrite_drivers_mutex());
  int count = active_rewrite_drivers_.erase(rewrite_driver);
  if (count != 1) {
    LOG(ERROR) << "ReleaseRewriteDriver called with driver not in active set.";
  } else {
    available_rewrite_drivers_.push_back(rewrite_driver);
  }
}

void RewriteDriverFactory::AddPlatformSpecificRewritePasses(
    RewriteDriver* driver) {
}

UrlFetcher* RewriteDriverFactory::ComputeUrlFetcher() {
  if (url_fetcher_ == NULL) {
    // Run any hooks like setting up slurp directory.
    FetcherSetupHooks();
    if (slurp_directory_.empty()) {
      if (base_url_fetcher_.get() == NULL) {
        url_fetcher_ = DefaultUrlFetcher();
      } else {
        url_fetcher_ = base_url_fetcher_.get();
      }
    } else {
      SetupSlurpDirectories();
    }
  }
  return url_fetcher_;
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
    url_fetcher_ = dump_fetcher;
  } else {
    // Check to see if the factory already had set_base_url_fetcher
    // called on it.  If so, then we'll want to use that fetcher
    // as the mechanism for the dump-writer to retrieve missing
    // content from the internet so it can be saved in the slurp
    // directory.
    url_fetcher_ = base_url_fetcher_.get();
    if (url_fetcher_ == NULL) {
      url_fetcher_ = DefaultUrlFetcher();
    }
    HttpDumpUrlWriter* dump_writer = new HttpDumpUrlWriter(
        slurp_directory_, url_fetcher_, file_system(), timer());
    dump_writer->set_print_urls(slurp_print_urls_);
    url_fetcher_ = dump_writer;
  }

  // We do not use real async fetches when slurping.
  url_async_fetcher_ = new FakeUrlAsyncFetcher(url_fetcher_);
}

void RewriteDriverFactory::FetcherSetupHooks() {
}

void RewriteDriverFactory::ShutDown() {
  // Avoid double-destructing the url fetchers if they were not overridden
  // programmatically
  if ((url_async_fetcher_ != NULL) &&
      (url_async_fetcher_ != base_url_async_fetcher_.get())) {
    delete url_async_fetcher_;
  }
  url_async_fetcher_ = NULL;
  if ((url_fetcher_ != NULL) && (url_fetcher_ != base_url_fetcher_.get())) {
    delete url_fetcher_;
  }
  url_fetcher_ = NULL;
  STLDeleteElements(&active_rewrite_drivers_);
  STLDeleteElements(&available_rewrite_drivers_);

  file_system_.reset(NULL);
  hasher_.reset(NULL);
  filename_encoder_.reset(NULL);
  timer_.reset(NULL);
  resource_manager_.reset(NULL);
  html_parse_message_handler_.reset(NULL);
  http_cache_.reset(NULL);
  cache_fetcher_.reset(NULL);
  cache_async_fetcher_.reset(NULL);
}

void RewriteDriverFactory::Initialize(Statistics* statistics) {
  if (statistics) {
    RewriteDriver::Initialize(statistics);
    statistics->AddVariable(kInstawebResource404Count);
    statistics->AddVariable(kInstawebSlurp404Count);
    HTTPCache::Initialize(statistics);
  }
}

void RewriteDriverFactory::Increment404Count() {
  Statistics* statistics = resource_manager_->statistics();
  if (statistics != NULL) {
    if (resource_404_count_ == NULL) {
      resource_404_count_ = statistics->GetVariable(kInstawebResource404Count);
    }
    resource_404_count_->Add(1);
  }
}

void RewriteDriverFactory::IncrementSlurpCount() {
  Statistics* statistics = resource_manager_->statistics();
  if (statistics != NULL) {
    if (slurp_404_count_ == NULL) {
      slurp_404_count_ = statistics->GetVariable(kInstawebSlurp404Count);
    }
    slurp_404_count_->Add(1);
  }
}

}  // namespace net_instaweb
