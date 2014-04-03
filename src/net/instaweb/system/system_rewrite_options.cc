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

#include "net/instaweb/system/public/system_rewrite_options.h"

#include "base/logging.h"
#include "net/instaweb/system/public/serf_url_async_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class ThreadSystem;

namespace {

const int64 kDefaultCacheFlushIntervalSec = 5;

const char kFetchHttps[] = "FetchHttps";

}  // namespace

RewriteOptions::Properties* SystemRewriteOptions::system_properties_ = NULL;

void SystemRewriteOptions::Initialize() {
  if (Properties::Initialize(&system_properties_)) {
    RewriteOptions::Initialize();
    AddProperties();
  }
}

void SystemRewriteOptions::Terminate() {
  if (Properties::Terminate(&system_properties_)) {
    RewriteOptions::Terminate();
  }
}

SystemRewriteOptions::SystemRewriteOptions(ThreadSystem* thread_system)
    : RewriteOptions(thread_system) {
  DCHECK(system_properties_ != NULL)
      << "Call SystemRewriteOptions::Initialize() before construction";
  InitializeOptions(system_properties_);
}

SystemRewriteOptions::SystemRewriteOptions(const StringPiece& description,
                                           ThreadSystem* thread_system)
    : RewriteOptions(thread_system),
      description_(description.data(), description.size()) {
  DCHECK(system_properties_ != NULL)
      << "Call SystemRewriteOptions::Initialize() before construction";
  InitializeOptions(system_properties_);
}

SystemRewriteOptions::~SystemRewriteOptions() {
}

