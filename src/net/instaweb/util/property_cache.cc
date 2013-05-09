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
#include "net/instaweb/util/public/cache_stats.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

// Property cache key prefixes.
const char PropertyCache::kPagePropertyCacheKeyPrefix[] = "prop_page/";
const char PropertyCache::kDevicePropertyCacheKeyPrefix[] = "prop_device/";

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

GoogleString GetStatsPrefix(const GoogleString& cohort_name) {
  return StrCat("pcache-cohorts-", cohort_name);
}

}  // namespace

// Tracks multiple cache lookups.  When they are all complete, page->Done() is
// called.
//
// TODO(jmarantz): refactor this to put the capability in CacheInterface, adding
// API support for a batched lookup.  Caches that have direct support for that
// may have a more efficient implementation for this.
class PropertyPage::CallbackCollector {
 public:
  CallbackCollector(PropertyPage* page, int num_pending, AbstractMutex* mutex)
      : page_(page),
        pending_(num_pending),
        success_(false),
        mutex_(mutex) {
  }

  void Done(bool success) {
    bool done = false;
    {
      ScopedMutex lock(mutex_.get());
      success_ |= success;  // Declare victory a if *any* lookups completed.
      --pending_;
      done = (pending_ == 0);
    }
    if (done) {
      page_->CallDone(success_);
      delete this;
    }
  }

 private:
  PropertyPage* page_;
  int pending_;
  bool success_;
  scoped_ptr<AbstractMutex> mutex_;

  DISALLOW_COPY_AND_ASSIGN(CallbackCollector);
};

PropertyCache::PropertyCache(const GoogleString& cache_key_prefix,
                             CacheInterface* cache, Timer* timer,
                             Statistics* stats, ThreadSystem* threads)
    : cache_key_prefix_(cache_key_prefix),
      cache_(cache),
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

// Helper class to receive low-level cache callbacks, decode them
// as properties with meta-data (e.g. value stability), and
// store the payload for PropertyPage::Done().
// TODO(pulkitg): Remove PropertyCache::CacheInterfaceCallback as friend class
// of PropertyPage.
class PropertyCache::CacheInterfaceCallback : public CacheInterface::Callback {
 public:
  CacheInterfaceCallback(PropertyPage* page, const Cohort* cohort,
                         PropertyPage::PropertyMapStruct* pmap_struct,
                         PropertyPage::CallbackCollector* collector)
      : page_(page),
        cohort_(cohort),
        pmap_struct_(pmap_struct),
        collector_(collector) {
  }
  virtual ~CacheInterfaceCallback() {}
  virtual void Done(CacheInterface::KeyState state) {
    bool valid = false;
    if (state == CacheInterface::kAvailable) {
      StringPiece value_string = value()->Value();
      ArrayInputStream input(value_string.data(), value_string.size());
      PropertyCacheValues values;
      if (values.ParseFromZeroCopyStream(&input)) {
        valid = true;
        int64 min_write_timestamp_ms = kint64max;
        // The values in a cohort could have different write_timestamp_ms
        // values, since it is populated in UpdateValue.  But since all values
        // in a cohort are written (and read) together we need to treat either
        // all as valid or none as valid.  Hence we look at the oldest write
        // timestamp to make this decision.
        for (int i = 0; i < values.value_size(); ++i) {
          min_write_timestamp_ms = std::min(
              min_write_timestamp_ms, values.value(i).write_timestamp_ms());
        }
        // Return valid for empty cohort, and if IsCacheValid returns true for
        // Value with oldest timestamp.
        if (values.value_size() == 0 ||
            page_->IsCacheValid(min_write_timestamp_ms)) {
          valid = true;
          for (int i = 0; i < values.value_size(); ++i) {
            const PropertyValueProtobuf& pcache_value = values.value(i);
            page_->AddValueFromProtobuf(cohort_, pcache_value);
          }
        } else {
          valid = false;
        }
      }
    }

    page_->log_record()->SetCacheStatusForCohortInfo(
        page_->page_type_, cohort_->name(), valid, state);
    pmap_struct_->cache_state = state;
    collector_->Done(valid);
    delete this;
  }

