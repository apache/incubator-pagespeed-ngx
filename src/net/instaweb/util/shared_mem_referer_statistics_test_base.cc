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

#include "net/instaweb/util/public/shared_mem_referer_statistics_test_base.h"

#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/shared_mem_referer_statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

const int SharedMemRefererStatisticsTestBase::kNumberOfStrings = 1024;
const int SharedMemRefererStatisticsTestBase::kStringSize = 64;
const char SharedMemRefererStatisticsTestBase::kPrefix[] = "/prefix/";
const char SharedMemRefererStatisticsTestBase::kSuffix[] = "suffix";

namespace {

// kEmptyUrl is used to convey a break in referrals to LogSequenceOfPageRequests
const GoogleUrl kEmptyUrl("");
const GoogleString kBase("http://www.example.com/");

const TestUrl kNews(kBase + GoogleString("news"),
                    GoogleString(""));
const TestUrl kUSNews(kBase + GoogleString("news/us"),
                      GoogleString("1.1.0.1"));
const TestUrl kUSNewsArticle(kBase + GoogleString("news/us/article"),
                             GoogleString("1.1.2.0"));
const TestUrl kUSNewsArticleImage(
                  kBase + GoogleString("images/news_us_article.jpg"),
                  GoogleString(""));
const TestUrl kNewUSNewsArticle(kBase + GoogleString("news/us/article2"),
                                GoogleString("1.1.2.0"));
const TestUrl kNewOldUSNewsArticle(kBase + GoogleString("news/us/article"),
                                   GoogleString("1.1.2.1.0"));
const TestUrl kAccount(kBase + GoogleString("account"),
                       GoogleString("0.0.9"));
const TestUrl kProfile(kBase + GoogleString("account/profile.html"),
                       GoogleString("1.3.0"),
                       GoogleString("user=jason"));
const TestUrl kOtherProfile(kBase + GoogleString("account/profile.html"),
                            GoogleString("1.3.0"),
                            GoogleString("user=jhoch"));

}  // namespace

GoogleString TestUrl::FormUrl(GoogleString input_string,
                              GoogleString input_div_location,
                              GoogleString query_params) {
  if (input_div_location.empty()) {
    if (query_params.empty()) {
      return input_string;
    } else {
      return StrCat(input_string, "?", query_params);
    }
  } else {
    if (query_params.empty()) {
      return StrCat(input_string, "?div_location=", input_div_location);
    } else {
      return StrCat(input_string, "?div_location=", input_div_location,
                    "&", query_params);
    }
  }
}

SharedMemRefererStatisticsTestBase::SharedMemRefererStatisticsTestBase(
    SharedMemTestEnv* test_env)
    : test_env_(test_env),
      shmem_runtime_(test_env->CreateSharedMemRuntime()) {
}

bool SharedMemRefererStatisticsTestBase::CreateChild(TestMethod method) {
  Function* callback =
      new MemberFunction0<SharedMemRefererStatisticsTestBase>(method, this);
  return test_env_->CreateChild(callback);
}

SharedMemRefererStatistics* SharedMemRefererStatisticsTestBase::ChildInit() {
  SharedMemRefererStatistics* stats = new SharedMemRefererStatistics(
      kNumberOfStrings,
      kStringSize,
      shmem_runtime_.get(),
      kPrefix,
      kSuffix);
  stats->InitSegment(false, &message_handler_);
  return stats;
}

SharedMemRefererStatistics* SharedMemRefererStatisticsTestBase::ParentInit() {
  SharedMemRefererStatistics* stats = new SharedMemRefererStatistics(
      kNumberOfStrings,
      kStringSize,
      shmem_runtime_.get(),
      kPrefix,
      kSuffix);
  stats->InitSegment(true, &message_handler_);
  return stats;
}

void SharedMemRefererStatisticsTestBase::LogSequenceOfPageRequests(
    SharedMemRefererStatistics* stats,
    const GoogleUrl* urls[],
    int number_of_urls) {
  bool previous_was_null = true;
  for (int i = 0; i < number_of_urls; i++) {
    // kEmptyUrl("") is used to signify break in referrals
    if (urls[i]->UncheckedSpec().empty()) {
      previous_was_null = true;
    } else {
      if (previous_was_null) {
        stats->LogPageRequestWithoutReferer(*urls[i]);
      } else {
        stats->LogPageRequestWithReferer(*urls[i], *urls[i - 1]);
      }
      previous_was_null = false;
    }
  }
}

void SharedMemRefererStatisticsTestBase::TestGetDivLocationFromUrl() {
  scoped_ptr<SharedMemRefererStatistics> stats(ParentInit());
  const char value[] = "0.0.0";
  GoogleString url = GoogleString("http://a.com/?") +
                     SharedMemRefererStatistics::kParamName +
                     GoogleString("=") + value;
  GoogleUrl test_url(url);
  EXPECT_EQ(value, stats->GetDivLocationFromUrl(test_url));
  stats->GlobalCleanup(&message_handler_);
  EXPECT_EQ(0, message_handler_.SeriousMessages());
}

