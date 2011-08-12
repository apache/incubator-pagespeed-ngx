// Copyright 2011 Google Inc.
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
// Author: jhoch@google.com (Jason Hoch)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_REFERER_STATISTICS_TEST_BASE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_REFERER_STATISTICS_TEST_BASE_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/shared_mem_test_base.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class SharedMemRefererStatistics;


struct TestUrl {
 public:
  TestUrl(GoogleString input_string,
          GoogleString input_div_location,
          GoogleString query_params = GoogleString(""))
      : url(FormUrl(input_string, input_div_location, query_params)),
        div_location(input_div_location),
        string(input_string) {
  }
  const GoogleUrl url;
  const GoogleString div_location;
  const GoogleString string;

 private:
  // Helper function that puts together Url string from constructor inputs
  static GoogleString FormUrl(GoogleString input_string,
                              GoogleString input_div_location,
                              GoogleString query_params);
};

class SharedMemRefererStatisticsTestBase : public testing::Test {
 protected:
  typedef void (SharedMemRefererStatisticsTestBase::*TestMethod)();

  static const int kNumberOfStrings;
  static const int kStringSize;
  static const char kPrefix[];

  explicit SharedMemRefererStatisticsTestBase(SharedMemTestEnv* test_env);

  bool CreateChild(TestMethod method);

  // Tests that query parameter value corresponding to query parameter name
  // SharedMemRefererStatistics::kParamName is extracted properly
  void TestGetDivLocationFromUrl();
  // Tests simple functionality of referer statistics, namely logging a
  // sequence of two page requests
  void TestSimple();
  // Tests logging of a resource request
  void TestResource();
  // Tests that Urls that are identical but for query parameters are still
  // logged as the same Url
  void TestIgnoreQueryParams();
  // Tests that different Urls with the same div location and vice versa are
  // logged properly
  void TestDivLocation();
  // Tests DumpSimple method
  void TestDumpSimple();
  // Tests Dump method
  void TestDump();
  // Tests DumpOrganized method
  void TestDumpOrganized();
  // Tests accumulation of statistics simultaneously across multiple processes
  void TestMultiProcess();

  void AddChild();

  // Convience method that, for sequence url1, url2, url3, ..., performs
  //   LogPageRequest(url1, "");
  //   LogPageRequest(url2, url1.uncheckedSpec());
  //   LogPageRequest(url3, url2.uncheckedSpec());
  //        ... and so on
  //
  // If GoogleUrl("") is passed as a sequence entry, for example if the sequence
  // is url1, url2, "", url3, url4, ..., the following behavior occurs:
  //   LogPageRequest(url1, "");
  //   LogPageRequest(url2, url1.uncheckedSpec());
  //   LogPageRequest(url3, "");
  //   LogPageRequest(url4, url3.uncheckedSpec());
  //       ... and so on
  void LogSequenceOfPageRequests(SharedMemRefererStatistics* stats,
                                 const GoogleUrl* urls[],
                                 int number_of_urls);

  // Create child process
  SharedMemRefererStatistics* ChildInit();
  // Create parent process
  SharedMemRefererStatistics* ParentInit();

  scoped_ptr<SharedMemTestEnv> test_env_;
  scoped_ptr<AbstractSharedMem> shmem_runtime_;
  MockMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedMemRefererStatisticsTestBase);
};

template<typename ConcreteTestEnv>
class SharedMemRefererStatisticsTestTemplate
    : public SharedMemRefererStatisticsTestBase {
 public:
  SharedMemRefererStatisticsTestTemplate()
      : SharedMemRefererStatisticsTestBase(new ConcreteTestEnv) {
  }
};

TYPED_TEST_CASE_P(SharedMemRefererStatisticsTestTemplate);

TYPED_TEST_P(SharedMemRefererStatisticsTestTemplate,
             TestGetDivLocationFromUrl) {
  SharedMemRefererStatisticsTestBase::TestGetDivLocationFromUrl();
}

TYPED_TEST_P(SharedMemRefererStatisticsTestTemplate, TestSimple) {
  SharedMemRefererStatisticsTestBase::TestSimple();
}

TYPED_TEST_P(SharedMemRefererStatisticsTestTemplate, TestResource) {
  SharedMemRefererStatisticsTestBase::TestResource();
}

TYPED_TEST_P(SharedMemRefererStatisticsTestTemplate, TestIgnoreQueryParams) {
  SharedMemRefererStatisticsTestBase::TestIgnoreQueryParams();
}

TYPED_TEST_P(SharedMemRefererStatisticsTestTemplate, TestDivLocation) {
  SharedMemRefererStatisticsTestBase::TestDivLocation();
}

TYPED_TEST_P(SharedMemRefererStatisticsTestTemplate, TestDumpSimple) {
  SharedMemRefererStatisticsTestBase::TestDumpSimple();
}

TYPED_TEST_P(SharedMemRefererStatisticsTestTemplate, TestDump) {
  SharedMemRefererStatisticsTestBase::TestDump();
}

TYPED_TEST_P(SharedMemRefererStatisticsTestTemplate, TestDumpOrganized) {
  SharedMemRefererStatisticsTestBase::TestDumpOrganized();
}

TYPED_TEST_P(SharedMemRefererStatisticsTestTemplate, TestMultiProcess) {
  SharedMemRefererStatisticsTestBase::TestMultiProcess();
}

REGISTER_TYPED_TEST_CASE_P(SharedMemRefererStatisticsTestTemplate,
                           TestGetDivLocationFromUrl,
                           TestSimple,
                           TestResource,
                           TestIgnoreQueryParams,
                           TestDivLocation,
                           TestDumpSimple,
                           TestDump,
                           TestDumpOrganized,
                           TestMultiProcess);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_REFERER_STATISTICS_TEST_BASE_H_
