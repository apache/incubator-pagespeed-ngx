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

#ifndef PAGESPEED_SYSTEM_SYSTEM_REWRITE_OPTIONS_H_
#define PAGESPEED_SYSTEM_SYSTEM_REWRITE_OPTIONS_H_

#include <set>

#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/static_asset_config.pb.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/fast_wildcard_group.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/util/copy_on_write.h"
#include "pagespeed/system/external_server_spec.h"

namespace net_instaweb {

class MessageHandler;

// This manages configuration options specific to server implementations of
// pagespeed optimization libraries, such as mod_pagespeed and ngx_pagespeed.
class SystemRewriteOptions : public RewriteOptions {
 public:
  typedef std::set<StaticAssetEnum::StaticAsset> StaticAssetSet;

  static const char kCentralControllerPort[];
  static const char kPopularityContestMaxInFlight[];
  static const char kPopularityContestMaxQueueSize[];
  static const char kStaticAssetCDN[];
  static const char kRedisServer[];
  static const char kRedisReconnectionDelayMs[];
  static const char kRedisTimeoutUs[];

  static constexpr int kMemcachedDefaultPort = 11211;
  static constexpr int kRedisDefaultPort = 6379;

  static void Initialize();
  static void Terminate();

  SystemRewriteOptions(const StringPiece& description,
                       ThreadSystem* thread_system);
  explicit SystemRewriteOptions(ThreadSystem* thread_system);
  virtual ~SystemRewriteOptions();

  virtual void Merge(const RewriteOptions& src);

  virtual OptionSettingResult ParseAndSetOptionFromName2(
      StringPiece name, StringPiece arg1, StringPiece arg2,
      GoogleString* msg, MessageHandler* handler);