 private:
  PropertyPage* page_;
  const Cohort* cohort_;
  PropertyPage::PropertyMapStruct* pmap_struct_;
  PropertyPage::CallbackCollector* collector_;
};

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

bool PropertyPage::EncodeCacheEntry(const PropertyCache::Cohort* cohort,
                                    GoogleString* value) {
  bool ret = false;
  PropertyCacheValues values;
  {
    ScopedMutex lock(mutex_.get());
    CohortDataMap::const_iterator p = cohort_data_map_.find(cohort);
    if (p != cohort_data_map_.end()) {
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
          *values.add_value() = *pcache_value;
          ret = true;
        }
      }
    }
  }
  if (ret) {
    StringOutputStream sstream(value);  // finalizes in destructor
    values.SerializeToZeroCopyStream(&sstream);
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

GoogleString PropertyCache::CacheKey(const StringPiece& key,
                                     const Cohort* cohort) const {
  return StrCat(cache_key_prefix_, key, "@", cohort->name());
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
  SetupCohorts(cohort_list);
  CallbackCollector* collector = new CallbackCollector(
      this, cohort_list.size(), property_cache_->thread_system()->NewMutex());
  for (int j = 0, n = cohort_list.size(); j < n; ++j) {
    const PropertyCache::Cohort* cohort = cohort_list[j];
    PropertyPage::CohortDataMap::iterator cohort_itr =
        cohort_data_map_.find(cohort);
    CHECK(cohort_itr != cohort_data_map_.end());
    PropertyPage::PropertyMapStruct* pmap_struct = cohort_itr->second;
    const GoogleString cache_key = property_cache_->CacheKey(key(), cohort);
    cohort->cache()->Get(cache_key, new PropertyCache::CacheInterfaceCallback(
        this, cohort, pmap_struct, collector));
  }
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
  // Use the default cache implementation.
  return AddCohortWithCache(cohort_name, cache_);
}

const PropertyCache::Cohort* PropertyCache::AddCohortWithCache(
    const StringPiece& cohort_name, CacheInterface* cache) {
  CHECK(cache != NULL);
  CHECK(GetCohort(cohort_name) == NULL) << cohort_name << " is added twice.";
  GoogleString cohort_string;
  cohort_name.CopyToString(&cohort_string);
  std::pair<CohortMap::iterator, bool> insertions = cohorts_.insert(
      make_pair(cohort_string, static_cast<Cohort*>(NULL)));
  if (insertions.second) {
    // Create a new CacheStats for every cohort so that we can track cache
    // statistics independently for every cohort.
    CacheInterface* cache_stats = new CacheStats(
        GetStatsPrefix(cohort_string), cache, timer_, stats_);
    insertions.first->second = new Cohort(cohort_name, cache_stats);
    cohort_list_.push_back(insertions.first->second);
  }
  return insertions.first->second;
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

void PropertyCache::InitCohortStats(const GoogleString& cohort,
                                    Statistics* statistics) {
  CacheStats::InitStats(GetStatsPrefix(cohort), statistics);
}

AbstractPropertyPage::~AbstractPropertyPage() {
}

PropertyPage::PropertyPage(
    PageType page_type,
    const StringPiece& key,
    const RequestContextPtr& request_context,
    AbstractMutex* mutex,
    PropertyCache* property_cache)
      : mutex_(mutex),
        key_(key.as_string()),
        request_context_(request_context),
        was_read_(false),
        property_cache_(property_cache),
        page_type_(page_type) {
}

PropertyPage::~PropertyPage() {
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
    GoogleString value;
    if (EncodeCacheEntry(cohort, &value) ||
        HasPropertyValueDeleted(cohort)) {
      const GoogleString cache_key = property_cache_->CacheKey(key(), cohort);
      cohort->cache()->PutSwappingString(cache_key, &value);
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

void PropertyPage::set_cache_state_for_tests(
    const PropertyCache::Cohort* cohort,
    CacheInterface::KeyState x) {
  ScopedMutex lock(mutex_.get());
  DCHECK(was_read_);
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

}  // namespace net_instaweb