void SharedMemRefererStatisticsTestBase::TestSimple() {
  scoped_ptr<SharedMemRefererStatistics> stats(ParentInit());
  EXPECT_EQ(0, stats->GetNumberOfVisitsForUrl(kNews.url));
  EXPECT_EQ(0, stats->GetNumberOfVisitsForUrl(kUSNews.url));
  stats->LogPageRequestWithoutReferer(kNews.url);
  EXPECT_EQ(1, stats->GetNumberOfVisitsForUrl(kNews.url));
  EXPECT_EQ(0, stats->GetNumberOfVisitsForUrl(kUSNews.url));
  stats->LogPageRequestWithReferer(kUSNews.url, kNews.url);
  EXPECT_EQ(1, stats->GetNumberOfVisitsForUrl(kNews.url));
  EXPECT_EQ(1, stats->GetNumberOfVisitsForUrl(kUSNews.url));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToPage(kNews.url,
                                                         kUSNews.url));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kNews.url, kUSNews.div_location));
  stats->GlobalCleanup(&message_handler_);
  EXPECT_EQ(0, message_handler_.SeriousMessages());
}

void SharedMemRefererStatisticsTestBase::TestResource() {
  scoped_ptr<SharedMemRefererStatistics> stats(ParentInit());
  const GoogleUrl* urls[] = {&kNews.url, &kUSNews.url, &kUSNewsArticle.url};
  LogSequenceOfPageRequests(stats.get(), urls, arraysize(urls));
  stats->LogResourceRequestWithReferer(kUSNewsArticleImage.url,
                                       kUSNewsArticle.url);
  EXPECT_EQ(1, stats->GetNumberOfVisitsForUrl(kNews.url));
  EXPECT_EQ(1, stats->GetNumberOfVisitsForUrl(kUSNews.url));
  EXPECT_EQ(1, stats->GetNumberOfVisitsForUrl(kUSNewsArticle.url));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToPage(kNews.url,
                                                         kUSNews.url));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToPage(kUSNews.url,
                                                         kUSNewsArticle.url));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kNews.url, kUSNews.div_location));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kUSNews.url, kUSNewsArticle.div_location));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToResource(
      kUSNewsArticle.url, kUSNewsArticleImage.url));
  EXPECT_EQ(0, stats->GetNumberOfVisitsForUrl(kUSNewsArticleImage.url));
  EXPECT_EQ(0, stats->GetNumberOfReferencesFromUrlToPage(
      kUSNewsArticle.url, kUSNewsArticleImage.url));
  stats->GlobalCleanup(&message_handler_);
  EXPECT_EQ(0, message_handler_.SeriousMessages());
}

void SharedMemRefererStatisticsTestBase::TestIgnoreQueryParams() {
  scoped_ptr<SharedMemRefererStatistics> stats(ParentInit());
  const GoogleUrl* urls[] = {&kNews.url, &kAccount.url, &kProfile.url};
  LogSequenceOfPageRequests(stats.get(), urls, arraysize(urls));
  stats->LogPageRequestWithReferer(kOtherProfile.url, kAccount.url);
  EXPECT_EQ(1, stats->GetNumberOfVisitsForUrl(kNews.url));
  EXPECT_EQ(1, stats->GetNumberOfVisitsForUrl(kAccount.url));
  EXPECT_EQ(2, stats->GetNumberOfVisitsForUrl(kProfile.url));
  EXPECT_EQ(2, stats->GetNumberOfVisitsForUrl(kOtherProfile.url));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToPage(kNews.url,
                                                         kAccount.url));
  EXPECT_EQ(2, stats->GetNumberOfReferencesFromUrlToPage(kAccount.url,
                                                         kProfile.url));
  EXPECT_EQ(2, stats->GetNumberOfReferencesFromUrlToPage(kAccount.url,
                                                         kOtherProfile.url));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kNews.url, kAccount.div_location));
  EXPECT_EQ(2, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kAccount.url, kProfile.div_location));
  EXPECT_EQ(2, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kAccount.url, kOtherProfile.div_location));
  stats->GlobalCleanup(&message_handler_);
  EXPECT_EQ(0, message_handler_.SeriousMessages());
}

