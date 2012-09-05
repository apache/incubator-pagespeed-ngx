/*
 * Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/automatic/public/static_rewriter.h"

#include <cstdio>
#include <cstdlib>  // for exit()

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/wget_url_fetcher.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_gflags.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/google_timer.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/threadsafe_cache.h"

namespace net_instaweb {

class CacheInterface;
class FileSystem;
class Hasher;
class MessageHandler;
class Statistics;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;
class Writer;

FileRewriter::FileRewriter(const net_instaweb::RewriteGflags* gflags,
                           bool echo_errors_to_stdout)
    : gflags_(gflags),
      echo_errors_to_stdout_(echo_errors_to_stdout) {
  net_instaweb::RewriteDriverFactory::Initialize(&simple_stats_);
  SetStatistics(&simple_stats_);
}

FileRewriter::~FileRewriter() {
}

Hasher* FileRewriter::NewHasher() {
  return new MD5Hasher;
}

UrlFetcher* FileRewriter::DefaultUrlFetcher() {
  return new WgetUrlFetcher;
}

UrlAsyncFetcher* FileRewriter::DefaultAsyncUrlFetcher() {
  return new FakeUrlAsyncFetcher(ComputeUrlFetcher());
}

MessageHandler* FileRewriter::DefaultHtmlParseMessageHandler() {
  if (echo_errors_to_stdout_) {
    return new GoogleMessageHandler;
  }
  return new NullMessageHandler;
}

MessageHandler* FileRewriter::DefaultMessageHandler() {
  return DefaultHtmlParseMessageHandler();
}

FileSystem* FileRewriter::DefaultFileSystem() {
  return new StdioFileSystem;
}

Timer* FileRewriter::DefaultTimer() {
  return new GoogleTimer;
}

void FileRewriter::SetupCaches(ServerContext* resource_manager) {
  LRUCache* lru_cache = new LRUCache(gflags_->lru_cache_size_bytes());
  CacheInterface* cache = new ThreadsafeCache(lru_cache,
                                              thread_system()->NewMutex());
  HTTPCache* http_cache = new HTTPCache(cache, timer(), hasher(), statistics());
  resource_manager->set_http_cache(http_cache);
  resource_manager->set_metadata_cache(cache);
  resource_manager->MakePropertyCaches(cache);
}

Statistics* FileRewriter::statistics() {
  return &simple_stats_;
}

StaticRewriter::StaticRewriter(int* argc, char*** argv)
    : gflags_((*argv)[0], argc, argv),
      file_rewriter_(&gflags_, true),
      resource_manager_(file_rewriter_.CreateServerContext()) {
  if (!gflags_.SetOptions(&file_rewriter_,
                          resource_manager_->global_options())) {
    exit(1);
  }
}

StaticRewriter::StaticRewriter()
    : file_rewriter_(&gflags_, false),
      resource_manager_(file_rewriter_.CreateServerContext()) {
  if (!gflags_.SetOptions(&file_rewriter_,
                          resource_manager_->global_options())) {
    exit(1);
  }
}

StaticRewriter::~StaticRewriter() {
}

bool StaticRewriter::ParseText(const StringPiece& url,
                               const StringPiece& id,
                               const StringPiece& text,
                               const StringPiece& output_dir,
                               Writer* writer) {
  RewriteDriver* driver = resource_manager_->NewRewriteDriver();

  // For this simple file transformation utility we always want to perform
  // any optimizations we can, so we wait until everything is done rather
  // than using a deadline, the way a server deployment would.
  driver->set_fully_rewrite_on_flush(true);

  file_rewriter_.set_filename_prefix(output_dir);
  driver->SetWriter(writer);
  if (!driver->StartParseId(url, id, kContentTypeHtml)) {
    fprintf(stderr, "StartParseId failed on url %s\n", url.as_string().c_str());
    resource_manager_->ReleaseRewriteDriver(driver);
    return false;
  }

  // Note that here we are sending the entire buffer into the parser
  // in one chunk, but it's also fine to break up the calls to
  // driver->ParseText as data streams in.  It's up to the caller when
  // to call driver->Flush().  If no calls are ever made to
  // driver->Flush(), then no HTML will be serialized until the end of
  // the document is reached, but rewriters tha work over document
  // structure will have the maximum benefit.
  driver->ParseText(text);
  driver->FinishParse();

  return true;
}

FileSystem* StaticRewriter::file_system() {
  return file_rewriter_.file_system();
}

MessageHandler* StaticRewriter::message_handler() {
  return file_rewriter_.message_handler();
}

}  // namespace net_instaweb
