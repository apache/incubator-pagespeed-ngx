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
#include "net/instaweb/util/property_cache.pb.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

// Property cache key prefixes.
const char PropertyCache::kPagePropertyCacheKeyPrefix[] = "prop_page/";
const char PropertyCache::kClientPropertyCacheKeyPrefix[] = "prop_client/";

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
                             ThreadSystem* threads)
    : cache_key_prefix_(cache_key_prefix),
      cache_(cache),
      timer_(timer),
      thread_system_(threads),
      mutations_per_1000_writes_threshold_(
          kDefaultMutationsPer1000WritesThreshold),
      enabled_(true) {
}

PropertyCache::~PropertyCache() {
}

// Helper class to receive low-level cache callbacks, decode them
// as properties with meta-data (e.g. value stability), and
// store the payload for PropertyPage::Done().
class PropertyCache::CacheInterfaceCallback : public CacheInterface::Callback {
 public:
  CacheInterfaceCallback(PropertyPage* page, const Cohort* cohort,
                         PropertyPage::CallbackCollector* collector)
      : page_(page),
        cohort_(cohort),
        collector_(collector) {
  }
  virtual ~CacheInterfaceCallback() {}
  virtual void Done(CacheInterface::KeyState state) {
    bool valid = false;
    if (state == CacheInterface::kAvailable) {
      const GoogleString& value_string = *value()->get();
      ArrayInputStream input(value_string.data(), value_string.size());
      PropertyCacheValues values;
      if (values.ParseFromZeroCopyStream(&input)) {
        for (int i = 0; i < values.value_size(); ++i) {
          const PropertyValueProtobuf& pcache_value = values.value(i);
          page_->AddValueFromProtobuf(cohort_, pcache_value);
        }
        valid = true;
      }
    }
    collector_->Done(valid);
    delete this;
  }

 private:
  PropertyPage* page_;
  const Cohort* cohort_;
  PropertyPage::CallbackCollector* collector_;
};

void PropertyPage::AddValueFromProtobuf(
    const PropertyCache::Cohort* cohort,
    const PropertyValueProtobuf& pcache_value) {
  ScopedMutex lock(mutex_.get());
  CohortDataMap::const_iterator p = cohort_data_map_.find(cohort);
  PropertyMapStruct* pmap_struct = NULL;
  if (p != cohort_data_map_.end()) {
    pmap_struct = p->second;
  } else {
    pmap_struct = new PropertyMapStruct;
    cohort_data_map_[cohort] = pmap_struct;
  }
  PropertyMap* pmap = &pmap_struct->pmap;
  PropertyValue* property = (*pmap)[pcache_value.name()];
  if (property == NULL) {
    property = new PropertyValue;
    (*pmap)[pcache_value.name()] = property;
  }
  property->InitFromProtobuf(pcache_value);
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
    StringOutputStream sstream(value);
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

void PropertyCache::UpdateValue(const StringPiece& value,
                                PropertyValue* property) const {
  int64 now_ms = timer_->NowMs();

  // TODO(jmarantz): the policy of not having old timestamps override
  // new timestamps can cause us to discard some writes when
  // system-time jumps backwards, which can happen for various
  // reasons.  I think will need to revisit this policy as we learn how
  // to use the property cache & get the dynamics we want.
  if (property->write_timestamp_ms() <= now_ms) {
    property->SetValue(value, now_ms);
  }
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
  return StrCat(cache_key_prefix_, key, "@", *cohort);
}

void PropertyCache::Read(PropertyPage* page) const {
  if (enabled_ && !cohorts_.empty()) {
    PropertyPage::CallbackCollector* collector =
        new PropertyPage::CallbackCollector(
            page, cohorts_.size(), thread_system_->NewMutex());
    for (CohortSet::const_iterator p = cohorts_.begin(), e = cohorts_.end();
         p != e; ++p) {
      const Cohort& cohort = *p;
      const GoogleString cache_key = CacheKey(page->key(), &cohort);
      cache_->Get(cache_key,
                  new CacheInterfaceCallback(page, &cohort, collector));
    }
  } else {
    page->CallDone(false);
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

void PropertyCache::WriteCohort(const PropertyCache::Cohort* cohort,
                                PropertyPage* page) const {
  if (enabled_) {
    DCHECK(GetCohort(*cohort) == cohort);
    SharedString value;
    if (page->EncodeCacheEntry(cohort, value.get()) ||
        page->HasPropertyValueDeleted(cohort)) {
      const GoogleString cache_key = CacheKey(page->key(), cohort);
      cache_->Put(cache_key, &value);
    }
  }
}

bool PropertyCache::IsExpired(const PropertyValue* property_value,
                              int64 ttl_ms) const {
  DCHECK(property_value->has_value());
  int64 expiration_time_ms = property_value->write_timestamp_ms() + ttl_ms;
  return timer_->NowMs() > expiration_time_ms;
}

const PropertyCache::Cohort* PropertyCache::AddCohort(
    const StringPiece& cohort_name) {
  Cohort cohort_prototype;
  cohort_name.CopyToString(&cohort_prototype);
  std::pair<CohortSet::iterator, bool> insertion = cohorts_.insert(
      cohort_prototype);
  const Cohort& cohort = *insertion.first;
  return &cohort;
}

const PropertyCache::Cohort* PropertyCache::GetCohort(
    const StringPiece& cohort_name) const {
  // Since cohorts_ is a set<Cohort>, which is not the same C++ type as
  // a StringSet, we must actually construct a Cohort as a prototype in
  // order to look one up.
  Cohort cohort_prototype;
  cohort_name.CopyToString(&cohort_prototype);
  CohortSet::const_iterator p = cohorts_.find(cohort_prototype);
  if (p == cohorts_.end()) {
    return NULL;
  }
  const Cohort& cohort = *p;
  return &cohort;
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

PropertyValue* PropertyPage::GetProperty(const PropertyCache::Cohort* cohort,
                                         const StringPiece& property_name) {
  ScopedMutex lock(mutex_.get());
  DCHECK(was_read_);
  DCHECK(cohort != NULL);
  PropertyValue* property = NULL;
  CohortDataMap::iterator p = cohort_data_map_.find(cohort);
  PropertyMapStruct* pmap_struct = NULL;
  GoogleString property_name_str(property_name.data(), property_name.size());
  std::pair<CohortDataMap::iterator, bool> insertion = cohort_data_map_.insert(
      CohortDataMap::value_type(cohort, pmap_struct));
  if (insertion.second) {
    // The insertion occured: mutate the returned iterator with a new map.
    pmap_struct = new PropertyMapStruct;
    insertion.first->second = pmap_struct;
  } else {
    // The entry was already in the cohort map, so pull out the pmap.
    pmap_struct = insertion.first->second;
  }
  PropertyMap* pmap = &pmap_struct->pmap;
  property = (*pmap)[property_name_str];
  if (property == NULL) {
    property = new PropertyValue;
    (*pmap)[property_name_str] = property;
    property->set_was_read(was_read_);
  }
  return property;
}

void PropertyPage::DeleteProperty(const PropertyCache::Cohort* cohort,
                                  const StringPiece& property_name) {
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
