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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/property_cache.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/util/property_cache.pb.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/abstract_property_store_get_callback.h"
#include "net/instaweb/util/public/cache_stats.h"
#include "net/instaweb/util/public/property_store.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/timer.h"
#include "pagespeed/kernel/base/callback.h"

namespace net_instaweb {

namespace {

const int kDefaultMutationsPer1000WritesThreshold = 300;

// http://stackoverflow.com/questions/109023/
// best-algorithm-to-count-the-number-of-set-bits-in-a-32-bit-integer
//
// Note that gcc has __builtin_popcount(i) and we could conceivably
// exploit that.  Also note that g++ does not seem to support that on
// MacOS.  In the future we could consider switching to that
// implementation if this ever was measured as a performance
// bottleneck.
inline uint32 NumberOfSetBits32(uint32 i) {
  uint32 num_set = i - ((i >> 1) & 0x55555555);
  num_set = (num_set & 0x33333333) + ((num_set >> 2) & 0x33333333);
  return (((num_set + (num_set >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

inline int NumberOfSetBits64(uint64 i) {
  return (NumberOfSetBits32(static_cast<uint32>(i)) +
          NumberOfSetBits32(static_cast<uint32>(i >> 32)));
}

}  // namespace

PropertyCache::PropertyCache(PropertyStore* property_store,
                             Timer* timer,
                             Statistics* stats,
                             ThreadSystem* threads)
    : property_store_(property_store),
      timer_(timer),
      stats_(stats),
      thread_system_(threads),
      mutations_per_1000_writes_threshold_(
          kDefaultMutationsPer1000WritesThreshold),
      enabled_(true) {
}

PropertyCache::~PropertyCache() {
  STLDeleteValues(&cohorts_);
}

void PropertyPage::AddValueFromProtobuf(
    const PropertyCache::Cohort* cohort,
    const PropertyValueProtobuf& pcache_value) {
  ScopedMutex lock(mutex_.get());
  CohortDataMap::iterator cohort_itr = cohort_data_map_.find(cohort);
  CHECK(cohort_itr != cohort_data_map_.end());
  PropertyMapStruct* pmap_struct = cohort_itr->second;
  PropertyMap* pmap = &pmap_struct->pmap;
  PropertyValue* property = (*pmap)[pcache_value.name()];
  if (property == NULL) {
    property = new PropertyValue;
    (*pmap)[pcache_value.name()] = property;
    log_record()->AddFoundPropertyToCohortInfo(
        page_type_, cohort->name(), pcache_value.name());
  }
  pmap_struct->has_value = true;
  property->InitFromProtobuf(pcache_value);
}

void PropertyPage::SetupCohorts(
    const PropertyCache::CohortVector& cohort_list) {
  for (int j = 0, n = cohort_list.size(); j < n; ++j) {
    const PropertyCache::Cohort* cohort = cohort_list[j];
    PropertyMapStruct* pmap_struct = new PropertyMapStruct(log_record());
    cohort_data_map_[cohort] = pmap_struct;
  }
}

bool PropertyPage::EncodePropertyCacheValues(
    const PropertyCache::Cohort* cohort, PropertyCacheValues* values) {
  ScopedMutex lock(mutex_.get());
  CohortDataMap::const_iterator p = cohort_data_map_.find(cohort);
  if (p == cohort_data_map_.end()) {
    return false;
  }

  bool ret = false;
  PropertyMap* pmap = &p->second->pmap;
  for (PropertyMap::iterator p = pmap->begin(), e = pmap->end();
       p != e; ++p) {
    PropertyValue* property = p->second;
    PropertyValueProtobuf* pcache_value = property->protobuf();
    if (pcache_value->name().empty()) {
      pcache_value->set_name(p->first);
    }

    // Why might the value be empty? If a cache lookup is performed, misses,
    // and UpdateValue() is never called. In this case, we can skip the
    // write.
    if (!pcache_value->body().empty()) {
      *values->add_value() = *pcache_value;
      ret = true;
    }
  }
  return ret;
}

bool PropertyPage::HasPropertyValueDeleted(
    const PropertyCache::Cohort* cohort) {
  bool ret = false;
  {
    ScopedMutex lock(mutex_.get());
    CohortDataMap::const_iterator p = cohort_data_map_.find(cohort);
    if (p != cohort_data_map_.end()) {
      ret = p->second->has_deleted_property;
    }
  }
  return ret;
}

PropertyValue::PropertyValue()
  : proto_(new PropertyValueProtobuf),
    changed_(true),
    valid_(false),
    was_read_(false) {
}

PropertyValue::~PropertyValue() {
}

void PropertyValue::InitFromProtobuf(const PropertyValueProtobuf& value) {
  proto_.reset(new PropertyValueProtobuf(value));
  changed_ = false;
  valid_ = true;
  was_read_ = true;
}

void PropertyValue::SetValue(const StringPiece& value, int64 now_ms) {
  if (!valid_ || (value != proto_->body())) {
    valid_ = true;
    changed_ = true;
    value.CopyToString(proto_->mutable_body());
  }
  proto_->set_update_mask((proto_->update_mask() << 1) | changed_);
  proto_->set_num_writes(proto_->num_writes() + 1);
  proto_->set_write_timestamp_ms(now_ms);
}

StringPiece PropertyValue::value() const {
  return StringPiece(proto_->body());
}

int64 PropertyValue::write_timestamp_ms() const {
  return proto_->write_timestamp_ms();
}

// TODO(hujie): Remove Read after all original Read calls  are changed to
// ReadWithCohorts.
void PropertyCache::Read(PropertyPage* page) const {
  ReadWithCohorts(cohort_list_, page);
}

void PropertyCache::ReadWithCohorts(const CohortVector& cohort_list,
                                    PropertyPage* page) const {
  if (!enabled_ || cohort_list.empty()) {
    page->Abort();
    return;
  }
  page->Read(cohort_list);
}

void PropertyPage::Abort() {
  CallDone(false);
}

void PropertyPage::Read(const PropertyCache::CohortVector& cohort_list) {
  DCHECK(!cohort_list.empty());
  DCHECK(property_store_callback_ == NULL);
  SetupCohorts(cohort_list);
  property_cache_->property_store()->Get(
      url_,
      options_signature_hash_,
      cache_key_suffix_,
      cohort_list,
      this,
      NewCallback(this, &PropertyPage::CallDone),
      &property_store_callback_);
}

bool PropertyValue::IsStable(int mutations_per_1000_threshold) const {
  // We allocate a 64-bit mask to record whether recent calls to Write
  // actually changed the data.  So although we keep a total number of
  // Writes that is not clamped to 64, we need to clamp between 1-64 so
  // we can use this as a divisor to determine stability.
  int num_writes = std::max(static_cast<int64>(1),
                            std::min(static_cast<int64>(64),
                                     proto_->num_writes()));
  int num_changes = NumberOfSetBits64(proto_->update_mask());
  int changes_per_1000_writes = (1000 * num_changes) / num_writes;
  return (changes_per_1000_writes < mutations_per_1000_threshold);
}

bool PropertyValue::IsRecentlyConstant(int num_writes_unchanged) const {
  int num_pcache_writes = proto_->num_writes();
  if (num_writes_unchanged > 64) {
    // We track at most 64 writes in update_mask.
    return false;
  }
  // If we have not seen num_writes_unchanged writes then just check whether all
  // the writes were for the same value.
  if (num_writes_unchanged > num_pcache_writes) {
    num_writes_unchanged = num_pcache_writes;
  }
  uint64 update_mask =  proto_->update_mask();
  // Check if the index of least set bit for update_mask is >=
  // num_writes_unchanged. OR all writes are for the same value.
  return !IsIndexOfLeastSetBitSmaller(update_mask, num_writes_unchanged) ||
      (update_mask == 0);
}

bool PropertyValue::IsIndexOfLeastSetBitSmaller(uint64 value, int index) {
  uint64 check_mask = static_cast<uint64>(1) << std::max(index - 1, 0);
  return ((value & ~(value - 1)) < check_mask);
}

bool PropertyCache::IsExpired(const PropertyValue* property_value,
                              int64 ttl_ms) const {
  DCHECK(property_value->has_value());
  int64 expiration_time_ms = property_value->write_timestamp_ms() + ttl_ms;
  return timer_->NowMs() > expiration_time_ms;
}

const PropertyCache::Cohort* PropertyCache::AddCohort(
    const StringPiece& cohort_name) {
  PropertyCache::Cohort* cohort = new PropertyCache::Cohort(cohort_name);
  std::pair<CohortMap::iterator, bool> insertions = cohorts_.insert(
        make_pair(cohort->name(), static_cast<Cohort*>(NULL)));
  CHECK(insertions.second) << cohort->name() << " is added twice.";
  insertions.first->second = cohort;
  cohort_list_.push_back(insertions.first->second);
  return cohort;
}

const PropertyCache::Cohort* PropertyCache::GetCohort(
    const StringPiece& cohort_name) const {
  GoogleString cohort_string;
  cohort_name.CopyToString(&cohort_string);
  CohortMap::const_iterator p = cohorts_.find(cohort_string);
  if (p == cohorts_.end()) {
    return NULL;
  }
  const Cohort* cohort = p->second;
  return cohort;
}

GoogleString PropertyCache::GetStatsPrefix(const GoogleString& cohort_name) {
  return StrCat("pcache-cohorts-", cohort_name);
}

void PropertyCache::InitCohortStats(const GoogleString& cohort,
                                    Statistics* statistics) {
  CacheStats::InitStats(GetStatsPrefix(cohort), statistics);
}

AbstractPropertyPage::~AbstractPropertyPage() {
}

PropertyPage::PropertyPage(
    PageType page_type,
    StringPiece url,
    StringPiece options_signature_hash,
    StringPiece cache_key_suffix,
    const RequestContextPtr& request_context,
    AbstractMutex* mutex,
    PropertyCache* property_cache)
    : mutex_(mutex),
      url_(url.as_string()),
      options_signature_hash_(options_signature_hash.as_string()),
      cache_key_suffix_(cache_key_suffix.as_string()),
      request_context_(request_context),
      was_read_(false),
      property_cache_(property_cache),
      property_store_callback_(NULL),
      page_type_(page_type) {
}

PropertyPage::~PropertyPage() {
  if (property_store_callback_ != NULL) {
    property_store_callback_->DeleteWhenDone();
  }
  while (!cohort_data_map_.empty()) {
    CohortDataMap::iterator p = cohort_data_map_.begin();
    PropertyMapStruct* pmap_struct = p->second;
    PropertyMap* pmap = &p->second->pmap;
    cohort_data_map_.erase(p);

    // TODO(jmarantz): Not currently Using STLDeleteValues because
    // ~PropertyValue is private, the syntax for friending template
    // functions eludes me.
    for (PropertyMap::iterator p = pmap->begin(), e = pmap->end(); p != e;
         ++p) {
      PropertyValue* value = p->second;
      delete value;
    }
    delete pmap_struct;
  }
}

PropertyValue* PropertyPage::GetProperty(
    const PropertyCache::Cohort* cohort,
    const StringPiece& property_name) {
  ScopedMutex lock(mutex_.get());
  DCHECK(was_read_);
  DCHECK(cohort != NULL);
  PropertyValue* property = NULL;
  GoogleString property_name_str(property_name.data(), property_name.size());
  CohortDataMap::const_iterator cohort_itr = cohort_data_map_.find(cohort);
  CHECK(cohort_itr != cohort_data_map_.end());
  PropertyMapStruct* pmap_struct = cohort_itr->second;
  PropertyMap* pmap = &pmap_struct->pmap;
  property = (*pmap)[property_name_str];
  log_record()->AddRetrievedPropertyToCohortInfo(
      page_type_, cohort->name(), property_name.as_string());
  if (property == NULL) {
    property = new PropertyValue;
    (*pmap)[property_name_str] = property;
    property->set_was_read(was_read_);
  }
  return property;
}

void PropertyPage::UpdateValue(
    const PropertyCache::Cohort* cohort, const StringPiece& property_name,
    const StringPiece& value) {
  if (cohort == NULL) {
    // TODO(pulkitg): Change LOG(WARNING) to LOG(DFATAL).
    LOG(WARNING) << "Cohort is NULL in PropertyPage::UpdateValue()";
    return;
  }
  PropertyValue* property = GetProperty(cohort, property_name);
  int64 now_ms = property_cache_->timer()->NowMs();

  // TODO(jmarantz): the policy of not having old timestamps override
  // new timestamps can cause us to discard some writes when
  // system-time jumps backwards, which can happen for various
  // reasons.  I think will need to revisit this policy as we learn how
  // to use the property cache & get the dynamics we want.
  if (property->write_timestamp_ms() <= now_ms) {
    property->SetValue(value, now_ms);
  }
}

void PropertyPage::WriteCohort(const PropertyCache::Cohort* cohort) {
  if (cohort == NULL) {
    // TODO(pulkitg): Change LOG(WARNING) to LOG(DFATAL).
    LOG(WARNING) << "Cohort is NULL in PropertyPage::WriteCohort()";
    return;
  }
  if (property_cache_->enabled()) {
    PropertyCacheValues values;
    if (EncodePropertyCacheValues(cohort, &values) ||
        HasPropertyValueDeleted(cohort)) {
      property_cache_->property_store()->Put(
          url_,
          options_signature_hash_,
          cache_key_suffix_,
          cohort,
          &values,
          NULL);
    }
  }
}

CacheInterface::KeyState PropertyPage::GetCacheState(
    const PropertyCache::Cohort* cohort) {
  ScopedMutex lock(mutex_.get());
  DCHECK(was_read_);
  DCHECK(cohort != NULL);
  CohortDataMap::iterator cohort_itr = cohort_data_map_.find(cohort);
  CHECK(cohort_itr != cohort_data_map_.end());
  PropertyMapStruct* pmap_struct = cohort_itr->second;
  return pmap_struct->cache_state;
}

void PropertyPage::SetCacheState(
    const PropertyCache::Cohort* cohort,
    CacheInterface::KeyState x) {
  ScopedMutex lock(mutex_.get());
  DCHECK(cohort != NULL);
  CohortDataMap::iterator cohort_itr = cohort_data_map_.find(cohort);
  CHECK(cohort_itr != cohort_data_map_.end());
  PropertyMapStruct* pmap_struct = cohort_itr->second;
  pmap_struct->cache_state = x;
}

void PropertyPage::DeleteProperty(
    const PropertyCache::Cohort* cohort, const StringPiece& property_name) {
  DCHECK(was_read_);
  DCHECK(cohort != NULL);
  ScopedMutex lock(mutex_.get());
  CohortDataMap::iterator cohort_itr = cohort_data_map_.find(cohort);
  if (cohort_itr == cohort_data_map_.end()) {
    return;
  }
  PropertyMapStruct* pmap_struct = cohort_itr->second;
  PropertyMap* pmap = &pmap_struct->pmap;
  PropertyMap::iterator pmap_itr = pmap->find(property_name.as_string());
  if (pmap_itr == pmap->end()) {
    return;
  }
  PropertyValue* property = pmap_itr->second;
  pmap->erase(pmap_itr);
  pmap_struct->has_deleted_property = true;
  delete property;
}

bool PropertyPage::IsCohortPresent(const PropertyCache::Cohort* cohort) {
  ScopedMutex lock(mutex_.get());
  DCHECK(cohort != NULL);
  CohortDataMap::iterator cohort_itr = cohort_data_map_.find(cohort);
  CHECK(cohort_itr != cohort_data_map_.end());
  PropertyMapStruct* pmap_struct = cohort_itr->second;
  return pmap_struct->has_value;
}

void PropertyPage::FastFinishLookup() {
  if (property_store_callback_ != NULL) {
    property_store_callback_->FastFinishLookup();
  }
}

}  // namespace net_instaweb
