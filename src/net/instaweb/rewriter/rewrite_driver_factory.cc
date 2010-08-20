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
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/http_cache.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

RewriteDriverFactory::RewriteDriverFactory()
    : html_parse_(NULL),
      filename_prefix_(""),
      url_prefix_(""),
      num_shards_(0),
      use_http_cache_(false),
      force_caching_(false) {
}

RewriteDriverFactory::~RewriteDriverFactory() {
  STLDeleteContainerPointers(rewrite_drivers_.begin(), rewrite_drivers_.end());
}

void RewriteDriverFactory::SetEnabledFilters(const StringPiece& filter_names) {
  std::vector<StringPiece> names;
  SplitStringPieceToVector(filter_names, ",", &names, true);
  for (int i = 0, n = names.size(); i < n; ++i) {
    enable_filter(std::string(names[i].data(), names[i].size()));
  }
}

void RewriteDriverFactory::set_html_parse_message_handler(
    MessageHandler* message_handler) {
  html_parse_message_handler_.reset(message_handler);
}

void RewriteDriverFactory::set_message_handler(
    MessageHandler* message_handler) {
  message_handler_.reset(message_handler);
}

void RewriteDriverFactory::set_file_system(FileSystem* file_system) {
  file_system_.reset(file_system);
}

void RewriteDriverFactory::set_url_fetcher(UrlFetcher* url_fetcher) {
  CHECK(url_async_fetcher_.get() == NULL)
      << "Only call one of set_url_fetcher and set_url_async_fetcher";
  CHECK(url_fetcher_.get() == NULL) << "Only call set_url_fetcher once";
  url_fetcher_.reset(url_fetcher);
}

void RewriteDriverFactory::set_url_async_fetcher(
    UrlAsyncFetcher* url_async_fetcher) {
  CHECK(url_fetcher_.get() == NULL)
      << "Only call one of set_url_fetcher and set_url_async_fetcher";
  CHECK(url_async_fetcher_.get() == NULL)
      << "Only call set_url_async_fetcher once";
  url_async_fetcher_.reset(url_async_fetcher);
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

HTTPCache* RewriteDriverFactory::http_cache() {
  if (http_cache_ == NULL) {
    CacheInterface* cache = DefaultCacheInterface();
    http_cache_.reset(new HTTPCache(cache, timer()));
  }
  return http_cache_.get();
}

UrlFetcher* RewriteDriverFactory::url_fetcher() {
  UrlFetcher* fetcher = url_fetcher_.get();

  if (fetcher == NULL) {
    fetcher = DefaultUrlFetcher();
    url_fetcher_.reset(fetcher);
  }

  return fetcher;
}

UrlAsyncFetcher* RewriteDriverFactory::url_async_fetcher() {
  UrlAsyncFetcher* async_fetcher = url_async_fetcher_.get();

  // If no asynchronous fetcher was explicitly set, then build a fake
  // one using the synchronous fetcher.
  if (async_fetcher == NULL) {
    async_fetcher = DefaultAsyncUrlFetcher();
    url_async_fetcher_.reset(async_fetcher);
  }

  return async_fetcher;
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

StringPiece RewriteDriverFactory::filename_prefix() {
  return filename_prefix_;
}

StringPiece RewriteDriverFactory::url_prefix() {
  // Check this lazily, so an application can look at the default value from
  // the factory before deciding whether to update it.  It's checked before
  // use in resource_manager() beflow.
  return url_prefix_;
}

ResourceManager* RewriteDriverFactory::resource_manager() {
  if (resource_manager_ == NULL) {
    CHECK(!filename_prefix_.empty())
        << "Must specify --filename_prefix or call "
        << "RewriteDriverFactory::set_filename_prefix.";
    CHECK(!url_prefix_.empty())
        << "Must specify --url_prefix or call "
        << "RewriteDriverFactory::set_url_prefix.";
    resource_manager_.reset(new ResourceManager(
        filename_prefix_, url_prefix_, num_shards_,
        file_system(), filename_encoder(), url_async_fetcher(), hasher(),
        http_cache()));
  }
  return resource_manager_.get();
}

Timer* RewriteDriverFactory::timer() {
  if (timer_ == NULL) {
    timer_.reset(DefaultTimer());
  }
  return timer_.get();
}

RewriteDriver* RewriteDriverFactory::NewRewriteDriver() {
  RewriteDriver* rewrite_driver =  new RewriteDriver(
      message_handler(), file_system(), url_async_fetcher());
  rewrite_driver->SetResourceManager(resource_manager());
  AddPlatformSpecificRewritePasses(rewrite_driver);
  rewrite_driver->AddFilters(enabled_filters_);
  {
    ScopedMutex lock(rewrite_drivers_mutex());
    rewrite_drivers_.push_back(rewrite_driver);
  }
  return rewrite_driver;
}

void RewriteDriverFactory::AddPlatformSpecificRewritePasses(
    RewriteDriver* driver) {
}

void RewriteDriverFactory::ShutDown() {
  file_system_.reset(NULL);
  url_fetcher_.reset(NULL);
  url_async_fetcher_.reset(NULL);
  hasher_.reset(NULL);
  filename_encoder_.reset(NULL);
  timer_.reset(NULL);
  resource_manager_.reset(NULL);
  html_parse_message_handler_.reset(NULL);
  http_cache_.reset(NULL);
  cache_fetcher_.reset(NULL);
  cache_async_fetcher_.reset(NULL);
}

}  // namespace net_instaweb
