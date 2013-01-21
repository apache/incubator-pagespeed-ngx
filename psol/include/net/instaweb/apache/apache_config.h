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
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class Hasher;

// Establishes a context for VirtualHosts and directory-scoped
// options, either via .htaccess or <Directory>...</Directory>.
class ApacheConfig : public RewriteOptions {
 public:
  enum RefererStatisticsOutputLevel {
    kFast,
    kSimple,
    kOrganized,
  };

  static const char kClassName[];

  static bool ParseRefererStatisticsOutputLevel(
      const StringPiece& in, RefererStatisticsOutputLevel* out);

  static void Initialize();
  static void Terminate();

  explicit ApacheConfig(const StringPiece& dir);
  ApacheConfig();
  ~ApacheConfig() {}

  // Human-readable description of what this configuration is for.  This
  // may be a directory, or a string indicating a combination of directives
  // for multiple directories.
  StringPiece description() const { return description_; }
  void set_description(const StringPiece& x) { x.CopyToString(&description_); }

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
  int64 slurp_flush_limit() const {
    return slurp_flush_limit_.value();
  }
  void set_slurp_flush_limit(int64 x) {
    set_option(x, &slurp_flush_limit_);
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
  bool statistics_logging_enabled() const {
    return statistics_logging_enabled_.value();
  }
  void set_statistics_logging_enabled(bool x) {
    set_option(x, &statistics_logging_enabled_);
  }
  const GoogleString& statistics_logging_file() const {
    return statistics_logging_file_.value();
  }
  const GoogleString& statistics_logging_charts_css() const {
    return statistics_logging_charts_css_.value();
  }
  const GoogleString& statistics_logging_charts_js() const {
    return statistics_logging_charts_js_.value();
  }
  void set_statistics_logging_file(GoogleString x) {
    set_option(x, &statistics_logging_file_);
  }
  int64 statistics_logging_interval_ms() const {
    return statistics_logging_interval_ms_.value();
  }
  void set_statistics_logging_interval_ms(int64 x) {
    set_option(x, &statistics_logging_interval_ms_);
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
  const GoogleString& memcached_servers() const {
    return memcached_servers_.value();
  }
  void set_memcached_servers(GoogleString x) {
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
  //  - If this is empty, we use the usual mod_pagespeed fetcher
  //    (e.g. Serf)
  //  - If it's non-empty, the fallback URLs will be fetched from the given
  //    slurp directory. mod_pagespeed resource fetches, however, will still
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

  bool experimental_fetch_from_mod_spdy() const {
    return experimental_fetch_from_mod_spdy_.value();
  }

  // Make an identical copy of these options and return it.
  virtual ApacheConfig* Clone() const;

  // Returns a suitably down cast version of 'instance' if it is an instance
  // of this class, NULL if not.
  static const ApacheConfig* DynamicCast(const RewriteOptions* instance);
  static ApacheConfig* DynamicCast(RewriteOptions* instance);

  // Name of the actual type of this instance as a poor man's RTTI.
  virtual const char* class_name() const;

 protected:
  template<class T> class ApacheOption : public OptionTemplateBase<T> {
   public:
    ApacheOption() {}

    // Sets value_ from value_string.
    virtual bool SetFromString(const GoogleString& value_string) {
      T value;
      bool success = ApacheConfig::ParseFromString(value_string, &value);
      if (success) {
        this->set(value);
      }
      return success;
    }

    virtual GoogleString Signature(const Hasher* hasher) const {
      return ApacheConfig::OptionSignature(this->value(), hasher);
    }

    virtual GoogleString ToString() const {
      return ApacheConfig::ToString(this->value());
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(ApacheOption);
  };

 private:
  // Keeps the properties added by this subclass.  These are merged into
  // RewriteOptions::all_properties_ during Initialize().
  static Properties* apache_properties_;

  // Adds an option to apache_properties_.
  //
  // TODO(jmarantz): rename this to avoid coinciding with private
  // method RewriteOptions::add_option.  This is done for now so
  // review-diffs are readable, at the cost of a small non-functional
  // follow-up refactor.
  template<class RewriteOptionsSubclass, class OptionClass>
  static void add_option(typename OptionClass::ValueType default_value,
                         OptionClass RewriteOptionsSubclass::*offset,
                         const char* id,
                         OptionEnum option_enum) {
    AddProperty(default_value, offset, id, option_enum, apache_properties_);
  }

  void InitializeSignaturesAndDefaults();
  static void AddProperties();
  void Init();

  static bool ParseFromString(const GoogleString& value_string,
                              RefererStatisticsOutputLevel* value) {
    return ParseRefererStatisticsOutputLevel(value_string, value);
  }

  static GoogleString OptionSignature(RefererStatisticsOutputLevel x,
                                      const Hasher* hasher) {
    // TODO(sriharis):  This is what we had so far due to implicit cast to int.
    // Do we need something better now?
    return IntegerToString(x);
  }

  static GoogleString ToString(RefererStatisticsOutputLevel x) {
    // TODO(sriharis):  This is what we had so far due to implicit cast to int.
    // Do we need something better now?
    return IntegerToString(x);
  }

  GoogleString description_;
  RewriteOptions options_;

  Option<GoogleString> fetcher_proxy_;
  Option<GoogleString> file_cache_path_;

  // comma-separated list of host[:port].  See AprMemCache::AprMemCache
  // for code that parses it.
  Option<GoogleString> fetch_https_;
  Option<GoogleString> memcached_servers_;
  Option<GoogleString> slurp_directory_;
  Option<GoogleString> statistics_logging_file_;
  Option<GoogleString> statistics_logging_charts_css_;
  Option<GoogleString> statistics_logging_charts_js_;
  Option<GoogleString> cache_flush_filename_;
  Option<GoogleString> test_proxy_slurp_;

  ApacheOption<RefererStatisticsOutputLevel> referer_statistics_output_level_;

  Option<bool> collect_referer_statistics_;
  Option<bool> hash_referer_statistics_;
  Option<bool> slurp_read_only_;
  Option<bool> statistics_enabled_;
  Option<bool> statistics_logging_enabled_;
  Option<bool> test_proxy_;
  Option<bool> use_shared_mem_locking_;
  Option<bool> rate_limit_background_fetches_;
  Option<bool> experimental_fetch_from_mod_spdy_;

  Option<int> memcached_threads_;
  Option<int> memcached_timeout_us_;

  Option<int64> file_cache_clean_inode_limit_;
  Option<int64> file_cache_clean_interval_ms_;
  Option<int64> file_cache_clean_size_kb_;
  Option<int64> lru_cache_byte_limit_;
  Option<int64> lru_cache_kb_per_process_;
  Option<int64> slurp_flush_limit_;
  Option<int64> statistics_logging_interval_ms_;
  // If cache_flush_poll_interval_sec_<=0 then we turn off polling for
  // cache-flushes.
  Option<int64> cache_flush_poll_interval_sec_;

  DISALLOW_COPY_AND_ASSIGN(ApacheConfig);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_CONFIG_H_
