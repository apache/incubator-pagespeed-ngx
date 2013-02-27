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

SystemRewriteOptions::SystemRewriteOptions() {
  DCHECK(system_properties_ != NULL)
      << "Call SystemRewriteOptions::Initialize() before construction";
  InitializeOptions(system_properties_);
}

SystemRewriteOptions::~SystemRewriteOptions() {
}

void SystemRewriteOptions::AddProperties() {
  add_option("", &SystemRewriteOptions::fetcher_proxy_, "afp",
             RewriteOptions::kFetcherProxy,
             "Set the fetch proxy");
  add_option("", &SystemRewriteOptions::file_cache_path_, "afcp",
             RewriteOptions::kFileCachePath,
             "Set the path for file cache");
  add_option("", &SystemRewriteOptions::memcached_servers_, "ams",
             RewriteOptions::kMemcachedServers,
             "Comma-separated list of servers e.g. host1:port1,host2:port2");
  add_option(1, &SystemRewriteOptions::memcached_threads_, "amt",
             RewriteOptions::kMemcachedThreads,
             "Number of background threads to use to run memcached fetches");
  add_option(0, &SystemRewriteOptions::memcached_timeout_us_, "amo",
             RewriteOptions::kMemcachedTimeoutUs,
             "Maximum time in microseconds to allow for memcached "
             "transactions");
  add_option("", &SystemRewriteOptions::statistics_logging_file_, "aslf",
             RewriteOptions::kStatisticsLoggingFile,
             "Where to log cross-process statistics if they're being "
             "collected."),
  add_option("", &SystemRewriteOptions::statistics_logging_charts_css_, "aslcc",
             RewriteOptions::kStatisticsLoggingChartsCSS,
             "Where to find an offline copy of the Google Charts Tools API "
             "CSS.");
  add_option("", &SystemRewriteOptions::statistics_logging_charts_js_, "aslcj",
             RewriteOptions::kStatisticsLoggingChartsJS,
             "Where to find an offline copy of the Google Charts Tools API "
             "JS.");
  add_option(true, &SystemRewriteOptions::statistics_enabled_, "ase",
             RewriteOptions::kStatisticsEnabled,
             "Whether to collect cross-process statistics.");
  add_option(false, &SystemRewriteOptions::statistics_logging_enabled_, "asle",
             RewriteOptions::kStatisticsLoggingEnabled,
             "Whether to log cross-process statistics if they're being "
             "collected.");
  add_option(true, &SystemRewriteOptions::use_shared_mem_locking_, "ausml",
             RewriteOptions::kUseSharedMemLocking,
             "Use shared memory for internal named lock service");
  add_option(Timer::kHourMs,
             &SystemRewriteOptions::file_cache_clean_interval_ms_,
             "afcci", RewriteOptions::kFileCacheCleanIntervalMs,
             "Set the interval (in ms) for cleaning the file cache");
  add_option(100 * 1024, &SystemRewriteOptions::file_cache_clean_size_kb_,
             "afc", RewriteOptions::kFileCacheCleanSizeKb,   // 100 megabytes
             "Set the target size (in kilobytes) for file cache");
  // Default to no inode limit so that existing installations are not affected.
  // pagespeed.conf.template contains suggested limit for new installations.
  // TODO(morlovich): Inject this as an argument, since we want a different
  // default for ngx_pagespeed?
  add_option(0, &SystemRewriteOptions::file_cache_clean_inode_limit_, "afcl",
             RewriteOptions::kFileCacheCleanInodeLimit,
             "Set the target number of inodes for the file cache; 0 "
             "means no limit");
  add_option(0, &SystemRewriteOptions::lru_cache_byte_limit_, "alcb",
             RewriteOptions::kLruCacheByteLimit,
             "Set the maximum byte size entry to store in the per-process "
             "in-memory LRU cache");
  add_option(0, &SystemRewriteOptions::lru_cache_kb_per_process_, "alcp",
             RewriteOptions::kLruCacheKbPerProcess,
             "Set the total size, in KB, of the per-process in-memory "
             "LRU cache");
  add_option(3000, &SystemRewriteOptions::statistics_logging_interval_ms_,
             "asli", RewriteOptions::kStatisticsLoggingIntervalMs,
             "How often to log cross-process statistics, in milliseconds.");
  add_option("", &SystemRewriteOptions::cache_flush_filename_, "acff",
             RewriteOptions::kCacheFlushFilename,
             "Name of file to check for timestamp updates used to flush "
             "cache. This file will be relative to the "
             "ModPagespeedFileCachePath if it does not begin with a slash.");
  add_option(kDefaultCacheFlushIntervalSec,
             &SystemRewriteOptions::cache_flush_poll_interval_sec_, "acfpi",
             RewriteOptions::kCacheFlushPollIntervalSec,
             "Number of seconds to wait between polling for cache-flush "
             "requests");
  add_option("", &SystemRewriteOptions::use_shared_mem_metadata_cache_,
             "asmc", RewriteOptions::kUseSharedMemMetadataCache,
             "Use given shared memory cache for metadata cache");

  MergeSubclassProperties(system_properties_);
}

SystemRewriteOptions* SystemRewriteOptions::Clone() const {
  SystemRewriteOptions* options = new SystemRewriteOptions();
  options->Merge(*this);
  return options;
}

}  // namespace net_instaweb
