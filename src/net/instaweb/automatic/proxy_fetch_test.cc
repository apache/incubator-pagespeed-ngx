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

// Author: mdw@google.com (Matt Welsh)

// Unit-tests for ProxyFetch and related classes

#include "net/instaweb/automatic/public/proxy_fetch.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/platform.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/thread_synchronizer.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {

// A stripped-down mock of ProxyFetch, used for testing PropertyCacheComplete().
class MockProxyFetch : public ProxyFetch {
 public:
  MockProxyFetch(AsyncFetch* async_fetch,
                 ProxyFetchFactory* factory,
                 ServerContext* server_context)
      : ProxyFetch("http://www.google.com", false,
                   NULL,  // callback
                   async_fetch,
                   NULL,  // no original content fetch
                   GetNewRewriteDriver(server_context, async_fetch),
                   server_context,
                   NULL,  // timer
                   factory),
        complete_(false) {
    response_headers()->set_status_code(HttpStatus::kOK);
  }

  ~MockProxyFetch() { }

  static RewriteDriver* GetNewRewriteDriver(ServerContext* server_context,
                                     AsyncFetch* async_fetch) {
    RewriteDriver* driver = server_context->NewRewriteDriver(
                                async_fetch->request_context());
    RequestHeaders headers;
    driver->SetRequestHeaders(headers);
    return driver;
  }

  void PropertyCacheComplete(
      ProxyFetchPropertyCallbackCollector* callback_collector) {
    complete_ = true;
  }

  void Done(bool success) {
    HandleDone(success);
  }

  bool complete() const { return complete_; }

 private:
  bool complete_;
  DISALLOW_COPY_AND_ASSIGN(MockProxyFetch);
};


class ProxyFetchPropertyCallbackCollectorTest : public RewriteTestBase {
 public:
  void PostLookupTask() {
    post_lookup_called_ = true;
  }
  void CheckPageNotNullPostLookupTask(
      ProxyFetchPropertyCallbackCollector* collector) {
    EXPECT_TRUE(collector->fallback_property_page() != NULL);
  }

  void AddCheckPageNotNullPostLookupTask(
      ProxyFetchPropertyCallbackCollector* collector) {
    collector->AddPostLookupTask(MakeFunction(
        this,
        &ProxyFetchPropertyCallbackCollectorTest::
            CheckPageNotNullPostLookupTask,
        collector));
  }

 protected:
  ProxyFetchPropertyCallbackCollectorTest() :
    thread_system_(Platform::CreateThreadSystem()),
    server_context_(server_context()),
    post_lookup_called_(false) {}

  scoped_ptr<ThreadSystem> thread_system_;
  ServerContext* server_context_;

  // Create a collector.
  ProxyFetchPropertyCallbackCollector* MakeCollector() {
    ProxyFetchPropertyCallbackCollector* collector =
        new ProxyFetchPropertyCallbackCollector(
            server_context_, RewriteTestBase::kTestDomain,
            RequestContext::NewTestRequestContext(thread_system_.get()),
            options(), UserAgentMatcher::kDesktop);
    // Collector should not contain any PropertyPages
    EXPECT_EQ(NULL, collector->ReleasePropertyPage(
        ProxyFetchPropertyCallback::kPropertyCachePage));

    return collector;
  }

  // Add a callback to the collector.
  ProxyFetchPropertyCallback* AddCallback(
      ProxyFetchPropertyCallbackCollector* collector,
      ProxyFetchPropertyCallback::PageType page_type) {
    AbstractMutex* mutex = thread_system_->NewMutex();
    DCHECK(page_type == ProxyFetchPropertyCallback::kPropertyCachePage);
    ProxyFetchPropertyCallback* callback =
        new ProxyFetchPropertyCallback(
            page_type, page_property_cache(), RewriteTestBase::kTestDomain,
            UserAgentMatcher::kDesktop, collector, mutex);
    EXPECT_EQ(page_type, callback->page_type());
    collector->AddCallback(callback);
    return callback;
  }

  void AddPostLookupConnectProxyFetchCallDone(
      ProxyFetchPropertyCallbackCollector* collector,
      MockProxyFetch* mock_proxy_fetch,
      ProxyFetchPropertyCallback* callback) {
    collector->AddPostLookupTask(MakeFunction(
        this, &ProxyFetchPropertyCallbackCollectorTest::PostLookupTask));
    collector->ConnectProxyFetch(mock_proxy_fetch);
    callback->Done(true);
  }

