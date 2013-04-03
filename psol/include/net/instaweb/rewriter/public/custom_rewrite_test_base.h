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
// Author: jmarantz@google.com (Joshua Marantz)

// Helper class to make RewriteTestBase tests that use a custom options
// subclass.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CUSTOM_REWRITE_TEST_BASE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CUSTOM_REWRITE_TEST_BASE_H_

#include <utility>

#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

template<class OptionsClass>
class CustomRewriteTestBase : public RewriteTestBase {
 public:
  class CustomTestRewriteDriverFactory : public TestRewriteDriverFactory {
   public:
    explicit CustomTestRewriteDriverFactory(MockUrlFetcher* url_fetcher,
                                            MockUrlFetcher* distributed_fetcher)
        : TestRewriteDriverFactory(GTestTempDir(), url_fetcher,
                                   distributed_fetcher) {
      InitializeDefaultOptions();
    }

    virtual OptionsClass* NewRewriteOptions() {
      return new OptionsClass;
    }
  };

  CustomRewriteTestBase()
      : RewriteTestBase(MakeFactories(&mock_url_fetcher_,
                                      &mock_distributed_fetcher_)) {
  }

  virtual ~CustomRewriteTestBase() {
    OptionsClass::Terminate();
  }

  virtual TestRewriteDriverFactory* MakeTestFactory() {
    return new CustomTestRewriteDriverFactory(&mock_url_fetcher_,
                                              &mock_distributed_fetcher_);
  }

  static void SetUpTestCase() {
  }

  static void TearDownTestCase() {
  }

 private:
  // We must call the static Initialize method on the options class before
  // we construct a factory, which will 'new' the OptionsClass.
  static std::pair<TestRewriteDriverFactory*, TestRewriteDriverFactory*>
      MakeFactories(MockUrlFetcher* mock_fetcher,
                    MockUrlFetcher* mock_distributed_fetcher) {
    OptionsClass::Initialize();

    return make_pair(
        new CustomTestRewriteDriverFactory(mock_fetcher,
                                           mock_distributed_fetcher),
        new CustomTestRewriteDriverFactory(mock_fetcher,
                                           mock_distributed_fetcher));
  }
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CUSTOM_REWRITE_TEST_BASE_H_