void SystemRewriteOptions::AddProperties() {
  AddSystemProperty("", &SystemRewriteOptions::fetcher_proxy_, "afp",
                    RewriteOptions::kFetcherProxy,
                    "Set the fetch proxy");
  AddSystemProperty("", &SystemRewriteOptions::file_cache_path_, "afcp",
                    RewriteOptions::kFileCachePath,
                    "Set the path for file cache");
  AddSystemProperty("", &SystemRewriteOptions::log_dir_, "ald",
                    RewriteOptions::kLogDir,
                    "Directory to store logs in.");
  AddSystemProperty("", &SystemRewriteOptions::memcached_servers_, "ams",
                    RewriteOptions::kMemcachedServers,
                    "Comma-separated list of servers e.g. "
                        "host1:port1,host2:port2");
  AddSystemProperty(1, &SystemRewriteOptions::memcached_threads_, "amt",
                    RewriteOptions::kMemcachedThreads,
                    "Number of background threads to use to run "
                        "memcached fetches");
  AddSystemProperty(500 * Timer::kMsUs,  // half a second
                    &SystemRewriteOptions::memcached_timeout_us_, "amo",
                    RewriteOptions::kMemcachedTimeoutUs,
                    "Maximum time in microseconds to allow for memcached "
                        "transactions");
  AddSystemProperty(true, &SystemRewriteOptions::statistics_enabled_, "ase",
                    RewriteOptions::kStatisticsEnabled,
                    "Whether to collect cross-process statistics.");
  AddSystemProperty("", &SystemRewriteOptions::statistics_logging_charts_css_,
                    "aslcc", RewriteOptions::kStatisticsLoggingChartsCSS,
                    "Where to find an offline copy of the Google Charts Tools "
                        "API CSS.");
  AddSystemProperty("", &SystemRewriteOptions::statistics_logging_charts_js_,
                    "aslcj", RewriteOptions::kStatisticsLoggingChartsJS,
                    "Where to find an offline copy of the Google Charts Tools "
                        "API JS.");
  AddSystemProperty(false, &SystemRewriteOptions::statistics_logging_enabled_,
                    "asle", RewriteOptions::kStatisticsLoggingEnabled,
                    "Whether to log statistics if they're being collected.");
  AddSystemProperty(10 * Timer::kMinuteMs,
                    &SystemRewriteOptions::statistics_logging_interval_ms_,
                    "asli", RewriteOptions::kStatisticsLoggingIntervalMs,
                    "How often to log statistics, in milliseconds.");
  // 2 Weeks of data w/ 10 minute intervals.
  // Takes about 0.1s to parse 1MB file for modpagespeed.com/pagespeed_console
  // TODO(sligocki): Increase once we have a better method for reading
  // historical data.
  AddSystemProperty(1 * 1024 /* 1 Megabytes */,
                    &SystemRewriteOptions::statistics_logging_max_file_size_kb_,
                    "aslfs", RewriteOptions::kStatisticsLoggingMaxFileSizeKb,
                    "Max size for statistics logging file.");
  AddSystemProperty(true, &SystemRewriteOptions::use_shared_mem_locking_,
                    "ausml", RewriteOptions::kUseSharedMemLocking,
                    "Use shared memory for internal named lock service");
  AddSystemProperty(Timer::kHourMs,
                    &SystemRewriteOptions::file_cache_clean_interval_ms_,
                    "afcci", RewriteOptions::kFileCacheCleanIntervalMs,
                    "Set the interval (in ms) for cleaning the file cache");
  AddSystemProperty(100 * 1024 /* 100 megabytes */,
                    &SystemRewriteOptions::file_cache_clean_size_kb_,
                    "afc", RewriteOptions::kFileCacheCleanSizeKb,
                    "Set the target size (in kilobytes) for file cache");
  // Default to no inode limit so that existing installations are not affected.
  // pagespeed.conf.template contains suggested limit for new installations.
  // TODO(morlovich): Inject this as an argument, since we want a different
  // default for ngx_pagespeed?
  AddSystemProperty(0, &SystemRewriteOptions::file_cache_clean_inode_limit_,
                    "afcl", RewriteOptions::kFileCacheCleanInodeLimit,
                    "Set the target number of inodes for the file cache; 0 "
                        "means no limit");
  AddSystemProperty(0, &SystemRewriteOptions::lru_cache_byte_limit_, "alcb",
                    RewriteOptions::kLruCacheByteLimit,
                    "Set the maximum byte size entry to store in the "
                        "per-process in-memory LRU cache");
  AddSystemProperty(0, &SystemRewriteOptions::lru_cache_kb_per_process_, "alcp",
                    RewriteOptions::kLruCacheKbPerProcess,
                    "Set the total size, in KB, of the per-process in-memory "
                        "LRU cache");
  AddSystemProperty("", &SystemRewriteOptions::cache_flush_filename_, "acff",
                    RewriteOptions::kCacheFlushFilename,
                    "Name of file to check for timestamp updates used to flush "
                        "cache. This file will be relative to the "
                        "ModPagespeedFileCachePath if it does not begin with a "
                        "slash.");
  AddSystemProperty(kDefaultCacheFlushIntervalSec,
                    &SystemRewriteOptions::cache_flush_poll_interval_sec_,
                    "acfpi", RewriteOptions::kCacheFlushPollIntervalSec,
                    "Number of seconds to wait between polling for cache-flush "
                        "requests");
  AddSystemProperty(false,
                    &SystemRewriteOptions::compress_metadata_cache_,
                    "cc", RewriteOptions::kCompressMetadataCache,
                    "Whether to compress cache entries before writing them to "
                    "memory or disk.");
  AddSystemProperty("disable", &SystemRewriteOptions::https_options_, "fhs",
                    kFetchHttps, "Controls direct fetching of HTTPS resources."
                    "  Value is comma-separated list of keywords: "
                    SERF_HTTPS_KEYWORDS);
  AddSystemProperty("", &SystemRewriteOptions::ssl_cert_directory_, "assld",
                    RewriteOptions::kSslCertDirectory,
                    "Directory to find SSL certificates.");
  AddSystemProperty("", &SystemRewriteOptions::ssl_cert_file_, "asslf",
                    RewriteOptions::kSslCertFile,
                    "File with SSL certificates.");
  AddSystemProperty("", &SystemRewriteOptions::slurp_directory_, "asd",
                    RewriteOptions::kSlurpDirectory,
                    "Directory from which to read slurped resources");
  AddSystemProperty(false, &SystemRewriteOptions::test_proxy_, "atp",
                    RewriteOptions::kTestProxy,
                    "Direct non-PageSpeed URLs to a fetcher, acting as a "
                    "simple proxy. Meant for test use only");
  AddSystemProperty("", &SystemRewriteOptions::test_proxy_slurp_, "atps",
                    RewriteOptions::kTestProxySlurp,
                    "If set, the fetcher used by the TestProxy mode will be a "
                    "readonly slurp fetcher from the given directory");
  AddSystemProperty(false, &SystemRewriteOptions::slurp_read_only_, "asro",
                    RewriteOptions::kSlurpReadOnly,
                    "Only read from the slurped directory, fail to fetch "
                    "URLs not already in the slurped directory");
  AddSystemProperty(true,
                    &SystemRewriteOptions::rate_limit_background_fetches_,
                    "rlbf",
                    RewriteOptions::kRateLimitBackgroundFetches,
                    "Rate-limit the number of background HTTP fetches done at "
                    "once");
  AddSystemProperty(0, &SystemRewriteOptions::slurp_flush_limit_, "asfl",
                    RewriteOptions::kSlurpFlushLimit,
                    "Set the maximum byte size for the slurped content to hold "
                    "before a flush");
  AddSystemProperty(false, &SystemRewriteOptions::disable_loopback_routing_,
                    "adlr",
                    "DangerPermitFetchFromUnknownHosts",
                    kProcessScopeStrict,
                    "Disable security checks that prohibit fetching from "
                    "hostnames mod_pagespeed does not know about");
  AddSystemProperty(false, &SystemRewriteOptions::fetch_with_gzip_,
                    "afg", "FetchWithGzip", kProcessScope,
                    "Request http content from origin servers using gzip");
  AddSystemProperty(1024 * 1024 * 10,  /* 10 Megabytes */
                    &SystemRewriteOptions::ipro_max_response_bytes_,
                    "imrb", "IproMaxResponseBytes", kProcessScope,
                    "Limit allowed size of IPRO responses. "
                    "Set to 0 for unlimited.");
  AddSystemProperty(10,
                    &SystemRewriteOptions::ipro_max_concurrent_recordings_,
                    "imcr", "IproMaxConcurrentRecordings", kProcessScope,
                    "Limit allowed number of IPRO recordings");
  AddSystemProperty(1024 * 50,  /* 50 Megabytes */
                    &SystemRewriteOptions::default_shared_memory_cache_kb_,
                    "dsmc", "DefaultSharedMemoryCacheKB", kProcessScope,
                    "Size of the default shared memory cache used by all "
                    "virtual hosts that don't use "
                    "CreateSharedMemoryMetadataCache. "
                    "Set to 0 to turn off the default shared memory cache.");

  MergeSubclassProperties(system_properties_);

  // We allow a special instantiation of the options with a null thread system
  // because we are only updating the static properties on process startup; we
  // won't have a thread-system yet or multiple threads.
  //
  // Leave slurp_read_only out of the signature as (a) we don't actually change
  // this spontaneously, and (b) it's useful to keep the metadata cache between
  // slurping read-only and slurp read/write.
  SystemRewriteOptions config("dummy_options", NULL);
  config.slurp_read_only_.DoNotUseForSignatureComputation();
}

SystemRewriteOptions* SystemRewriteOptions::Clone() const {
  SystemRewriteOptions* options = NewOptions();
  options->Merge(*this);
  return options;
}

SystemRewriteOptions* SystemRewriteOptions::NewOptions() const {
  return new SystemRewriteOptions("new_options", thread_system());
}

const SystemRewriteOptions* SystemRewriteOptions::DynamicCast(
    const RewriteOptions* instance) {
  const SystemRewriteOptions* config =
      dynamic_cast<const SystemRewriteOptions*>(instance);
  DCHECK(config != NULL);
  return config;
}

SystemRewriteOptions* SystemRewriteOptions::DynamicCast(
    RewriteOptions* instance) {
  SystemRewriteOptions* config = dynamic_cast<SystemRewriteOptions*>(instance);
  DCHECK(config != NULL);
  return config;
}

bool SystemRewriteOptions::HttpsOptions::SetFromString(
    StringPiece value, GoogleString* error_detail) {
  bool success = SerfUrlAsyncFetcher::ValidateHttpsOptions(value, error_detail);
  if (success) {
    set(value.as_string());
  }
  return success;
}

}  // namespace net_instaweb
