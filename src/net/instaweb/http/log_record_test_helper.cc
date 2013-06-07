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

#include "net/instaweb/http/public/log_record_test_helper.h"

namespace net_instaweb {

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
    Matcher<int> low_res_data_size) {
  // AllOf can only take 10 arguments. So split the 11 arguments into 7+4.
  return AllOf(
      AllOf(Field(&ImageRewriteInfo::id_, id),
            Field(&ImageRewriteInfo::url_, url),
            Field(&ImageRewriteInfo::status_, status),
            Field(&ImageRewriteInfo::is_image_inlined_, is_image_inlined),
            Field(&ImageRewriteInfo::is_critical_image_, is_critical_image),
            Field(&ImageRewriteInfo::size_, size)),
      AllOf(Field(&ImageRewriteInfo::try_low_res_src_insertion_,
                  try_low_res_src_insertion),
            Field(&ImageRewriteInfo::low_res_src_inserted_,
                  low_res_src_inserted),
            Field(&ImageRewriteInfo::low_res_image_type_, low_res_image_type),
            Field(&ImageRewriteInfo::low_res_data_size_, low_res_data_size)));
}

}  // namespace net_instaweb
