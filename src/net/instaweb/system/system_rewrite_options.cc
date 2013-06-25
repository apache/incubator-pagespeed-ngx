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
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class ThreadSystem;

namespace {

const int64 kDefaultCacheFlushIntervalSec = 5;

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
  AddSystemProperty(0, &SystemRewriteOptions::memcached_timeout_us_, "amo",
                    RewriteOptions::kMemcachedTimeoutUs,
                    "Maximum time in microseconds to allow for memcached "
                        "transactions");
  AddSystemProperty(true, &SystemRewriteOptions::statistics_enabled_, "ase",
                    RewriteOptions::kStatisticsEnabled,
                    "Whether to collect cross-process statistics.");
  AddSystemProperty("/pagespeed_statistics",
                    &SystemRewriteOptions::statistics_handler_path_, "ashp",
                    RewriteOptions::kStatisticsHandlerPath,
                    "Absolute path URL to statistics handler.");
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
  AddSystemProperty(1 * Timer::kMinuteMs,
                    &SystemRewriteOptions::statistics_logging_interval_ms_,
                    "asli", RewriteOptions::kStatisticsLoggingIntervalMs,
                    "How often to log statistics, in milliseconds.");
  AddSystemProperty(100 * 1024 /* 100 Megabytes */,
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
  AddSystemProperty("", &SystemRewriteOptions::ssl_cert_directory_, "assld",
                    RewriteOptions::kSslCertDirectory,
                    "Directory to find SSL certificates.");
  AddSystemProperty("", &SystemRewriteOptions::ssl_cert_file_, "asslf",
                    RewriteOptions::kSslCertFile,
                    "File with SSL certificates.");

  MergeSubclassProperties(system_properties_);
}

SystemRewriteOptions* SystemRewriteOptions::Clone() const {
  SystemRewriteOptions* options = NewOptions();
  options->Merge(*this);
  return options;
}

SystemRewriteOptions* SystemRewriteOptions::NewOptions() const {
  return new SystemRewriteOptions(thread_system());
}

}  // namespace net_instaweb
