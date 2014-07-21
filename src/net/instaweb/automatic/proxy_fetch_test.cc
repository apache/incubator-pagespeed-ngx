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

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
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

  RewriteDriver* driver() const { return driver_; }

 private:
  bool complete_;
  DISALLOW_COPY_AND_ASSIGN(MockProxyFetch);
};

// A wrapper around MockProxyFetch that manages all objects on the heap and
// initializes things for no parsing and then parses a token script. You must
// NOT call Done() on the MockProxyFetch object as we do it in our destructor.
class ManagedMockProxyFetch {
 public:
  ManagedMockProxyFetch(ServerContext* server_context,
                        bool allow_options_to_be_set_by_cookies,
                        StringPiece sticky_query_parameters,
                        StringPiece sticky_query_parameters_token,
                        StringPiece query_params, StringPiece option_cookies) {
    server_context->global_options()->ClearSignatureForTesting();
    server_context->global_options()->set_allow_options_to_be_set_by_cookies(
        allow_options_to_be_set_by_cookies);
    server_context->global_options()->set_sticky_query_parameters(
        sticky_query_parameters);
    // Suppresses parsing and the invocation of mock_proxy_fetch_->Done().
    server_context->global_options()->set_max_html_parse_bytes(0L);
    server_context->global_options()->ComputeSignature();
    message_handler_.reset(new NullMessageHandler());
    async_fetch_.reset(new StringAsyncFetch(
        RequestContext::NewTestRequestContext(
            server_context->thread_system())));
    async_fetch_->response_headers()->Add("Content-Type", "text/html");
    async_fetch_->response_headers()->ComputeCaching();
    async_fetch_->request_context()->set_sticky_query_parameters_token(
        sticky_query_parameters_token);
    fetch_factory_.reset(new ProxyFetchFactory(server_context));
    mock_proxy_fetch_ = new MockProxyFetch(async_fetch_.get(),
                                           fetch_factory_.get(),
                                           server_context);
    mock_proxy_fetch_->driver()->set_pagespeed_query_params(query_params);
    mock_proxy_fetch_->driver()->set_pagespeed_option_cookies(option_cookies);
    mock_proxy_fetch_->Write("<html>HTML</html>.", message_handler_.get());
    mock_proxy_fetch_->Flush(message_handler_.get());
  }

  ~ManagedMockProxyFetch() {
    mock_proxy_fetch_->Done(true);
  }

  MockProxyFetch* mock_proxy_fetch() const { return mock_proxy_fetch_; }

 private:
  scoped_ptr<NullMessageHandler> message_handler_;
  scoped_ptr<StringAsyncFetch> async_fetch_;
  scoped_ptr<ProxyFetchFactory> fetch_factory_;
  MockProxyFetch* mock_proxy_fetch_;  // Not scoped_ptr as it self-deletes.
};

class ProxyFetchPropertyCallbackCollectorTest : public RewriteTestBase {
 public:
  void PostLookupTask() {
    post_lookup_called_ = true;
  }
  void CheckPageNotNullPostLookupTask(
      ProxyFetchPropertyCallbackCollector* collector,
      WorkerTestBase::SyncPoint* sync_point) {
    EXPECT_TRUE(collector->fallback_property_page() != NULL);
    sync_point->Notify();
  }

  void AddCheckPageNotNullPostLookupTask(
      ProxyFetchPropertyCallbackCollector* collector,
      WorkerTestBase::SyncPoint* sync_point) {
    collector->AddPostLookupTask(MakeFunction(
        this,
        &ProxyFetchPropertyCallbackCollectorTest::
            CheckPageNotNullPostLookupTask,
        collector,
        sync_point));
  }

 protected:
  ProxyFetchPropertyCallbackCollectorTest() :
    thread_system_(Platform::CreateThreadSystem()),
    server_context_(server_context()),
    post_lookup_called_(false) {
  }

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

  void EnableCollectorPrefix() {
    ThreadSynchronizer* sync = server_context()->thread_synchronizer();
    sync->EnableForPrefix(ProxyFetch::kCollectorDoneFinish);
    sync->EnableForPrefix(ProxyFetch::kCollectorDetachFinish);
    sync->EnableForPrefix(ProxyFetch::kCollectorConnectProxyFetchFinish);
    sync->EnableForPrefix(ProxyFetch::kCollectorRequestHeadersCompleteFinish);
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
            "hash", UserAgentMatcher::kDesktop, collector, mutex);
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
    EnableCollectorPrefix();
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
    collector->RequestHeadersComplete();
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
  EnableCollectorPrefix();
  scoped_ptr<ProxyFetchPropertyCallbackCollector> collector;
  collector.reset(MakeCollector());
  // This should not fail
  collector->Detach(HttpStatus::kUnknownStatusCode);
}