  virtual GoogleString SubclassSignatureLockHeld();

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
  const ExternalClusterSpec& memcached_servers() const {
    return memcached_servers_.value();
  }
  void set_memcached_servers(const ExternalClusterSpec& x) {
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
  const ExternalServerSpec& redis_server() const {
    return redis_server_.value();
  }
  void set_redis_server(const ExternalServerSpec& x) {
    set_option(x, &redis_server_);
  }
  int64 redis_reconnection_delay_ms() const {
    return redis_reconnection_delay_ms_.value();
  }
  int64 redis_timeout_us() const {
    return redis_timeout_us_.value();
  }
  int64 slow_file_latency_threshold_us() const {
    return slow_file_latency_threshold_us_.value();
  }
  bool has_slow_file_latency_threshold_us() const {
    return slow_file_latency_threshold_us_.was_set();
  }
  void set_slow_file_latency_threshold_us(int64 x) {
    set_option(x, &slow_file_latency_threshold_us_);
  }
  const GoogleString& fetcher_proxy() const {
    return fetcher_proxy_.value();
  }
  void set_fetcher_proxy(const GoogleString& x) {
    set_option(x, &fetcher_proxy_);
  }

  const GoogleString& controller_port() const {
    return controller_port_.value();
  }
  int popularity_contest_max_inflight_requests() const {
    return popularity_contest_max_inflight_requests_.value();
  }
  int popularity_contest_max_queue_size() const {
    return popularity_contest_max_queue_size_.value();
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
  int shm_metadata_cache_checkpoint_interval_sec() const {
    return shm_metadata_cache_checkpoint_interval_sec_.value();
  }
  void set_purge_method(const GoogleString& x) {
    set_option(x, &purge_method_);
  }
  const GoogleString& purge_method() const { return purge_method_.value(); }

  bool AllowDomain(const GoogleUrl& url,
                   const FastWildcardGroup& wildcard_group) const;

  // For each of these AccessAllowed() methods, the url needs to have been
  // checked to make sure it is_valid().
  bool StatisticsAccessAllowed(const GoogleUrl& url) const {
    return AllowDomain(url, *statistics_domains_);
  }
  bool GlobalStatisticsAccessAllowed(const GoogleUrl& url) const {
    return AllowDomain(url, *global_statistics_domains_);
  }
  bool MessagesAccessAllowed(const GoogleUrl& url) const {
    return AllowDomain(url, *messages_domains_);
  }
  bool ConsoleAccessAllowed(const GoogleUrl& url) const {
    return AllowDomain(url, *console_domains_);
  }
  bool AdminAccessAllowed(const GoogleUrl& url) const {
    return AllowDomain(url, *admin_domains_);
  }
  bool GlobalAdminAccessAllowed(const GoogleUrl& url) const {
    return AllowDomain(url, *global_admin_domains_);
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

  // Returns true if we were asked to configure StaticAssetManager to
  // serve static assets that are usually compiled in from an external
  // base URL.
  bool has_static_assets_to_cdn() const {
    return static_assets_to_cdn_.was_set();
  }

  // Particular assets to serve of an external URL.
  const StaticAssetSet& static_assets_to_cdn() const {
    return static_assets_to_cdn_.asset_set();
  }

  // Base URL to serve them from.
  const GoogleString& static_assets_cdn_base() const {
    return static_assets_to_cdn_.value();
  }

  // Fills in an options proto based on the CDN settings passed above.
  void FillInStaticAssetCDNConf(StaticAssetConfig* out_conf) const;

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

  class StaticAssetCDNOptions : public OptionTemplateBase<GoogleString> {
   public:
    virtual bool SetFromString(StringPiece value_string,
                               GoogleString* error_detail);

    virtual GoogleString Signature(const Hasher* hasher) const;
    virtual GoogleString ToString() const;
    virtual void Merge(const OptionBase* src);

    // value() here is just the base path.
    const StaticAssetSet& asset_set() const {
      return *static_assets_to_cdn_.get();
    }

   private:
    // The string is the base path,
    CopyOnWrite<StaticAssetSet> static_assets_to_cdn_;
  };

  template<typename Spec, int default_port>
  class ExternalServersOption : public OptionTemplateBase<Spec> {
   public:
    bool SetFromString(StringPiece value_string,
                       GoogleString* error_detail) override {
      return this->mutable_value().SetFromString(value_string, default_port,
                                                 error_detail);
    }
    GoogleString ToString() const override {
      return this->value().ToString();
    }
    GoogleString Signature(const Hasher* hasher) const override {
      return hasher->Hash(ToString());
    }
  };

  class ControllerPortOption : public Option<GoogleString> {
   public:
    bool SetFromString(StringPiece value_string,
                       GoogleString* error_detail) override;
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
                                const char* help,
                                bool safe_to_print) {
    AddProperty(default_value, offset, id, option_name, kServerScope, help,
                safe_to_print, system_properties_);
  }

  template<class OptionClass>
  static void AddSystemProperty(typename OptionClass::ValueType default_value,
                                OptionClass SystemRewriteOptions::*offset,
                                const char* id,
                                StringPiece option_name,
                                OptionScope scope,
                                const char* help,
                                bool safe_to_print) {
    AddProperty(default_value, offset, id, option_name, scope, help,
                safe_to_print, system_properties_);
  }

  static void AddProperties();

  // Debug string useful in understanding config merges.
  GoogleString description_;

  Option<GoogleString> fetcher_proxy_;
  Option<GoogleString> file_cache_path_;
  Option<GoogleString> log_dir_;

  ExternalServersOption<ExternalClusterSpec, kMemcachedDefaultPort>
      memcached_servers_;
  ExternalServersOption<ExternalServerSpec, kRedisDefaultPort> redis_server_;
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

  ControllerPortOption controller_port_;
  Option<int> popularity_contest_max_inflight_requests_;
  Option<int> popularity_contest_max_queue_size_;

  Option<int> memcached_threads_;
  Option<int> memcached_timeout_us_;
  Option<int64> redis_reconnection_delay_ms_;
  Option<int64> redis_timeout_us_;

  Option<int64> slow_file_latency_threshold_us_;
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
  Option<int> shm_metadata_cache_checkpoint_interval_sec_;
  Option<GoogleString> purge_method_;

  StaticAssetCDNOptions static_assets_to_cdn_;

  CopyOnWrite<FastWildcardGroup> statistics_domains_;
  CopyOnWrite<FastWildcardGroup> global_statistics_domains_;
  CopyOnWrite<FastWildcardGroup> messages_domains_;
  CopyOnWrite<FastWildcardGroup> console_domains_;
  CopyOnWrite<FastWildcardGroup> admin_domains_;
  CopyOnWrite<FastWildcardGroup> global_admin_domains_;

  DISALLOW_COPY_AND_ASSIGN(SystemRewriteOptions);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_SYSTEM_REWRITE_OPTIONS_H_
