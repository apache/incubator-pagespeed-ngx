// Copyright 2010 Google Inc.
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
// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/apache/apache_config.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

ApacheConfig::ApacheConfig(const StringPiece& description)
    : description_(description.data(), description.size()) {
  add_option("", &fetcher_proxy_);
  add_option("", &file_cache_path_);
  add_option("", &filename_prefix_);

  add_option(kOrganized, &referer_statistics_output_level_);

  add_option(false, &collect_referer_statistics_);
  add_option(false, &hash_referer_statistics_);
  add_option(true, &statistics_enabled_);
  add_option(false, &test_proxy_);
  add_option(false, &use_shared_mem_locking_);

  add_option(5 * Timer::kSecondMs, &fetcher_time_out_ms_);
  add_option(Timer::kHourMs, &file_cache_clean_interval_ms_);

  add_option(100 * 1024, &file_cache_clean_size_kb_);  // 100 megabytes
  add_option(0, &lru_cache_byte_limit_);
  add_option(0, &lru_cache_kb_per_process_);
  add_option(0, &slurp_flush_limit_);

  add_option(100 * 1024, &message_buffer_size_);  // 100 kbytes
}

bool ApacheConfig::ParseRefererStatisticsOutputLevel(
    const StringPiece& in, RefererStatisticsOutputLevel* out) {
  bool ret = false;
  if (in != NULL) {
    if (StringCaseEqual(in, "Fast")) {
      *out = kFast;
       ret = true;
    } else if (StringCaseEqual(in, "Simple")) {
      *out = kSimple;
       ret = true;
    } else if (StringCaseEqual(in, "Organized")) {
      *out = kOrganized;
       ret = true;
    }
  }
  return ret;
}

}  // namespace net_instaweb