TEST_F(ProxyFetchPropertyCallbackCollectorTest, DoneBeforeDetach) {
  EnableCollectorPrefix();
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
  EnableCollectorPrefix();
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
  EnableCollectorPrefix();
  // Test that calling Detach() before Done() works.
  ProxyFetchPropertyCallbackCollector* collector = MakeCollector();
  collector->RequestHeadersComplete();
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
  EnableCollectorPrefix();
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
  EnableCollectorPrefix();
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

  // Not yet complete since RequestHeadersComplete() not called yet.
  EXPECT_FALSE(mock_proxy_fetch->complete());

  collector->RequestHeadersComplete();

  // Should be complete since both Done() and RequestHeadersComplete() called.
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
  EnableCollectorPrefix();
  // This test will check PostLookup tasks should not have Null
  // fallback_property_page.
  scoped_ptr<ProxyFetchPropertyCallbackCollector> collector;
  collector.reset(MakeCollector());
  collector->RequestHeadersComplete();
  ProxyFetchPropertyCallback* page_callback = AddCallback(
      collector.get(), ProxyFetchPropertyCallback::kPropertyCachePage);
  ExpectStringAsyncFetch async_fetch(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  ProxyFetchFactory factory(server_context_);
  MockProxyFetch* mock_proxy_fetch = new MockProxyFetch(
      &async_fetch, &factory, server_context_);

  ThreadSystem* thread_system = server_context()->thread_system();
  QueuedWorkerPool pool(1, "test", thread_system);
  QueuedWorkerPool::Sequence* sequence = pool.NewSequence();
  WorkerTestBase::SyncPoint sync_point(thread_system);
  sequence->Add(
      MakeFunction(
          static_cast<ProxyFetchPropertyCallbackCollectorTest*>(this),
          &ProxyFetchPropertyCallbackCollectorTest::
              AddCheckPageNotNullPostLookupTask,
          collector.get(), &sync_point));
  page_callback->Done(true);
  sync_point.Wait();
  collector->ConnectProxyFetch(mock_proxy_fetch);
  mock_proxy_fetch->Done(true);
}


TEST_F(ProxyFetchPropertyCallbackCollectorTest, TestOptionsValid) {
  RewriteOptions* options = new RewriteOptions(thread_system_.get());
  ProxyFetchPropertyCallbackCollector* collector =
      new ProxyFetchPropertyCallbackCollector(
          server_context_,
          RewriteTestBase::kTestDomain,
          RequestContext::NewTestRequestContext(thread_system_.get()),
          options,
          UserAgentMatcher::kDesktop);
  ThreadSynchronizer* sync = server_context()->thread_synchronizer();
  sync->EnableForPrefix(ProxyFetch::kCollectorFinish);
  sync->EnableForPrefix(ProxyFetch::kCollectorDetachStart);
  sync->EnableForPrefix(ProxyFetch::kCollectorRequestHeadersCompleteFinish);
  collector->RequestHeadersComplete();
  ProxyFetchPropertyCallback* page_callback = AddCallback(
      collector, ProxyFetchPropertyCallback::kPropertyCachePage);

  collector->Detach(HttpStatus::kUnknownStatusCode);
  delete options;
  collector->IsCacheValid(1L);
  sync->Signal(ProxyFetch::kCollectorDetachStart);
  page_callback->Done(true);
  sync->Wait(ProxyFetch::kCollectorFinish);
}

TEST_F(ProxyFetchTest, TestStickyPageSpeedOptions) {
  // The various options we set as cookies.
  StringPieceVector pagespeed_options;
  pagespeed_options.push_back("PageSpeedImageRecompressionQuality=77");
  pagespeed_options.push_back("ModPagespeedImageLimitOptimizedPercent=55");
  pagespeed_options.push_back("ModPagespeedCssInlineMaxBytes=19");
  GoogleString cookies = JoinCollection(pagespeed_options, "&");
  // Add the sticky query param as a QP to show that it's not set as a cookie.
  GoogleString params = StrCat("PageSpeedStickyQueryParameters=on&", cookies);
  // For producing the expected response headers cookies string.
  const char kCookiesPrefix[] = "[\"";
  const char kCookieSetExpires[] = "; Expires=Tue, 02 Feb 2010 19:01:26 GMT";
  const char kCookieClearExpires[] = "; Expires=Thu, 01 Jan 1970 00:00:00 GMT";
  const char kCookieAttrs[] = "; Domain=www.google.com; Path=/; HttpOnly";
  const char kCookiesSeparator[] = "\",\"";
  const char kCookiesSuffix[] = "\"]";
  GoogleString current_cookies =
      StrCat(kCookiesPrefix,
             JoinCollection(pagespeed_options, StrCat(kCookieSetExpires,
                                                      kCookieAttrs,
                                                      kCookiesSeparator)),
             kCookieSetExpires, kCookieAttrs, kCookiesSuffix);
  GoogleString expired_cookies =
      StrCat(kCookiesPrefix,
             JoinCollection(pagespeed_options, StrCat(kCookieClearExpires,
                                                      kCookieAttrs,
                                                      kCookiesSeparator)),
             kCookieClearExpires, kCookieAttrs, kCookiesSuffix);
  // Strip out the values from the expiration cookies. Pardon the hackiness.
  GlobalReplaceSubstring("=77;", ";", &expired_cookies);
  GlobalReplaceSubstring("=55;", ";", &expired_cookies);
  GlobalReplaceSubstring("=19;", ";", &expired_cookies);

  // 0. Allow options to be set by cookie but don't allow them to be sticky.
  {
    ManagedMockProxyFetch managed_mock(server_context_, true, "", "right value",
                                       params, "");
    MockProxyFetch* mock_proxy_fetch = managed_mock.mock_proxy_fetch();
    GoogleString response_cookies;
    EXPECT_FALSE(mock_proxy_fetch->response_headers()->GetCookieString(
        &response_cookies));
    EXPECT_STREQ("", response_cookies);
  }
  // 1. Allow options to be set by cookie, allow them to be sticky, but don't
  //    ask for stickiness in the request.
  {
    ManagedMockProxyFetch managed_mock(server_context_, true, "right value", "",
                                       params, "");
    MockProxyFetch* mock_proxy_fetch = managed_mock.mock_proxy_fetch();
    GoogleString response_cookies;
    EXPECT_FALSE(mock_proxy_fetch->response_headers()->GetCookieString(
        &response_cookies));
    EXPECT_STREQ("", response_cookies);
  }
  // 2. Don't allow options to be set by cookie, allow them to be made sticky,
  //    don't ask for stickiness in the request, but pass in some already-set
  //    cookies - these should be expired for us.
  {
    ManagedMockProxyFetch managed_mock(server_context_, false, "right value",
                                       "", "", cookies);
    MockProxyFetch* mock_proxy_fetch = managed_mock.mock_proxy_fetch();
    GoogleString response_cookies;
    EXPECT_TRUE(mock_proxy_fetch->response_headers()->GetCookieString(
        &response_cookies));
    EXPECT_STREQ(expired_cookies, response_cookies);
  }
  // 3. Allow options to be set by cookie, allow them to be made sticky, and
  //    ask for stickiness in the request.
  {
    ManagedMockProxyFetch managed_mock(server_context_, true, "right value",
                                       "right value", params, "");
    MockProxyFetch* mock_proxy_fetch = managed_mock.mock_proxy_fetch();
    GoogleString response_cookies;
    EXPECT_TRUE(mock_proxy_fetch->response_headers()->GetCookieString(
        &response_cookies));
    EXPECT_STREQ(current_cookies, response_cookies);
  }
  // 4. Pass in some options-as-cookies without any query params or changes.
  //    We expect no cookies in the response as the browser has them already.
  {
    ManagedMockProxyFetch managed_mock(server_context_, true, "right value", "",
                                       "", cookies);
    MockProxyFetch* mock_proxy_fetch = managed_mock.mock_proxy_fetch();
    GoogleString response_cookies;
    EXPECT_FALSE(mock_proxy_fetch->response_headers()->GetCookieString(
        &response_cookies));
    EXPECT_STREQ("", response_cookies);
  }
  // 5. Explicitly ask for sticky options-by-cookies to be cleared.
  {
    ManagedMockProxyFetch managed_mock(server_context_, true, "right value",
                                       "different value", "", cookies);
    MockProxyFetch* mock_proxy_fetch = managed_mock.mock_proxy_fetch();
    GoogleString response_cookies;
    EXPECT_TRUE(mock_proxy_fetch->response_headers()->GetCookieString(
        &response_cookies));
    EXPECT_STREQ(expired_cookies, response_cookies);
  }
}

}  // namespace net_instaweb
