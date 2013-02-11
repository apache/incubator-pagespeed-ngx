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
             RewriteOptions::kFetcherProxy);
  add_option("", &SystemRewriteOptions::file_cache_path_, "afcp",
             RewriteOptions::kFileCachePath);
  add_option("", &SystemRewriteOptions::memcached_servers_, "ams",
             RewriteOptions::kMemcachedServers);
  add_option(1, &SystemRewriteOptions::memcached_threads_, "amt",
             RewriteOptions::kMemcachedThreads);
  add_option(0, &SystemRewriteOptions::memcached_timeout_us_, "amo",
             RewriteOptions::kMemcachedTimeoutUs);
  add_option("", &SystemRewriteOptions::statistics_logging_file_, "aslf",
             RewriteOptions::kStatisticsLoggingFile);
  add_option("", &SystemRewriteOptions::statistics_logging_charts_css_, "aslcc",
      RewriteOptions::kStatisticsLoggingChartsCSS);
  add_option("", &SystemRewriteOptions::statistics_logging_charts_js_, "aslcj",
      RewriteOptions::kStatisticsLoggingChartsJS);
  add_option(true, &SystemRewriteOptions::statistics_enabled_, "ase",
             RewriteOptions::kStatisticsEnabled);
  add_option(false, &SystemRewriteOptions::statistics_logging_enabled_, "asle",
             RewriteOptions::kStatisticsLoggingEnabled);
  add_option(true, &SystemRewriteOptions::use_shared_mem_locking_, "ausml",
             RewriteOptions::kUseSharedMemLocking);

  add_option(Timer::kHourMs,
             &SystemRewriteOptions::file_cache_clean_interval_ms_,
             "afcci", RewriteOptions::kFileCacheCleanIntervalMs);

  add_option(100 * 1024, &SystemRewriteOptions::file_cache_clean_size_kb_,
             "afc", RewriteOptions::kFileCacheCleanSizeKb);  // 100 megabytes
  // Default to no inode limit so that existing installations are not affected.
  // pagespeed.conf.template contains suggested limit for new installations.
  // TODO(morlovich): Inject this as an argument, since we want a different
  // default for ngx_pagespeed?
  add_option(0, &SystemRewriteOptions::file_cache_clean_inode_limit_, "afcl",
             RewriteOptions::kFileCacheCleanInodeLimit);
  add_option(0, &SystemRewriteOptions::lru_cache_byte_limit_, "alcb",
             RewriteOptions::kLruCacheByteLimit);
  add_option(0, &SystemRewriteOptions::lru_cache_kb_per_process_, "alcp",
             RewriteOptions::kLruCacheKbPerProcess);
  add_option(3000, &SystemRewriteOptions::statistics_logging_interval_ms_,
             "asli", RewriteOptions::kStatisticsLoggingIntervalMs);
  add_option("", &SystemRewriteOptions::cache_flush_filename_, "acff",
             RewriteOptions::kCacheFlushFilename);
  add_option(kDefaultCacheFlushIntervalSec,
             &SystemRewriteOptions::cache_flush_poll_interval_sec_, "acfpi",
             RewriteOptions::kCacheFlushPollIntervalSec);
  add_option("", &SystemRewriteOptions::use_shared_mem_metadata_cache_,
             "asmc", RewriteOptions::kUseSharedMemMetadataCache);

  MergeSubclassProperties(system_properties_);
}

SystemRewriteOptions* SystemRewriteOptions::Clone() const {
  SystemRewriteOptions* options = new SystemRewriteOptions();
  options->Merge(*this);
  return options;
}

}  // namespace net_instaweb
