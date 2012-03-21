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

// Author: mdw@google.com (Matt Welsh)

#include "net/instaweb/util/public/client_state.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/util/client_state.pb.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

const char ClientState::kClientStateCohort[] = "clientstate";
const char ClientState::kClientStatePropertyValue[] = "clientstate";
const uint32 ClientState::kClientStateMaxUrls = 1024;
const int64 ClientState::kClientStateExpiryTimeThresholdMs = 60 * 1000;

// This implementation is a skeleton that uses a simple FIFO queue to
// track recent URLs.

ClientState::ClientState()
    : timer_(NULL),
      property_page_(NULL),
      property_cache_(NULL) { }

ClientState::~ClientState() { }

// Performs a lookup in the ClientState.
bool ClientState::InCache(const GoogleString& url) {
  // TODO(mdw): Better implementation which is faster.
  return (std::find(recent_urls_.begin(), recent_urls_.end(), url) !=
          recent_urls_.end());
}

// Adds an object to the ClientState.
void ClientState::Set(const GoogleString& url, int64 expire_ms) {
  if (expire_ms >= kClientStateExpiryTimeThresholdMs) {
    recent_urls_.push_back(url);
    if (recent_urls_.size() > kClientStateMaxUrls) {
      recent_urls_.erase(recent_urls_.begin());
    }
  }
}

// Clears all state for this client.
void ClientState::Clear() {
  recent_urls_.clear();
}

// Packs the ClientState into the given protobuffer.
void ClientState::Pack(ClientStateMsg* proto) {
  DCHECK(!client_id_.empty());
  proto->set_client_id(client_id_);
  proto->set_create_time_ms(create_time_ms_);
  for (StringVector::iterator iter = recent_urls_.begin();
       iter != recent_urls_.end(); ++iter) {
    proto->add_recent_urls(*iter);
  }
}

// Unpacks the given protobuffer into this, replacing any previous contents.
bool ClientState::Unpack(const ClientStateMsg& proto) {
  if (!proto.has_client_id()) {
    LOG(WARNING) << "ClientStateMsg does not have client_id field";
    return false;
  }
  Clear();
  client_id_ = proto.client_id();
  create_time_ms_ = proto.create_time_ms();
  for (int i = 0, n = proto.recent_urls_size(); i < n; i++) {
    recent_urls_.push_back(proto.recent_urls(i));
  }
  return true;
}

bool ClientState::InitFromPropertyCache(
    const GoogleString& client_id,
    PropertyCache* property_cache,
    PropertyPage* property_page,
    Timer* timer) {
  client_id_ = client_id;
  create_time_ms_ = timer->NowMs();
  property_page_.reset(property_page);
  property_cache_ = property_cache;

  const PropertyCache::Cohort* cohort = property_cache->GetCohort(
      kClientStateCohort);
  DCHECK(cohort != NULL);
  PropertyValue* value = property_page->GetProperty(
      cohort, ClientState::kClientStatePropertyValue);
  DCHECK(value != NULL);
  if (!value->has_value()) {
    LOG(WARNING) << "Property value "
                 << ClientState::kClientStatePropertyValue << " has no value";
    return false;
  }
  // Read and unpack the protobuf.
  ClientStateMsg proto;
  if (!proto.ParseFromString(value->value().as_string())) {
    LOG(WARNING) << "Unable to parse protobuf " << value->value().as_string();
    return false;
  }
  return Unpack(proto);
}

void ClientState::WriteBackToPropertyCache() {
  DCHECK(property_page_.get() != NULL);
  DCHECK(property_cache_ != NULL);

  // Get the cohort and property value
  const PropertyCache::Cohort* cohort = property_cache_->GetCohort(
      kClientStateCohort);
  if (cohort == NULL) {
    LOG(WARNING) << "Not writing ClientState to pCache due to NULL cohort";
    return;
  }
  PropertyValue* value = property_page_.get()->GetProperty(
      cohort, ClientState::kClientStatePropertyValue);
  DCHECK(value != NULL);

  // Pack and serialize the ClientState protobuf
  ClientStateMsg proto;
  Pack(&proto);
  GoogleString bytes;
  if (!proto.SerializeToString(&bytes)) {
    LOG(WARNING) << "ClientState serialization failed, not writing back";
    return;
  }
  property_cache_->UpdateValue(bytes, value);
  property_cache_->WriteCohort(client_id_, cohort, property_page_.get());
}

}  // namespace net_instaweb
