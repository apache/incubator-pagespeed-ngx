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
#include "net/instaweb/public/version.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

const char ApacheConfig::kClassName[] = "ApacheConfig";

RewriteOptions::Properties* ApacheConfig::apache_properties_ = NULL;

void ApacheConfig::Initialize() {
  if (Properties::Initialize(&apache_properties_)) {
    RewriteOptions::Initialize();
    AddProperties();
  }
}

void ApacheConfig::Terminate() {
  if (Properties::Terminate(&apache_properties_)) {
    RewriteOptions::Terminate();
  }
}

ApacheConfig::ApacheConfig(const StringPiece& description)
    : description_(description.data(), description.size()) {
  Init();
}

ApacheConfig::ApacheConfig() {
  Init();
}

void ApacheConfig::Init() {
  DCHECK(apache_properties_ != NULL)
      << "Call ApacheConfig::Initialize() before construction";
  InitializeOptions(apache_properties_);
}

void ApacheConfig::AddProperties() {
  add_option("", &ApacheConfig::fetcher_proxy_, "afp",
             RewriteOptions::kFetcherProxy);
  add_option("", &ApacheConfig::file_cache_path_, "afcp",
             RewriteOptions::kFileCachePath);
  add_option("", &ApacheConfig::memcached_servers_, "ams",
             RewriteOptions::kMemcachedServers);
  add_option(1, &ApacheConfig::memcached_threads_, "amt",
             RewriteOptions::kMemcachedThreads);
  add_option("", &ApacheConfig::slurp_directory_, "asd",
             RewriteOptions::kSlurpDirectory);
  add_option("", &ApacheConfig::statistics_logging_file_, "aslf",
             RewriteOptions::kStatisticsLoggingFile);
  add_option(kOrganized, &ApacheConfig::referer_statistics_output_level_,
             "arso", RewriteOptions::kRefererStatisticsOutputLevel);
  add_option(false, &ApacheConfig::collect_referer_statistics_, "acrs",
             RewriteOptions::kCollectRefererStatistics);
  add_option(false, &ApacheConfig::hash_referer_statistics_, "ahrs",
             RewriteOptions::kHashRefererStatistics);
  add_option(true, &ApacheConfig::statistics_enabled_, "ase",
             RewriteOptions::kStatisticsEnabled);
  add_option(false, &ApacheConfig::statistics_logging_enabled_, "asle",
             RewriteOptions::kStatisticsLoggingEnabled);
  add_option(false, &ApacheConfig::test_proxy_, "atp",
             RewriteOptions::kTestProxy);
  add_option(false, &ApacheConfig::use_shared_mem_locking_, "ausml",
             RewriteOptions::kUseSharedMemLocking);
  add_option(false, &ApacheConfig::slurp_read_only_, "asro",
             RewriteOptions::kSlurpReadOnly);

  add_option(Timer::kHourMs, &ApacheConfig::file_cache_clean_interval_ms_,
             "afcci", RewriteOptions::kFileCacheCleanIntervalMs);

  add_option(100 * 1024, &ApacheConfig::file_cache_clean_size_kb_, "afc",
             RewriteOptions::kFileCacheCleanSizeKb);  // 100 megabytes
  // Default to no inode limit so that existing installations are not affected.
  // pagespeed.conf.template contains suggested limit for new installations.
  add_option(0, &ApacheConfig::file_cache_clean_inode_limit_, "afcl",
             RewriteOptions::kFileCacheCleanInodeLimit);
  add_option(0, &ApacheConfig::lru_cache_byte_limit_, "alcb",
             RewriteOptions::kLruCacheByteLimit);
  add_option(0, &ApacheConfig::lru_cache_kb_per_process_, "alcp",
             RewriteOptions::kLruCacheKbPerProcess);
  add_option(0, &ApacheConfig::slurp_flush_limit_, "asfl",
             RewriteOptions::kSlurpFlushLimit);
  add_option(3000, &ApacheConfig::statistics_logging_interval_ms_, "asli",
             RewriteOptions::kStatisticsLoggingIntervalMs);

  MergeSubclassProperties(apache_properties_);
  ApacheConfig config;
  config.InitializeSignaturesAndDefaults();
}

void ApacheConfig::InitializeSignaturesAndDefaults() {
  // TODO(jmarantz): Perform these operations on the Properties directly, rather
  // than going through a dummy ApacheConfig object to get to the properties.

  // Leave this out of the signature as (a) we don't actually change this
  // spontaneously, and (b) it's useful to keep the metadata cache between
  // slurping read-only and slurp read/write.
  slurp_read_only_.DoNotUseForSignatureComputation();

  // See the comment in RewriteOptions::RewriteOptions about leaving
  // the Signature() fairly comprehensive for now.
  //
  // fetcher_proxy_.DoNotUseForSignatureComputation();
  // file_cache_path_.DoNotUseForSignatureComputation();
  // slurp_directory_.DoNotUseForSignatureComputation();
  // referer_statistics_output_level_.DoNotUseForSignatureComputation();
  // collect_referer_statistics_.DoNotUseForSignatureComputation();
  // hash_referer_statistics_.DoNotUseForSignatureComputation();
  // statistics_enabled_.DoNotUseForSignatureComputation();
  // test_proxy_.DoNotUseForSignatureComputation();
  // use_shared_mem_locking_.DoNotUseForSignatureComputation();
  // file_cache_clean_interval_ms_.DoNotUseForSignatureComputation();
  // file_cache_clean_size_kb_.DoNotUseForSignatureComputation();
  // file_cache_clean_inode_limit_.DoNotUseForSignatureComputation();
  // lru_cache_byte_limit_.DoNotUseForSignatureComputation();
  // lru_cache_kb_per_process_.DoNotUseForSignatureComputation();
  // slurp_flush_limit_.DoNotUseForSignatureComputation();

  // Set mod_pagespeed-specific default header value.
  set_default_x_header_value(kModPagespeedVersion);
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

RewriteOptions* ApacheConfig::Clone() const {
  ApacheConfig* options = new ApacheConfig(description_);
  options->Merge(*this);
  return options;
}

const ApacheConfig* ApacheConfig::DynamicCast(const RewriteOptions* instance) {
  return (instance == NULL ||
          instance->class_name() != ApacheConfig::kClassName
          ? NULL
          : static_cast<const ApacheConfig*>(instance));
}

ApacheConfig* ApacheConfig::DynamicCast(RewriteOptions* instance) {
  return (instance == NULL ||
          instance->class_name() != ApacheConfig::kClassName
          ? NULL
          : static_cast<ApacheConfig*>(instance));
}

const char* ApacheConfig::class_name() const {
  return ApacheConfig::kClassName;
}

}  // namespace net_instaweb
