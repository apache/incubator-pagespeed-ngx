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

  explicit SystemRewriteOptions(ThreadSystem* thread_system);
  virtual ~SystemRewriteOptions();

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
  int64 file_cache_clean_inode_limit() const {
    return file_cache_clean_inode_limit_.value();
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
  const GoogleString& statistics_handler_path() const {
    return statistics_handler_path_.value();
  }
  void set_statistics_handler_path(const GoogleString& x) {
    set_option(x, &statistics_handler_path_);
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

  const GoogleString& ssl_cert_directory() const {
    return ssl_cert_directory_.value();
  }
  const GoogleString& ssl_cert_file() const {
    return ssl_cert_file_.value();
  }

  virtual SystemRewriteOptions* Clone() const;
  virtual SystemRewriteOptions* NewOptions() const;

 protected:
  // Apache and Nginx options classes need access to this.
  Option<GoogleString> statistics_handler_path_;

 private:
  // Keeps the properties added by this subclass.  These are merged into
  // RewriteOptions::all_properties_ during Initialize().
  static Properties* system_properties_;

  // Adds an option to system_properties_.
  //
  template<class RewriteOptionsSubclass, class OptionClass>
  static void AddSystemProperty(typename OptionClass::ValueType default_value,
                                OptionClass RewriteOptionsSubclass::*offset,
                                const char* id,
                                OptionEnum option_enum,
                                const char* help) {
    AddProperty(default_value, offset, id, option_enum, kServerScope, help,
                system_properties_);
  }

  static void AddProperties();

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

  Option<bool> statistics_enabled_;
  Option<bool> statistics_logging_enabled_;
  Option<bool> use_shared_mem_locking_;
  Option<bool> compress_metadata_cache_;

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

  DISALLOW_COPY_AND_ASSIGN(SystemRewriteOptions);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_REWRITE_OPTIONS_H_
