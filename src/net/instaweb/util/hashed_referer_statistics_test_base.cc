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

// Author: jhoch@google.com (Jason Hoch)

#include "net/instaweb/util/public/hashed_referer_statistics_test_base.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hashed_referer_statistics.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

const GoogleUrl kEmptyUrl("");
const GoogleString kBase("http://www.example.com/");

const TestUrl kUrl(kBase, GoogleString(""));
const TestUrl kUrlWithoutDivLocation(kBase + GoogleString("news"),
                                     GoogleString(""));
const TestUrl kUrlWithDivLocation(kBase + GoogleString("news/us"),
                                  GoogleString("1.1.0.1"));
const TestUrl kResourceUrl(kBase + GoogleString("images/news.jpg"),
                           GoogleString(""));

}  // namespace

HashedRefererStatistics* HashedRefererStatisticsTestBase::ParentInit() {
  HashedRefererStatistics* stats = new HashedRefererStatistics(
      SharedMemRefererStatisticsTestBase::kNumberOfStrings,
      SharedMemRefererStatisticsTestBase::kStringSize,
      shmem_runtime_.get(),
      SharedMemRefererStatisticsTestBase::kPrefix,
      SharedMemRefererStatisticsTestBase::kSuffix,
      new MD5Hasher());
  stats->InitSegment(true, &message_handler_);
  return stats;
}

HashedRefererStatistics* HashedRefererStatisticsTestBase::ChildInit() {
  HashedRefererStatistics* stats = new HashedRefererStatistics(
      SharedMemRefererStatisticsTestBase::kNumberOfStrings,
      SharedMemRefererStatisticsTestBase::kStringSize,
      shmem_runtime_.get(),
      SharedMemRefererStatisticsTestBase::kPrefix,
      SharedMemRefererStatisticsTestBase::kSuffix,
      new MD5Hasher());
  stats->InitSegment(false, &message_handler_);
  return stats;
}

void HashedRefererStatisticsTestBase::TestHashed() {
  scoped_ptr<HashedRefererStatistics> stats(ParentInit());
  stats->LogPageRequestWithoutReferer(kUrl.url);
  stats->LogPageRequestWithReferer(kUrlWithoutDivLocation.url, kUrl.url);
  stats->LogResourceRequestWithReferer(kResourceUrl.url,
                                       kUrlWithoutDivLocation.url);
  stats->LogPageRequestWithReferer(kUrlWithDivLocation.url,
                                   kUrlWithoutDivLocation.url);
  EXPECT_EQ(1, stats->GetNumberOfVisitsForUrl(kUrl.url));
  EXPECT_EQ(1, stats->GetNumberOfVisitsForUrl(kUrlWithDivLocation.url));
  EXPECT_EQ(1, stats->GetNumberOfVisitsForUrl(kUrlWithoutDivLocation.url));
  EXPECT_EQ(0, stats->GetNumberOfVisitsForUrl(kResourceUrl.url));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToPage(
      kUrl.url, kUrlWithoutDivLocation.url));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToPage(
      kUrlWithoutDivLocation.url, kUrlWithDivLocation.url));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kUrlWithoutDivLocation.url, kUrlWithDivLocation.div_location));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToResource(
      kUrlWithoutDivLocation.url, kResourceUrl.url));
  GoogleString string;
  StringWriter writer(&string);
  stats->DumpOrganized(&writer, &message_handler_);
  EXPECT_EQ(3, CountSubstring(string, "visits"));
  EXPECT_EQ(2, CountSubstring(string, "refered"));
  EXPECT_EQ(2, CountSubstring(string, "page"));
  EXPECT_EQ(1, CountSubstring(string, "div location"));
  EXPECT_EQ(1, CountSubstring(string, "resource"));
  stats->GlobalCleanup(&message_handler_);
  EXPECT_EQ(0, message_handler_.SeriousMessages());
}

}  // namespace net_instaweb
