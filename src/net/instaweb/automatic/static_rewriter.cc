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

#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/http/public/wget_url_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/pthread_mutex.h"
#include "net/instaweb/util/public/pthread_thread_system.h"
#include "net/instaweb/util/public/stdio_file_system.h"

// The purpose of this class is to help us test that pagespeed_automatic.a
// contains all that's needed to successfully link a rewriter using standard
// g++, without using the gyp flow.
//
// TODO(jmarantz): fill out enough functionality so that this will be
// a functional static rewriter that could optimize an HTML file
// passed as a command-line parameter or via stdin.
class FileRewriter : public net_instaweb::RewriteDriverFactory {
 public:
  virtual net_instaweb::AbstractMutex* NewMutex() {
    return new net_instaweb::PthreadMutex;
  }
  virtual net_instaweb::Hasher* NewHasher() {
    return new net_instaweb::MD5Hasher;
  }
  virtual net_instaweb::UrlFetcher* DefaultUrlFetcher() {
    return new net_instaweb::WgetUrlFetcher;
  }
  virtual net_instaweb::UrlAsyncFetcher* DefaultAsyncUrlFetcher() {
    return new net_instaweb::FakeUrlAsyncFetcher(ComputeUrlFetcher());
  }
  virtual net_instaweb::MessageHandler* DefaultHtmlParseMessageHandler() {
    return new net_instaweb::GoogleMessageHandler;
  }
  virtual net_instaweb::MessageHandler* DefaultMessageHandler() {
    return new net_instaweb::GoogleMessageHandler();
  }
  virtual net_instaweb::FileSystem* DefaultFileSystem() {
    return new net_instaweb::StdioFileSystem;
  }
  virtual net_instaweb::Timer* DefaultTimer() {
    return new net_instaweb::MockTimer(0);
  }
  virtual net_instaweb::CacheInterface* DefaultCacheInterface() {
    return new net_instaweb::LRUCache(10*1000*1000);
  }
  virtual net_instaweb::AbstractMutex* cache_mutex() {
    return &cache_mutex_;
  }
  virtual net_instaweb::AbstractMutex* rewrite_drivers_mutex() {
    return &rewrite_drivers_mutex_;
  }
  virtual net_instaweb::ThreadSystem* DefaultThreadSystem() {
    return new net_instaweb::PthreadThreadSystem;
  }

 private:
  net_instaweb::PthreadMutex cache_mutex_;
  net_instaweb::PthreadMutex rewrite_drivers_mutex_;
};

// TODO(jmarantz): make this actually read HTML files from stdin or from
// argv and send them through the rewriter chain.
int main(int argc, char** argv) {
  FileRewriter file_rewriter;
  return 0;
}