void SharedMemRefererStatisticsTestBase::TestDivLocation() {
  scoped_ptr<SharedMemRefererStatistics> stats(ParentInit());
  const GoogleUrl* urls[] = {&kNews.url, &kUSNews.url, &kUSNewsArticle.url};
  LogSequenceOfPageRequests(stats.get(), urls, arraysize(urls));
  stats->LogPageRequestWithReferer(kNewUSNewsArticle.url, kUSNews.url);
  stats->LogPageRequestWithReferer(kNewOldUSNewsArticle.url, kUSNews.url);
  EXPECT_EQ(1, stats->GetNumberOfVisitsForUrl(kNews.url));
  EXPECT_EQ(1, stats->GetNumberOfVisitsForUrl(kUSNews.url));
  EXPECT_EQ(2, stats->GetNumberOfVisitsForUrl(kUSNewsArticle.url));
  EXPECT_EQ(1, stats->GetNumberOfVisitsForUrl(kNewUSNewsArticle.url));
  EXPECT_EQ(2, stats->GetNumberOfVisitsForUrl(kNewOldUSNewsArticle.url));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToPage(kNews.url,
                                                         kUSNews.url));
  EXPECT_EQ(2, stats->GetNumberOfReferencesFromUrlToPage(kUSNews.url,
                                                         kUSNewsArticle.url));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToPage(
      kUSNews.url, kNewUSNewsArticle.url));
  EXPECT_EQ(2, stats->GetNumberOfReferencesFromUrlToPage(
      kUSNews.url, kNewOldUSNewsArticle.url));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kNews.url, kUSNews.div_location));
  EXPECT_EQ(2, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kUSNews.url, kUSNewsArticle.div_location));
  EXPECT_EQ(2, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kUSNews.url, kNewUSNewsArticle.div_location));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kUSNews.url, kNewOldUSNewsArticle.div_location));
  stats->GlobalCleanup(&message_handler_);
  EXPECT_EQ(0, message_handler_.SeriousMessages());
}

void SharedMemRefererStatisticsTestBase::TestDumpFast() {
  scoped_ptr<SharedMemRefererStatistics> stats(ParentInit());
  const GoogleUrl* urls[] = {&kNews.url, &kUSNews.url, &kUSNewsArticle.url};
  LogSequenceOfPageRequests(stats.get(), urls, arraysize(urls));
  stats->LogResourceRequestWithReferer(kUSNewsArticleImage.url,
                                       kUSNewsArticle.url);
  GoogleString expected_dump =
      kNews.string + ": 1\n" +
      kUSNews.string + ": 1\n" +
      kUSNews.string + " p" + kNews.string + ": 1\n" +
      kUSNews.div_location + " d" + kNews.string + ": 1\n" +
      kUSNewsArticle.string + ": 1\n" +
      kUSNewsArticle.string + " p" + kUSNews.string + ": 1\n" +
      kUSNewsArticle.div_location + " d" + kUSNews.string + ": 1\n" +
      kUSNewsArticleImage.string + " r" + kUSNewsArticle.string + ": 1\n";
  GoogleString string;
  StringWriter writer(&string);
  stats->DumpFast(&writer, &message_handler_);
  EXPECT_EQ(expected_dump, string);
  stats->GlobalCleanup(&message_handler_);
  EXPECT_EQ(0, message_handler_.SeriousMessages());
}

void SharedMemRefererStatisticsTestBase::TestDumpSimple() {
  scoped_ptr<SharedMemRefererStatistics> stats(ParentInit());
  const GoogleUrl* urls[] = {&kNews.url, &kUSNews.url, &kUSNewsArticle.url};
  LogSequenceOfPageRequests(stats.get(), urls, arraysize(urls));
  stats->LogResourceRequestWithReferer(kUSNewsArticleImage.url,
                                       kUSNewsArticle.url);
  GoogleString expected_dump =
      kNews.string + " refered div location " +
          kUSNews.div_location + " : 1\n" +
      kUSNews.string + " refered div location " +
          kUSNewsArticle.div_location + " : 1\n" +
      kUSNewsArticle.string + " refered resource " +
          kUSNewsArticleImage.string + " : 1\n" +
      kNews.string + " visits: 1\n" +
      kUSNews.string + " visits: 1\n" +
      kNews.string + " refered page " + kUSNews.string + " : 1\n" +
      kUSNewsArticle.string + " visits: 1\n" +
      kUSNews.string + " refered page " + kUSNewsArticle.string + " : 1\n";
  GoogleString string;
  StringWriter writer(&string);
  stats->DumpSimple(&writer, &message_handler_);
  EXPECT_EQ(expected_dump, string);
  stats->GlobalCleanup(&message_handler_);
  EXPECT_EQ(0, message_handler_.SeriousMessages());
}