  void ConnectProxyFetchAddPostLookupCallDone(
      ProxyFetchPropertyCallbackCollector* collector,
      MockProxyFetch* mock_proxy_fetch,
      ProxyFetchPropertyCallback* callback) {
    collector->ConnectProxyFetch(mock_proxy_fetch);
    collector->AddPostLookupTask(MakeFunction(
        this, &ProxyFetchPropertyCallbackCollectorTest::PostLookupTask));
    callback->Done(true);
  }

  void CallDoneAddPostLookupConnectProxyFetch(
      ProxyFetchPropertyCallbackCollector* collector,
      MockProxyFetch* mock_proxy_fetch,
      ProxyFetchPropertyCallback* callback) {
    callback->Done(true);
    collector->AddPostLookupTask(MakeFunction(
        this, &ProxyFetchPropertyCallbackCollectorTest::PostLookupTask));
    collector->ConnectProxyFetch(mock_proxy_fetch);
  }

  void TestAddPostlookupTask(bool add_before_done,
                             bool add_before_proxy_fetch) {
    GoogleString kUrl("http://www.test.com/");
    scoped_ptr<ProxyFetchPropertyCallbackCollector> collector;
    collector.reset(MakeCollector());
    ProxyFetchPropertyCallback* page_callback = AddCallback(
        collector.get(), ProxyFetchPropertyCallback::kPropertyCachePage);
    ExpectStringAsyncFetch async_fetch(
        true, RequestContext::NewTestRequestContext(thread_system_.get()));
    ProxyFetchFactory factory(server_context_);
    MockProxyFetch* mock_proxy_fetch = new MockProxyFetch(
        &async_fetch, &factory, server_context_);
    if (add_before_done && add_before_proxy_fetch) {
      AddPostLookupConnectProxyFetchCallDone(
          collector.get(), mock_proxy_fetch, page_callback);
    } else if (add_before_done && !add_before_proxy_fetch) {
      ConnectProxyFetchAddPostLookupCallDone(
          collector.get(), mock_proxy_fetch, page_callback);
    } else if (!add_before_done && add_before_proxy_fetch) {
      CallDoneAddPostLookupConnectProxyFetch(
          collector.get(), mock_proxy_fetch, page_callback);
    } else {
      // Not handled. Make this fail.
    }
    EXPECT_TRUE(post_lookup_called_);
    mock_proxy_fetch->Done(true);
  }

 private:
  bool post_lookup_called_;

  DISALLOW_COPY_AND_ASSIGN(ProxyFetchPropertyCallbackCollectorTest);
};


// Test fixture for ProxyFetch.
class ProxyFetchTest : public RewriteTestBase {
};

TEST_F(ProxyFetchTest, TestInhibitParsing) {
  NullMessageHandler handler;
  server_context()->global_options()->ClearSignatureForTesting();
  server_context()->global_options()->set_max_html_parse_bytes(0L);
  server_context()->global_options()->ComputeSignature();
  StringAsyncFetch fetch(
      RequestContext::NewTestRequestContext(
          server_context()->thread_system()));
  fetch.response_headers()->Add("Content-Type", "text/html");
  fetch.response_headers()->ComputeCaching();
  ProxyFetchFactory factory(server_context_);
  MockProxyFetch* mock_proxy_fetch = new MockProxyFetch(
      &fetch, &factory, server_context());
  mock_proxy_fetch->Write("<html>HTML</html>.", &handler);
  mock_proxy_fetch->Flush(&handler);

  // We never parsed the HTML, but we did log HTML content type.
  EXPECT_FALSE(mock_proxy_fetch->started_parse_);
  {
    AbstractLogRecord* log_record =
        mock_proxy_fetch->request_context()->log_record();
    ScopedMutex lock(log_record->mutex());
    LoggingInfo* info = log_record->logging_info();
    EXPECT_TRUE(info->is_html_response());
  }

  mock_proxy_fetch->Done(true);
}

