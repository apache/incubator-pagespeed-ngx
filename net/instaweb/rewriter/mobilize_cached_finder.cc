/*
 * Copyright 2015 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/mobilize_cached_finder.h"

#include <map>

#include "base/logging.h"
#include "net/instaweb/rewriter/critical_keys.pb.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

const char MobilizeCachedFinder::kMobilizeCachedPropertyName[] =
    "mobilize_cached";

const char MobilizeCachedFinder::kMobilizeCachedValidCount[] =
    "mobilize_cached_valid_count";

const char MobilizeCachedFinder::kMobilizeCachedExpiredCount[] =
    "mobilize_cached_expired_count";

const char MobilizeCachedFinder::kMobilizeCachedNotFoundCount[] =
    "mobilize_cached_not_found_count";

const char MobilizeCachedFinder::kMobilizeCachedNoConsensusCount[] =
    "mobilize_cached_no_consensus_count";


MobilizeCachedFinder::MobilizeCachedFinder(
    const PropertyCache::Cohort* cohort, Statistics* statistics)
    : cohort_(cohort) {
  mobilize_cached_valid_count_ = statistics->GetTimedVariable(
      kMobilizeCachedValidCount);
  mobilize_cached_expired_count_ = statistics->GetTimedVariable(
      kMobilizeCachedExpiredCount);
  mobilize_cached_not_found_count_ = statistics->GetTimedVariable(
      kMobilizeCachedNotFoundCount);
  mobilize_cached_no_consensus_count_ = statistics->GetTimedVariable(
      kMobilizeCachedNoConsensusCount);
}

MobilizeCachedFinder::~MobilizeCachedFinder() {
}

void MobilizeCachedFinder::InitStats(Statistics* statistics) {
  statistics->AddTimedVariable(kMobilizeCachedValidCount,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kMobilizeCachedExpiredCount,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kMobilizeCachedNotFoundCount,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kMobilizeCachedNoConsensusCount,
                               ServerContext::kStatisticsGroup);
}

bool MobilizeCachedFinder::GetMobilizeCachedFromPropertyCache(
    RewriteDriver* driver, MobilizeCached* out) {
  PropertyCacheDecodeResult result;
  // NOTE: if any of these checks fail you probably didn't set up your test
  // environment carefully enough.  Figuring that out based on test failures
  // alone will drive you nuts and take hours out of your life, thus DCHECKs.
  DCHECK(driver != NULL);
  DCHECK(cohort_ != NULL);
  // TODO(morlovich): Uses the wrong page (should be origin_property_page()).
  scoped_ptr<CriticalKeys> critical_keys(DecodeFromPropertyCache<CriticalKeys>(
      driver, cohort_, kMobilizeCachedPropertyName,
      driver->options()->finder_properties_cache_expiration_time_ms(),
      &result));
  switch (result) {
    case kPropertyCacheDecodeNotFound:
      mobilize_cached_not_found_count_->IncBy(1);
      return false;
    case kPropertyCacheDecodeExpired:
      mobilize_cached_expired_count_->IncBy(1);
      return false;
    case kPropertyCacheDecodeParseError:
      driver->message_handler()->Message(
          kWarning, "Unable to parse Mobilize Cached PropertyValue; "
          "url: %s", driver->url());
      return false;
    case kPropertyCacheDecodeOk:
      if (critical_keys.get() == NULL) {
        LOG(DFATAL) << "NULL critical_keys with success status of decoding";
        return false;
      }
      break;
  }

  // Use the critical key framework to aggregate various samples.
  StringSet encoded_out;
  GetCriticalKeysFromProto(51 /* support_percentage */, *critical_keys,
                           &encoded_out);
  if (encoded_out.size() >= 2) {
    LOG(DFATAL)
        << "Two candidates have more than 51% support, that makes no sense!?";
    return false;
  }

  if (encoded_out.size() == 0) {
    mobilize_cached_no_consensus_count_->IncBy(1);
    return false;
  }

  // The selected string is the encoded MobilizeCached.
  const GoogleString& encoded_result = *encoded_out.begin();
  ArrayInputStream input(encoded_result.data(), encoded_result.size());
  bool ok = out->ParseFromZeroCopyStream(&input);
  if (ok) {
    mobilize_cached_valid_count_->IncBy(1);
  } else {
    driver->message_handler()->Message(
        kWarning, "Unable to parse selected MobilizeCached; url: %s",
        driver->url());
  }
  return ok;
}

void MobilizeCachedFinder::UpdateMobilizeCachedInPropertyCache(
    const MobilizeCached& new_sample, RewriteDriver* driver) {
  // Marshal the new_sample to string.
  GoogleString new_sample_encoded;
  {
    StringOutputStream sstream(&new_sample_encoded);
    new_sample.SerializeToZeroCopyStream(&sstream);
  }

  StringSet new_sample_set;
  new_sample_set.insert(new_sample_encoded);

  // TODO(morlovich): wrong page (should be origin_property_page()).
  WriteCriticalKeysToPropertyCache(
    new_sample_set, NULL /* not using nonces */,
    100, /* ??? support_interval */
    kSkipNonceCheck,
    kMobilizeCachedPropertyName,
    driver->server_context()->page_property_cache(), cohort_,
    driver->property_page(), driver->message_handler(), driver->timer());
}

}  // namespace net_instaweb
