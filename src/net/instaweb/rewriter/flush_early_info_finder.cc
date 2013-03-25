/*
 * Copyright 2012 Google Inc.
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

// Author: mmohabey@google.com (Megha Mohabey)

#include "net/instaweb/rewriter/public/flush_early_info_finder.h"

#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

const char FlushEarlyInfoFinder::kFlushEarlyRenderPropertyName[] =
    "flush_early_render";

FlushEarlyInfoFinder::~FlushEarlyInfoFinder() {
}

void FlushEarlyInfoFinder::UpdateFlushEarlyInfoInDriver(RewriteDriver* driver) {
  if (driver->flush_early_render_info() != NULL) {
    return;
  }

  PropertyCacheDecodeResult decode_status;
  scoped_ptr<FlushEarlyRenderInfo> flush_early_render_info(
      DecodeFromPropertyCache<FlushEarlyRenderInfo>(
          driver, GetCohort(), kFlushEarlyRenderPropertyName,
          driver->options()->finder_properties_cache_expiration_time_ms(),
          &decode_status));
  if (decode_status == kPropertyCacheDecodeOk) {
    driver->set_flush_early_render_info(flush_early_render_info.release());
  } else if (decode_status == kPropertyCacheDecodeParseError) {
    driver->message_handler()->Message(kError, "Parsing value from cache "
                                        "into FlushEarlyRenderInfo failed.");
  }
}

void FlushEarlyInfoFinder::ComputeFlushEarlyInfo(RewriteDriver* driver) {
  // Default interface is empty and derived classes can override.
}

const char* FlushEarlyInfoFinder::GetCharset(
    const RewriteDriver* driver) {
  FlushEarlyRenderInfo* flush_early_render_info =
      driver->flush_early_render_info();
  if (flush_early_render_info != NULL) {
    return flush_early_render_info->charset().c_str();
  }
  return "";
}

void FlushEarlyInfoFinder::UpdateFlushEarlyInfoCacheEntry(
    RewriteDriver* driver,
    FlushEarlyRenderInfo* flush_early_render_info) {
  PropertyCache* property_cache =
      driver->server_context()->page_property_cache();
  PropertyPage* page = driver->property_page();
  if (property_cache != NULL && page != NULL) {
    const PropertyCache::Cohort* cohort = property_cache->GetCohort(
        GetCohort());
    if (cohort != NULL) {
      flush_early_render_info->set_updated(true);
      GoogleString value;
      {
        StringOutputStream sstream(&value);  // finalizes in destructor
        flush_early_render_info->SerializeToZeroCopyStream(&sstream);
      }
      page->UpdateValue(cohort, kFlushEarlyRenderPropertyName, value);
    } else {
      driver->message_handler()->Message(kWarning, "FlushEarly FinderCohort is"
                                         "NULL.");
    }
  }
}

}  // namespace net_instaweb