TEST_F(ProxyFetchPropertyCallbackCollectorTest, EmptyCollectorTest) {
  // Test that creating an empty collector works.
  scoped_ptr<ProxyFetchPropertyCallbackCollector> collector;
  collector.reset(MakeCollector());
  // This should not fail
  collector->Detach(HttpStatus::kUnknownStatusCode);
}

TEST_F(ProxyFetchPropertyCallbackCollectorTest, DoneBeforeDetach) {
  // Test that calling Done() before Detach() works.
  ProxyFetchPropertyCallbackCollector* collector = MakeCollector();
  ProxyFetchPropertyCallback* callback = AddCallback(
      collector, ProxyFetchPropertyCallback::kPropertyCachePage);

  // callback->IsCacheValid could be called anytime before callback->Done.
  // Will return true because there are no cache invalidation URL patterns.
  EXPECT_TRUE(callback->IsCacheValid(1L));

  // Invoke the callback.
  callback->Done(true);

  // Collector should now have a page property.
  scoped_ptr<AbstractPropertyPage> page;
  page.reset(collector->ReleaseFallbackPropertyPage());
  EXPECT_TRUE(NULL != page.get());

  // This should not fail - will also delete the collector.
  collector->Detach(HttpStatus::kUnknownStatusCode);
}

TEST_F(ProxyFetchPropertyCallbackCollectorTest, UrlInvalidDoneBeforeDetach) {
  // Invalidate all URLs cached before timestamp 2.
  options_->AddUrlCacheInvalidationEntry("*", 2L, true);
  // Test that calling Done() before Detach() works.
  ProxyFetchPropertyCallbackCollector* collector = MakeCollector();
  ProxyFetchPropertyCallback* callback = AddCallback(
      collector, ProxyFetchPropertyCallback::kPropertyCachePage);

  // callback->IsCacheValid could be called anytime before callback->Done.
  // Will return false due to the invalidation entry.
  EXPECT_FALSE(callback->IsCacheValid(1L));

  // Invoke the callback.
  callback->Done(true);

  // Collector should now have a page property.
  scoped_ptr<AbstractPropertyPage> page;
  page.reset(collector->ReleaseFallbackPropertyPage());
  EXPECT_TRUE(NULL != page.get());

  // This should not fail - will also delete the collector.
  collector->Detach(HttpStatus::kUnknownStatusCode);
}

TEST_F(ProxyFetchPropertyCallbackCollectorTest, DetachBeforeDone) {
  // Test that calling Detach() before Done() works.
  ProxyFetchPropertyCallbackCollector* collector = MakeCollector();
  ProxyFetchPropertyCallback* callback = AddCallback(
      collector, ProxyFetchPropertyCallback::kPropertyCachePage);

  // callback->IsCacheValid could be called anytime before callback->Done.
  // Will return true because there are no cache invalidation URL patterns.
  EXPECT_TRUE(callback->IsCacheValid(1L));

  // Will not delete the collector since we did not call Done yet.
  collector->Detach(HttpStatus::kUnknownStatusCode);

  // This call is after Detach (but before callback->Done).
  // ProxyFetchPropertyCallbackCollector::IsCacheValid returns false if
  // detached.
  EXPECT_FALSE(callback->IsCacheValid(1L));

  // This will delete the collector.
  callback->Done(true);
}

