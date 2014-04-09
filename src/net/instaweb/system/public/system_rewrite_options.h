// Copyright 2013 Google Inc.
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

#ifndef NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_REWRITE_OPTIONS_H_
#define NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_REWRITE_OPTIONS_H_

#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class ThreadSystem;

// This manages configuration options specific to server implementations of
// pagespeed optimization libraries, such as mod_pagespeed and ngx_pagespeed.
class SystemRewriteOptions : public RewriteOptions {
 public:
  static void Initialize();
  static void Terminate();

  SystemRewriteOptions(const StringPiece& description,
                       ThreadSystem* thread_system);
  explicit SystemRewriteOptions(ThreadSystem* thread_system);
  virtual ~SystemRewriteOptions();

  int64 file_cache_clean_interval_ms() const {
    return file_cache_clean_interval_ms_.value();
  }
  bool has_file_cache_clean_interval_ms() const {
    return file_cache_clean_interval_ms_.was_set();
  }
  void set_file_cache_clean_interval_ms(int64 x) {
    set_option(x, &file_cache_clean_interval_ms_);
  }
  int64 file_cache_clean_size_kb() const {
    return file_cache_clean_size_kb_.value();
  }
  bool has_file_cache_clean_size_kb() const {
    return file_cache_clean_size_kb_.was_set();
  }
  void set_file_cache_clean_size_kb(int64 x) {
    set_option(x, &file_cache_clean_size_kb_);
  }
  int64 file_cache_clean_inode_limit() const {
    return file_cache_clean_inode_limit_.value();
  }
  bool has_file_cache_clean_inode_limit() const {
    return file_cache_clean_inode_limit_.was_set();
  }
  void set_file_cache_clean_inode_limit(int64 x) {
    set_option(x, &file_cache_clean_inode_limit_);
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
  bool use_shared_mem_locking() const {
    return use_shared_mem_locking_.value();
  }
  void set_use_shared_mem_locking(bool x) {
    set_option(x, &use_shared_mem_locking_);
  }
  bool compress_metadata_cache() const {
    return compress_metadata_cache_.value();
  }
  void set_compress_metadata_cache(bool x) {
    set_option(x, &compress_metadata_cache_);
  }
  bool statistics_enabled() const {
    return statistics_enabled_.value();
  }
  void set_statistics_enabled(bool x) {
    set_option(x, &statistics_enabled_);
  }
  bool statistics_logging_enabled() const {
    return statistics_logging_enabled_.value();
  }
  void set_statistics_logging_enabled(bool x) {
    set_option(x, &statistics_logging_enabled_);
  }
  int64 statistics_logging_max_file_size_kb() const {
    return statistics_logging_max_file_size_kb_.value();
  }
  const GoogleString& statistics_logging_charts_css() const {
    return statistics_logging_charts_css_.value();
  }
  const GoogleString& statistics_logging_charts_js() const {
    return statistics_logging_charts_js_.value();
  }
  int64 statistics_logging_interval_ms() const {
    return statistics_logging_interval_ms_.value();
  }
  void set_statistics_logging_interval_ms(int64 x) {
    set_option(x, &statistics_logging_interval_ms_);
  }
  const GoogleString& file_cache_path() const {
    return file_cache_path_.value();
  }
  void set_file_cache_path(const GoogleString& x) {
    set_option(x, &file_cache_path_);
  }
  const GoogleString& log_dir() const { return log_dir_.value(); }
  void set_log_dir(const GoogleString& x) { set_option(x, &log_dir_); }
  const GoogleString& memcached_servers() const {
    return memcached_servers_.value();
  }
  void set_memcached_servers(const GoogleString& x) {
    set_option(x, &memcached_servers_);
  }
  int memcached_threads() const {
    return memcached_threads_.value();
  }
  void set_memcached_threads(int x) {
    set_option(x, &memcached_threads_);
  }
  int memcached_timeout_us() const {
    return memcached_timeout_us_.value();
  }
  bool has_memcached_timeout_us() const {
    return memcached_timeout_us_.was_set();
  }
  void set_memcached_timeout_us(int x) {
    set_option(x, &memcached_timeout_us_);
  }
  const GoogleString& fetcher_proxy() const {
    return fetcher_proxy_.value();
  }
  void set_fetcher_proxy(const GoogleString& x) {
    set_option(x, &fetcher_proxy_);
  }

  // Cache flushing configuration.
  void set_cache_flush_poll_interval_sec(int64 num_seconds) {
    set_option(num_seconds, &cache_flush_poll_interval_sec_);
  }
  int64 cache_flush_poll_interval_sec() const {
    return cache_flush_poll_interval_sec_.value();
  }
  void set_cache_flush_filename(const StringPiece& sp) {
    set_option(sp.as_string(), &cache_flush_filename_);
  }
  const GoogleString& cache_flush_filename() const {
    return cache_flush_filename_.value();
  }

  const GoogleString& https_options() const {
    return https_options_.value();
  }
  const GoogleString& ssl_cert_directory() const {
    return ssl_cert_directory_.value();
  }
  const GoogleString& ssl_cert_file() const {
    return ssl_cert_file_.value();
  }

  int64 slurp_flush_limit() const {
    return slurp_flush_limit_.value();
  }
  void set_slurp_flush_limit(int64 x) {
    set_option(x, &slurp_flush_limit_);
  }
  bool slurp_read_only() const {
    return slurp_read_only_.value();
  }
  void set_slurp_read_only(bool x) {
    set_option(x, &slurp_read_only_);
  }
  bool rate_limit_background_fetches() const {
    return rate_limit_background_fetches_.value();
  }
  const GoogleString& slurp_directory() const {
    return slurp_directory_.value();
  }
  void set_slurp_directory(GoogleString x) {
    set_option(x, &slurp_directory_);
  }
  bool disable_loopback_routing() const {
    return disable_loopback_routing_.value();
  }
  bool fetch_with_gzip() const {
    return fetch_with_gzip_.value();
  }
  int64 ipro_max_response_bytes() const {
    return ipro_max_response_bytes_.value();
  }
  int64 ipro_max_concurrent_recordings() const {
    return ipro_max_concurrent_recordings_.value();
  }
  int64 default_shared_memory_cache_kb() const {
    return default_shared_memory_cache_kb_.value();
  }
  void set_default_shared_memory_cache_kb(int64 x) {
    set_option(x, &default_shared_memory_cache_kb_);
  }

  // If this is set to true, we'll turn on our fallback proxy-like behavior
  // on non-.pagespeed. URLs without changing the main fetcher from Serf
  // (the way the slurp options would).
  bool test_proxy() const {
    return test_proxy_.value();
  }
  void set_test_proxy(bool x) {
    set_option(x, &test_proxy_);
  }

  // This configures the fetcher we use for fallback handling if test_proxy()
  // is on:
  //  - If this is empty, we use the usual fetcher (e.g. Serf)
  //  - If it's non-empty, the fallback URLs will be fetched from the given
  //    slurp directory.  PageSpeed resource fetches, however, will still
  //    use the usual fetcher (e.g. Serf).
  GoogleString test_proxy_slurp() const {
    return test_proxy_slurp_.value();
  }

  // Helper functions
  bool slurping_enabled() const {
    return !slurp_directory().empty();
  }

  bool slurping_enabled_read_only() const {
    return slurping_enabled() && slurp_read_only();
  }

  virtual SystemRewriteOptions* Clone() const;
  virtual SystemRewriteOptions* NewOptions() const;

  // Returns a suitably down cast version of 'instance' if it is an instance
  // of this class, NULL if not.
  static const SystemRewriteOptions* DynamicCast(
      const RewriteOptions* instance);
  static SystemRewriteOptions* DynamicCast(RewriteOptions* instance);

  // Human-readable description of what this configuration is for.  This
  // may be a directory, or a string indicating a combination of directives
  // for multiple directories.  Should only be used for debugging.
  StringPiece description() const { return description_; }
  void set_description(const StringPiece& x) { x.CopyToString(&description_); }

 private:
  // We have some special parsing error-checking requirements for
  // FetchHttps
  class HttpsOptions : public Option<GoogleString> {
   public:
    virtual bool SetFromString(StringPiece value_string,
                               GoogleString* error_detail);
  };

  // Keeps the properties added by this subclass.  These are merged into
  // RewriteOptions::all_properties_ during Initialize().
  static Properties* system_properties_;

  // Adds an option to system_properties_.
  //
  template<class OptionClass>
  static void AddSystemProperty(typename OptionClass::ValueType default_value,
                                OptionClass SystemRewriteOptions::*offset,
                                const char* id,
                                StringPiece option_name,
                                const char* help) {
    AddProperty(default_value, offset, id, option_name, kServerScope, help,
                system_properties_);
  }

  template<class OptionClass>
  static void AddSystemProperty(typename OptionClass::ValueType default_value,
                                OptionClass SystemRewriteOptions::*offset,
                                const char* id,
                                StringPiece option_name,
                                OptionScope scope,
                                const char* help) {
    AddProperty(default_value, offset, id, option_name, scope, help,
                system_properties_);
  }

  static void AddProperties();

  // Debug string useful in understanding config merges.
  GoogleString description_;

  Option<GoogleString> fetcher_proxy_;
  Option<GoogleString> file_cache_path_;
  Option<GoogleString> log_dir_;

  // comma-separated list of host[:port].  See AprMemCache::AprMemCache
  // for code that parses it.
  Option<GoogleString> memcached_servers_;
  Option<GoogleString> statistics_logging_charts_css_;
  Option<GoogleString> statistics_logging_charts_js_;
  Option<GoogleString> cache_flush_filename_;
  Option<GoogleString> ssl_cert_directory_;
  Option<GoogleString> ssl_cert_file_;
  HttpsOptions https_options_;

  Option<GoogleString> slurp_directory_;
  Option<GoogleString> test_proxy_slurp_;

  Option<bool> statistics_enabled_;
  Option<bool> statistics_logging_enabled_;
  Option<bool> use_shared_mem_locking_;
  Option<bool> compress_metadata_cache_;

  Option<bool> slurp_read_only_;
  Option<bool> test_proxy_;
  Option<bool> rate_limit_background_fetches_;

  // If false (default) we will redirect all fetches to unknown hosts to
  // localhost.
  Option<bool> disable_loopback_routing_;

  // Makes fetches from PSA to origin-server request
  // accept-encoding:gzip, even when used in a context when we want
  // cleartext.  We'll decompress as we read the content if needed.
  Option<bool> fetch_with_gzip_;

  Option<int> memcached_threads_;
  Option<int> memcached_timeout_us_;

  Option<int64> file_cache_clean_inode_limit_;
  Option<int64> file_cache_clean_interval_ms_;
  Option<int64> file_cache_clean_size_kb_;
  Option<int64> lru_cache_byte_limit_;
  Option<int64> lru_cache_kb_per_process_;
  Option<int64> statistics_logging_interval_ms_;
  // If cache_flush_poll_interval_sec_<=0 then we turn off polling for
  // cache-flushes.
  Option<int64> cache_flush_poll_interval_sec_;
  Option<int64> statistics_logging_max_file_size_kb_;
  Option<int64> slurp_flush_limit_;
  Option<int64> ipro_max_response_bytes_;
  Option<int64> ipro_max_concurrent_recordings_;
  Option<int64> default_shared_memory_cache_kb_;

  DISALLOW_COPY_AND_ASSIGN(SystemRewriteOptions);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_REWRITE_OPTIONS_H_
