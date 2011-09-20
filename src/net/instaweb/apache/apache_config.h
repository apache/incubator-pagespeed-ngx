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

#ifndef NET_INSTAWEB_APACHE_APACHE_CONFIG_H_
#define NET_INSTAWEB_APACHE_APACHE_CONFIG_H_

#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

// The httpd header must be after the
// apache_rewrite_driver_factory.h. Otherwise, the compiler will
// complain "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "httpd.h"

namespace net_instaweb {

// Establishes a context for VirtualHosts and directory-scoped
// options, either via .htaccess or <Directory>...</Directory>.
class ApacheConfig : public RewriteOptions {
 public:
  enum RefererStatisticsOutputLevel {
    kFast,
    kSimple,
    kOrganized,
  };

  static bool ParseRefererStatisticsOutputLevel(
      const StringPiece& in, RefererStatisticsOutputLevel* out);

  explicit ApacheConfig(const StringPiece& dir);
  ~ApacheConfig() {}

  // Human-readable description of what this configuration is for.  This
  // may be a directory, or a string indicating a combination of directives
  // for multiple directories.
  StringPiece description() const { return description_; }

  int64 file_cache_clean_interval_ms() const {
    return file_cache_clean_interval_ms_.value();
  }
  void set_file_cache_clean_interval_ms(int64 x) {
    set_option(x, &file_cache_clean_interval_ms_);
  }
  int64 file_cache_clean_size_kb() const {
    return file_cache_clean_size_kb_.value();
  }
  void set_file_cache_clean_size_kb(int64 x) {
    set_option(x, &file_cache_clean_size_kb_);
  }
  int64 lru_cache_byte_limit() const {
    return lru_cache_byte_limit_.value();
  }
  void set_lru_cache_byte_limit(int64 x) {
    set_option(x, &lru_cache_byte_limit_);
  }
  int64 lru_cache_kb_per_process() const {
    return lru_cache_kb_per_process_.value();
  }
  void set_lru_cache_kb_per_process(int64 x) {
    set_option(x, &lru_cache_kb_per_process_);
  }
  int64 fetcher_time_out_ms() const {
    return fetcher_time_out_ms_.value();
  }
  void set_fetcher_time_out_ms(int64 x) {
    set_option(x, &fetcher_time_out_ms_);
  }
  int64 slurp_flush_limit() const {
    return slurp_flush_limit_.value();
  }
  void set_slurp_flush_limit(int64 x) {
    set_option(x, &slurp_flush_limit_);
  }
  int message_buffer_size() const {
    return message_buffer_size_.value();
  }
  void set_message_buffer_size(int x) {
    set_option(x, &message_buffer_size_);
  }
  bool use_shared_mem_locking() const {
    return use_shared_mem_locking_.value();
  }
  void set_use_shared_mem_locking(bool x) {
    set_option(x, &use_shared_mem_locking_);
  }
  bool collect_referer_statistics() const {
    return collect_referer_statistics_.value();
  }
  void set_collect_referer_statistics(bool x) {
    set_option(x, &collect_referer_statistics_);
  }
  bool hash_referer_statistics() const {
    return hash_referer_statistics_.value();
  }
  void set_hash_referer_statistics(bool x) {
    set_option(x, &hash_referer_statistics_);
  }
  bool statistics_enabled() const {
    return statistics_enabled_.value();
  }
  void set_statistics_enabled(bool x) {
    set_option(x, &statistics_enabled_);
  }
  bool slurp_read_only() const {
    return slurp_read_only_.value();
  }
  void set_slurp_read_only(bool x) {
    set_option(x, &slurp_read_only_);
  }
  RefererStatisticsOutputLevel referer_statistics_output_level() const {
    return referer_statistics_output_level_.value();
  }
  void set_referer_statistics_output_level(RefererStatisticsOutputLevel x) {
    set_option(x, &referer_statistics_output_level_);
  }
  const GoogleString& file_cache_path() const {
    return file_cache_path_.value();
  }
  void set_file_cache_path(GoogleString x) {
    set_option(x, &file_cache_path_);
  }
  const GoogleString& filename_prefix() const {
    return filename_prefix_.value();
  }
  void set_filename_prefix(GoogleString x) {
    set_option(x, &filename_prefix_);
  }
  const GoogleString& slurp_directory() const {
    return slurp_directory_.value();
  }
  void set_slurp_directory(GoogleString x) {
    set_option(x, &slurp_directory_);
  }
  const GoogleString& fetcher_proxy() const {
    return fetcher_proxy_.value();
  }
  void set_fetcher_proxy(GoogleString x) {
    set_option(x, &fetcher_proxy_);
  }

  // Controls whether we act as a rewriting proxy, fetching
  // URLs from origin without managing a slurp dump.
  bool test_proxy() const {
    return test_proxy_.value();
  }
  void set_test_proxy(bool x) {
    set_option(x, &test_proxy_);
  }

  // Helper functions
  bool slurping_enabled() const {
    return !slurp_directory().empty();
  }

  bool slurping_enabled_read_only() const {
    return slurping_enabled() && slurp_read_only();
  }

 private:
  GoogleString description_;
  RewriteOptions options_;

  Option<GoogleString> fetcher_proxy_;
  Option<GoogleString> file_cache_path_;
  Option<GoogleString> filename_prefix_;
  Option<GoogleString> slurp_directory_;

  Option<RefererStatisticsOutputLevel> referer_statistics_output_level_;

  Option<bool> collect_referer_statistics_;
  Option<bool> hash_referer_statistics_;
  Option<bool> statistics_enabled_;
  Option<bool> test_proxy_;
  Option<bool> use_shared_mem_locking_;
  Option<bool> slurp_read_only_;

  Option<int64> fetcher_time_out_ms_;
  Option<int64> file_cache_clean_interval_ms_;
  Option<int64> file_cache_clean_size_kb_;
  Option<int64> lru_cache_byte_limit_;
  Option<int64> lru_cache_kb_per_process_;
  Option<int64> slurp_flush_limit_;

  Option<int> message_buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(ApacheConfig);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_CONFIG_H_
