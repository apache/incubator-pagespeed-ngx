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

// Author: abliss@google.com (Adam Bliss)

// Base class for tests which want a ResourceManager.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_TEST_BASE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_TEST_BASE_H_

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/fake_url_async_fetcher.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/http_cache.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/mock_url_fetcher.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include <string>

#define URL_PREFIX "http://www.example.com/"

namespace net_instaweb {

class ResourceManagerTestBase : public HtmlParseTestBaseNoAlloc {
 protected:
  ResourceManagerTestBase() : mock_url_async_fetcher_(&mock_url_fetcher_),
                              rewrite_driver_(&message_handler_, &file_system_,
                                              &mock_url_async_fetcher_),
                              lru_cache_(new LRUCache(100 * 1000 * 1000)),
                              mock_timer_(0),
                              http_cache_(lru_cache_, &mock_timer_),
                              url_prefix_(URL_PREFIX),
                              num_shards_(0) {
  }

  virtual void SetUp() {
    HtmlParseTestBaseNoAlloc::SetUp();
    file_prefix_ = GTestTempDir() + "/";
    resource_manager_ = NewResourceManager(&mock_hasher_);
    rewrite_driver_.SetResourceManager(resource_manager_);
  }

  virtual void TearDown() {
    delete resource_manager_;
    HtmlParseTestBaseNoAlloc::TearDown();
  }

  // In this set of tests, we will provide explicit body tags, so
  // the test harness should not add them in for our convenience.
  // It can go ahead and add the <html> and </html>, however.
  virtual bool AddBody() const {
    return false;
  }

  // Create new ResourceManager. These are owned by the caller.
  ResourceManager* NewResourceManager(Hasher* hasher) {
    return new ResourceManager(
        file_prefix_, url_prefix_, num_shards_, &file_system_,
        &filename_encoder_, &mock_url_async_fetcher_, hasher, &http_cache_,
                               &domain_lawyer_);
  }


  // FileSystem that rejects requests for unrooted input files
  // and allows the opening of input files to be temporarily
  // disabled.
  class RootedFileSystem : public StdioFileSystem {
   public:
    RootedFileSystem() : enabled_(true) { }
    virtual InputFile* OpenInputFile(const char* filename,
                                     MessageHandler* message_handler) {
      EXPECT_EQ('/', filename[0]);
      if (!enabled_) {
        return NULL;
      }
      return StdioFileSystem::OpenInputFile(filename, message_handler);
    }
    void enable() {
      enabled_ = true;
    }
    void disable() {
      enabled_ = false;
    }
   private:
    bool enabled_;
  };

  virtual HtmlParse* html_parse() { return rewrite_driver_.html_parse(); }

  // Initializes a resource for mock fetching.
  void InitMetaData(const StringPiece& resource_name,
                    const ContentType& content_type,
                    const StringPiece& content,
                    int64 ttl) {
    std::string name = StrCat("http://test.com/", resource_name);
    SimpleMetaData response_headers;
    resource_manager_->SetDefaultHeaders(&content_type, &response_headers);
    response_headers.RemoveAll(HttpAttributes::kCacheControl);
    response_headers.Add(
        HttpAttributes::kCacheControl,
        StringPrintf("public, max-age=%ld", static_cast<long>(ttl)).c_str());
    mock_url_fetcher_.SetResponse(name, response_headers, content);
  }

  // Callback that can be used for testing resource fetches.  As all the
  // async fetchers in unit-tests call their callbacks immediately, it
  // is safe to put this on the stack, rather than having it self-delete.
  class FetchCallback : public UrlAsyncFetcher::Callback {
   public:
    FetchCallback() : success_(false), done_(false) {}
    virtual void Done(bool success) {
      success_ = success;
      done_ = true;
    }
    bool success() const { return success_; }
   private:
    bool success_;
    bool done_;
  };

  // Helper function to test resource fetching, returning true if the fetch
  // succeeded, and modifying content.  It is up to the caller to EXPECT_TRUE
  // on the status and EXPECT_EQ on the content.
  bool ServeResource(const char* id, const StringPiece& name, const char* ext,
                     std::string* content) {
    SimpleMetaData request_headers, response_headers;
    content->clear();
    StringWriter writer(content);
    FetchCallback callback;
    ResourceNamer namer;
    namer.set_id(id);
    namer.set_name(name);
    namer.set_hash("0");
    namer.set_ext(ext);
    std::string url = StrCat(URL_PREFIX, namer.Encode());
    bool fetched = rewrite_driver_.FetchResource(
        url, request_headers, &response_headers, &writer, &message_handler_,
        &callback);
    return fetched && callback.success();
  }

  MockUrlFetcher mock_url_fetcher_;
  FakeUrlAsyncFetcher mock_url_async_fetcher_;
  RootedFileSystem file_system_;
  FilenameEncoder filename_encoder_;
  RewriteDriver rewrite_driver_;
  LRUCache* lru_cache_;
  MockTimer mock_timer_;
  HTTPCache http_cache_;
  DomainLawyer domain_lawyer_;
  MockHasher mock_hasher_;
  std::string file_prefix_;
  std::string url_prefix_;
  int num_shards_;
  ResourceManager* resource_manager_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_TEST_BASE_H_