TEST_F(ProxyFetchPropertyCallbackCollectorTest, DoneBeforeSetProxyFetch) {
  // Test that calling Done() before SetProxyFetch() works.
  scoped_ptr<ProxyFetchPropertyCallbackCollector> collector;
  collector.reset(MakeCollector());
  ProxyFetchPropertyCallback* callback = AddCallback(
      collector.get(), ProxyFetchPropertyCallback::kPropertyCachePage);

  // callback->IsCacheValid could be called anytime before callback->Done.
  // Will return true because there are no cache invalidation URL patterns.
  EXPECT_TRUE(callback->IsCacheValid(1L));

  // Invoke the callback.
  callback->Done(true);

  // Construct mock ProxyFetch to test SetProxyFetch().
  ExpectStringAsyncFetch async_fetch(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  ProxyFetchFactory factory(server_context_);
  MockProxyFetch* mock_proxy_fetch = new MockProxyFetch(
      &async_fetch, &factory, server_context_);

  // Should not be complete since SetProxyFetch() called first.
  EXPECT_FALSE(mock_proxy_fetch->complete());

  // Collector should now have a page property.
  scoped_ptr<AbstractPropertyPage> page;
  page.reset(collector->ReleaseFallbackPropertyPage());
  EXPECT_TRUE(NULL != page.get());

  collector.get()->ConnectProxyFetch(mock_proxy_fetch);
  // Should be complete since SetProxyFetch() called after Done().
  EXPECT_TRUE(mock_proxy_fetch->complete());

  // Needed for cleanup.
  mock_proxy_fetch->Done(true);
}

TEST_F(ProxyFetchPropertyCallbackCollectorTest, SetProxyFetchBeforeDone) {
  // Test that calling SetProxyFetch() before Done() works.
  scoped_ptr<ProxyFetchPropertyCallbackCollector> collector;
  collector.reset(MakeCollector());
  ProxyFetchPropertyCallback* callback = AddCallback(
      collector.get(), ProxyFetchPropertyCallback::kPropertyCachePage);

  // Construct mock ProxyFetch to test SetProxyFetch().
  ExpectStringAsyncFetch async_fetch(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  ProxyFetchFactory factory(server_context_);
  MockProxyFetch* mock_proxy_fetch = new MockProxyFetch(
      &async_fetch, &factory, server_context_);

  // callback->IsCacheValid could be called anytime before callback->Done.
  // Will return true because there are no cache invalidation URL patterns.
  EXPECT_TRUE(callback->IsCacheValid(1L));

  collector.get()->ConnectProxyFetch(mock_proxy_fetch);
  // Should not be complete since SetProxyFetch() called first.
  EXPECT_FALSE(mock_proxy_fetch->complete());

  EXPECT_TRUE(callback->IsCacheValid(1L));

  // Now invoke the callback.
  callback->Done(true);

  // Collector should now have a page property.
  scoped_ptr<AbstractPropertyPage> page;
  page.reset(collector->ReleaseFallbackPropertyPage());
  EXPECT_TRUE(NULL != page.get());

  // Should be complete since Done() called.
  EXPECT_TRUE(mock_proxy_fetch->complete());

  // Needed for cleanup.
  mock_proxy_fetch->Done(true);
}

TEST_F(ProxyFetchPropertyCallbackCollectorTest, PostLookupProxyFetchDone) {
  TestAddPostlookupTask(true, true);
}

TEST_F(ProxyFetchPropertyCallbackCollectorTest, DonePostLookupProxyFetch) {
  TestAddPostlookupTask(false, true);
}

TEST_F(ProxyFetchPropertyCallbackCollectorTest, ProxyFetchPostLookupDone) {
  TestAddPostlookupTask(true, false);
}

TEST_F(ProxyFetchPropertyCallbackCollectorTest, FallbackPagePostLookupRace) {
  // This test will check PostLookup tasks should not have Null
  // fallback_property_page.
  GoogleString kUrl("http://www.test.com/");
  scoped_ptr<ProxyFetchPropertyCallbackCollector> collector;
  collector.reset(MakeCollector());
  ProxyFetchPropertyCallback* page_callback = AddCallback(
      collector.get(), ProxyFetchPropertyCallback::kPropertyCachePage);
  ExpectStringAsyncFetch async_fetch(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  ProxyFetchFactory factory(server_context_);
  MockProxyFetch* mock_proxy_fetch = new MockProxyFetch(
      &async_fetch, &factory, server_context_);

  ThreadSynchronizer* sync = server_context()->thread_synchronizer();
  sync->EnableForPrefix(ProxyFetch::kCollectorReady);
  sync->EnableForPrefix(ProxyFetch::kCollectorDone);
  ThreadSystem* thread_system = server_context()->thread_system();
  QueuedWorkerPool pool(1, "test", thread_system);
  QueuedWorkerPool::Sequence* sequence = pool.NewSequence();
  WorkerTestBase::SyncPoint sync_point(thread_system);
  sequence->Add(
      MakeFunction(
          static_cast<ProxyFetchPropertyCallbackCollectorTest*>(this),
          &ProxyFetchPropertyCallbackCollectorTest::
              AddCheckPageNotNullPostLookupTask,
          collector.get()));
  page_callback->Done(true);
  collector->ConnectProxyFetch(mock_proxy_fetch);
  mock_proxy_fetch->Done(true);
}

}  // namespace net_instaweb
