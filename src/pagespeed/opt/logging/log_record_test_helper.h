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

// Author: bharathbhushan@google.com (Bharath Bhushan Kowshik Raghupathi)

#ifndef PAGESPEED_OPT_LOGGING_LOG_RECORD_TEST_HELPER_H_
#define PAGESPEED_OPT_LOGGING_LOG_RECORD_TEST_HELPER_H_

#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/http/image_types.pb.h"
#include "pagespeed/opt/logging/enums.pb.h"
#include "pagespeed/opt/logging/logging_proto.h"
#include "pagespeed/opt/logging/log_record.h"

using ::testing::_;
using ::testing::Matcher;
using ::testing::StrEq;

namespace net_instaweb {

class AbstractMutex;

// Captures all the arguments to LogImageRewriteActivity so that we can mock it
// in test.
struct ImageRewriteInfo {
  ImageRewriteInfo(
      const char* id,
      const GoogleString& url,
      RewriterApplication::Status status,
      bool is_image_inlined,
      bool is_critical_image,
      bool is_url_rewritten,
      int size,
      bool try_low_res_src_insertion,
      bool low_res_src_inserted,
      ImageType low_res_image_type,
      int low_res_data_size) :
    id_(id), url_(url), status_(status), is_image_inlined_(is_image_inlined),
    is_critical_image_(is_critical_image), is_url_rewritten_(is_url_rewritten),
    size_(size), try_low_res_src_insertion_(try_low_res_src_insertion),
    low_res_src_inserted_(low_res_src_inserted),
    low_res_image_type_(low_res_image_type),
    low_res_data_size_(low_res_data_size) {}

  const char* id_;
  GoogleString url_;
  RewriterApplication::Status status_;
  bool is_image_inlined_;
  bool is_critical_image_;
  bool is_url_rewritten_;
  int size_;
  bool try_low_res_src_insertion_;
  bool low_res_src_inserted_;
  ImageType low_res_image_type_;
  int low_res_data_size_;
};

// A custom matcher to match more than 10 arguments allowed by MOCK_METHOD*
// macros.
Matcher<ImageRewriteInfo> LogImageRewriteActivityMatcher(
    Matcher<const char*> id,
    Matcher<const GoogleString&> url,
    Matcher<RewriterApplication::Status> status,
    Matcher<bool> is_image_inlined,
    Matcher<bool> is_critical_image,
    Matcher<bool> is_url_rewritten,
    Matcher<int> size,
    Matcher<bool> try_low_res_src_insertion,
    Matcher<bool> low_res_src_inserted,
    Matcher<ImageType> low_res_image_type,
    Matcher<int> low_res_data_size);

// A class which helps mock the methods of LogRecord for testing.
class MockLogRecord : public LogRecord {
 public:
  explicit MockLogRecord(AbstractMutex* mutex) : LogRecord(mutex) {}
  ~MockLogRecord() {}
  virtual void LogImageRewriteActivity(
      const char* id,
      const GoogleString& url,
      RewriterApplication::Status status,
      bool is_image_inlined,
      bool is_critical_image,
      bool is_url_rewritten,
      int size,
      bool try_low_res_src_insertion,
      bool low_res_src_inserted,
      ImageType low_res_image_type,
      int low_res_data_size) {
    ImageRewriteInfo info(id, url, status, is_image_inlined, is_critical_image,
                          is_url_rewritten, size, try_low_res_src_insertion,
                          low_res_src_inserted, low_res_image_type,
                          low_res_data_size);
    MockLogImageRewriteActivity(info);
  }
  MOCK_METHOD1(MockLogImageRewriteActivity, void(ImageRewriteInfo));
};

}  // namespace net_instaweb

#endif  // PAGESPEED_OPT_LOGGING_LOG_RECORD_TEST_HELPER_H_