void SharedMemRefererStatisticsTestBase::TestDumpOrganized() {
  scoped_ptr<SharedMemRefererStatistics> stats(ParentInit());
  const GoogleUrl* urls[] = {&kNews.url, &kUSNews.url, &kUSNewsArticle.url};
  LogSequenceOfPageRequests(stats.get(), urls, arraysize(urls));
  stats->LogResourceRequestWithReferer(kUSNewsArticleImage.url,
                                       kUSNewsArticle.url);
  GoogleString expected_dump =
      kNews.string + " visits: 1\n" +
      kNews.string + " refered:\n" +
      "  div location " + kUSNews.div_location + " : 1\n" +
      "  page " + kUSNews.string + " : 1\n" +
      kUSNews.string + " visits: 1\n" +
      kUSNews.string + " refered:\n" +
      "  div location " + kUSNewsArticle.div_location + " : 1\n" +
      "  page " + kUSNewsArticle.string + " : 1\n" +
      kUSNewsArticle.string + " visits: 1\n" +
      kUSNewsArticle.string + " refered:\n" +
      "  resource " + kUSNewsArticleImage.string + " : 1\n";
  GoogleString string;
  StringWriter writer(&string);
  stats->DumpOrganized(&writer, &message_handler_);
  EXPECT_EQ(expected_dump, string);
  stats->GlobalCleanup(&message_handler_);
  EXPECT_EQ(0, message_handler_.SeriousMessages());
}

void SharedMemRefererStatisticsTestBase::TestMultiProcess() {
  scoped_ptr<SharedMemRefererStatistics> stats(ParentInit());
  for (int i = 0; i < 2; i++)
    ASSERT_TRUE(CreateChild(&SharedMemRefererStatisticsTestBase::AddChild));
  const GoogleUrl* urls[] = {&kNews.url, &kAccount.url, &kProfile.url,
                             &kEmptyUrl, &kNews.url, &kUSNews.url,
                             &kNewOldUSNewsArticle.url};
  LogSequenceOfPageRequests(stats.get(), urls, arraysize(urls));
  test_env_->WaitForChildren();
  EXPECT_EQ(6, stats->GetNumberOfVisitsForUrl(kNews.url));
  EXPECT_EQ(5, stats->GetNumberOfVisitsForUrl(kUSNews.url));
  EXPECT_EQ(3, stats->GetNumberOfVisitsForUrl(kUSNewsArticle.url));
  EXPECT_EQ(2, stats->GetNumberOfVisitsForUrl(kNewUSNewsArticle.url));
  EXPECT_EQ(3, stats->GetNumberOfVisitsForUrl(kNewOldUSNewsArticle.url));
  EXPECT_EQ(3, stats->GetNumberOfVisitsForUrl(kAccount.url));
  EXPECT_EQ(3, stats->GetNumberOfVisitsForUrl(kProfile.url));
  EXPECT_EQ(5, stats->GetNumberOfReferencesFromUrlToPage(kNews.url,
                                                         kUSNews.url));
  EXPECT_EQ(3, stats->GetNumberOfReferencesFromUrlToPage(kUSNews.url,
                                                         kUSNewsArticle.url));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToPage(kNews.url,
                                                         kAccount.url));
  EXPECT_EQ(2, stats->GetNumberOfReferencesFromUrlToPage(kUSNewsArticle.url,
                                                         kAccount.url));
  EXPECT_EQ(3, stats->GetNumberOfReferencesFromUrlToPage(kAccount.url,
                                                         kProfile.url));
  EXPECT_EQ(2, stats->GetNumberOfReferencesFromUrlToPage(
      kUSNews.url, kNewUSNewsArticle.url));
  EXPECT_EQ(3, stats->GetNumberOfReferencesFromUrlToPage(
      kUSNews.url, kNewOldUSNewsArticle.url));
  EXPECT_EQ(5, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kNews.url, kUSNews.div_location));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kNews.url, kAccount.div_location));
  EXPECT_EQ(4, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kUSNews.url, kUSNewsArticle.div_location));
  EXPECT_EQ(2, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kUSNewsArticle.url, kAccount.div_location));
  EXPECT_EQ(3, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kAccount.url, kProfile.div_location));
  EXPECT_EQ(4, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kUSNews.url, kNewUSNewsArticle.div_location));
  EXPECT_EQ(1, stats->GetNumberOfReferencesFromUrlToDivLocation(
      kUSNews.url, kNewOldUSNewsArticle.div_location));
  stats->GlobalCleanup(&message_handler_);
  EXPECT_EQ(0, message_handler_.SeriousMessages());
}

void SharedMemRefererStatisticsTestBase::AddChild() {
  scoped_ptr<SharedMemRefererStatistics> stats(ChildInit());
  const GoogleUrl* urls[] = {&kNews.url, &kUSNews.url, &kUSNewsArticle.url,
                             &kAccount.url, &kProfile.url, &kEmptyUrl,
                             &kNews.url, &kUSNews.url, &kNewUSNewsArticle.url};
  LogSequenceOfPageRequests(stats.get(), urls, arraysize(urls));
}

}  // namespace net_instaweb
