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

#include "base/logging.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class ThreadSystem;

namespace {

const char kModPagespeedStatisticsHandlerPath[] = "/mod_pagespeed_statistics";

}  // namespace

RewriteOptions::Properties* ApacheConfig::apache_properties_ = NULL;

void ApacheConfig::Initialize() {
  if (Properties::Initialize(&apache_properties_)) {
    SystemRewriteOptions::Initialize();
    AddProperties();
  }
}

void ApacheConfig::Terminate() {
  if (Properties::Terminate(&apache_properties_)) {
    SystemRewriteOptions::Terminate();
  }
}

ApacheConfig::ApacheConfig(const StringPiece& description,
                           ThreadSystem* thread_system)
    : SystemRewriteOptions(thread_system),
      description_(description.data(), description.size()) {
  Init();
}

ApacheConfig::ApacheConfig(ThreadSystem* thread_system)
    : SystemRewriteOptions(thread_system) {
  Init();
}

void ApacheConfig::Init() {
  DCHECK(apache_properties_ != NULL)
      << "Call ApacheConfig::Initialize() before construction";
  InitializeOptions(apache_properties_);

  // Apache-specific default.
  // TODO(sligocki): Get rid of this line and let both Apache and Nginx use
  // /pagespeed_statistics as the handler.
  statistics_handler_path_.set_default(kModPagespeedStatisticsHandlerPath);
}

void ApacheConfig::AddProperties() {
  AddApacheProperty(
      "", &ApacheConfig::slurp_directory_, "asd",
      RewriteOptions::kSlurpDirectory,
      "Directory from which to read slurped resources");
  AddApacheProperty(
      false, &ApacheConfig::test_proxy_, "atp",
      RewriteOptions::kTestProxy,
      "Direct non-mod_pagespeed URLs to a fetcher, acting as a simple "
      "proxy. Meant for test use only");
  AddApacheProperty(
      "", &ApacheConfig::test_proxy_slurp_, "atps",
      RewriteOptions::kTestProxySlurp,
      "If set, the fetcher used by the TestProxy mode will be a "
      "readonly slurp fetcher from the given directory");
  AddApacheProperty(
      false, &ApacheConfig::slurp_read_only_, "asro",
      RewriteOptions::kSlurpReadOnly,
      "Only read from the slurped directory, fail to fetch "
      "URLs not already in the slurped directory");
  AddApacheProperty(
      false, &ApacheConfig::rate_limit_background_fetches_, "rlbf",
      RewriteOptions::kRateLimitBackgroundFetches,
      "Rate-limit the number of background HTTP fetches done at once");
  AddApacheProperty(
      0, &ApacheConfig::slurp_flush_limit_, "asfl",
      RewriteOptions::kSlurpFlushLimit,
      "Set the maximum byte size for the slurped content to hold before "
      "a flush");
  AddApacheProperty(
      false, &ApacheConfig::experimental_fetch_from_mod_spdy_, "effms",
      RewriteOptions::kExperimentalFetchFromModSpdy,
      "Under construction. Do not use");

  MergeSubclassProperties(apache_properties_);

  // TODO(jmarantz): We allow a special instantiation of ApacheConfig
  // with null thread system because we are only updating the static properties
  // on process startup; we won't have a thread-system yet or multiple threads.
  //
  // We should get rid of this by moving the DoNotUseForSignatureComputation
  // bit into the Property constructor.
  ApacheConfig config(NULL);
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

ApacheConfig* ApacheConfig::Clone() const {
  ApacheConfig* options = new ApacheConfig(description_, thread_system());
  options->Merge(*this);
  return options;
}

ApacheConfig* ApacheConfig::NewOptions() const {
  return new ApacheConfig(thread_system());
}

const ApacheConfig* ApacheConfig::DynamicCast(const RewriteOptions* instance) {
  const ApacheConfig* config = dynamic_cast<const ApacheConfig*>(instance);
  DCHECK(config != NULL);
  return config;
}

ApacheConfig* ApacheConfig::DynamicCast(RewriteOptions* instance) {
  ApacheConfig* config = dynamic_cast<ApacheConfig*>(instance);
  DCHECK(config != NULL);
  return config;
}

}  // namespace net_instaweb
