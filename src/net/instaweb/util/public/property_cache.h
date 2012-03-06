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
//
//
// Implements a cache that can be used to store multiple properties on
// a key.  This can be useful if the origin data associated with the
// key is not cacheable itself, but we think some properties of it
// might be reasonably stable.  The cache can optionally track how
// frequently the properties change, so that when a property is read,
// the reader can gauge how stable it is.  It also will manage
// time-based expirations of property-cache data (NYI).
//
// It supports properties with widely varying update frequences,
// though these must be specified by the programmer by grouping
// objects of similar frequency in a Cohort.
//
// Terminology:
//   PropertyCache -- adds property semantics & grouping to the raw
//   name/value Cache Interface.
//
//   PropertyValue -- a single name/value pair with stability
//   metadata, so that users of the PropertyValue can find out whether
//   the property being measured appears to be stable.
//
//   PropertyCache::Cohort -- labels a group of PropertyValues that
//   are expected to have similar write-frequency. Properties are
//   grouped together to minimize the number of cache lookups and
//   puts. But we do not want to put all values into a single Cohort
//   to avoid having fast-changing properties stomp on a slow-changing
//   properties that share the same cache entry.  Thus we initiate
//   lookpus for all Cohorts immediately on receiving a URL, but
//   we write back each Cohort independently, under programmer control.
//
//   The concurrent read of all Cohorts can be implemented on top of
//   a batched cache lookup if the platform supports it, to reduce
//   RPCs.
//
//   Note that the Cohort* is simply a label, and doesn't hold the
//   properties or the data.
//
//   PropertyPage -- this tracks all the PropertyValues in all the
//   Cohorts for a key (e.g., an HTML page URL).  Generally a
//   PropertyPage must be read prior to being written, so that
//   unmodified PropertyValues in a Cohort are not erased by updating
//   a single Cohert property.  The page executes a Read/Modify/Write
//   sequence, but there is no locking.  Multiple processes & threads
//   are potentially writing entries to the cache simultaneously, so
//   there can be races which might stomp on writes for individual
//   properties in a Cohort.
//
//   The value of aggregating multiple properties into a Cohort is
//   to reduce the query-traffic on caches.
//
// Let's study an example for URL "http://..." with two Cohorts,
// "dom_metrics" and "render_data", where we expect dom_metrics to be
// updated very frequently.  In dom_metrics we have (not that this is
// useful) "num_divs" and "num_a_tags".  In "render_data" we have
// "critical_image_list" and "referenced_resources".  When we get a
// request for "http://example.com/index.html" we'll make a batched
// lookup for 2 keys:
//
//    "prop/http://example.com/index.html@dom_metrics".
//    "prop/http://example.com/index.html@render_data".
//
// Within the values for
//    "pcache/http://example.com/index.html@dom_metrics"
// we'll have a 2-element array of Property values for "num_divs" and
// "num_a_tags".  We'll write to that cache entry; possibly every
// time http://example.com/index.html is rewritten, so that we can track
// how stable the number of divs and a_tags is, so that rewriters that
// might wish to exploit advance knowledge of how many tags are going to
// be in the document can determine how reliable that information is.
//
// In the future we might track real-time & limit the frequency of
// updates for a given entry.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_PROPERTY_CACHE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_PROPERTY_CACHE_H_

#include <map>
#include <set>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class CacheInterface;
class PropertyValueProtobuf;
class PropertyPage;
class ThreadSystem;
class Timer;

// Holds the value & stability-metadata for a property.
class PropertyValue {
 public:
  StringPiece value() const;
  bool has_value() const { return valid_; }

  // The timestamp of the last time this data was written (in
  // milliseconds since 1970).
  int64 write_timestamp_ms() const;

  // Determines whether a read was completed.  Thus was_read() can be true
  // even if !has_value().
  bool was_read() { return was_read_; }

  // Determines whether this property is sufficiently stable to be considered
  // useful.  E.g. if 30% of the time a property is wrong, then it probably
  // cannot be relied upon for making optimization decisions.
  bool IsStable(int stable_hit_per_thousand_threshold) const;

 private:
  friend class PropertyCache;
  friend class PropertyPage;

  // PropertyValues are managed by PropertyPage.
  PropertyValue();
  ~PropertyValue();

  void set_was_read(bool was_read) { was_read_ = was_read; }

  // Initializes the value based on a parsed protobuf from the physical cache.
  void InitFromProtobuf(const PropertyValueProtobuf& value);

  // Updates the value of a property, tracking stability so future
  // Readers can get a sense of how stable it is.  This is called from
  // PropertyCache::UpdateValue only.
  //
  // Updating the value here buffers it in a protobuf, but does not commit
  // it to the cache.  PropertyCache::WriteCohort() is required to commit.
  void SetValue(const StringPiece& value, int64 now_ms);

  PropertyValueProtobuf* protobuf() { return proto_.get(); }

  scoped_ptr<PropertyValueProtobuf> proto_;
  bool changed_;
  bool valid_;
  bool was_read_;

  DISALLOW_COPY_AND_ASSIGN(PropertyValue);
};

// Adds property-semantics to a raw cache API.
class PropertyCache {
 public:
  class CacheInterfaceCallback;

  // A Cohort is a set of properties that update at roughly the
  // same expected frequency.  The PropertyCache object keeps track of
  // the known set of Cohorts but does not actually keep any data for
  // them.  The data only arrives when we do a lookup.
  //
  // Note: if you add any new methods, consider using containment
  // rather than inheritance as GoogleString lacks a virtual dtor.
  //
  // Note that the PropertyCache::Cohort is just a predefined label
  // used for organizing properties.  The Cohort object doesn't
  // contain any data itself.
  class Cohort : public GoogleString {};

  PropertyCache(CacheInterface* cache, Timer* timer, ThreadSystem* threads);
  ~PropertyCache();

  // Reads the all the PropertyValues in all the known Cohorts from
  // cache, calling PropertyPage::Done when done.  It is essential
  // that the Cohorts are established prior to calling this function.
  void Read(const StringPiece& cache_key, PropertyPage* property_page) const;

  // Updates a Cohort of properties into the cache.  It is a
  // programming error (dcheck-fail) to Write a PropertyPage that
  // was not read first.  It is fine to Write after a failed Read.
  //
  // Even if a PropertyValue was not changed since it was read, Write
  // should be called periodically to update stability metrics.
  void WriteCohort(const StringPiece& key, const Cohort* cohort,
                   PropertyPage* property_page) const;

  // Determines whether a value that was read is reasonably stable.
  bool IsStable(const PropertyValue* property) const {
    return property->IsStable(mutations_per_1000_writes_threshold_);
  }

  // Determines whether a value is expired relative to the specified TTL.
  //
  // It is an error (DCHECK) to call this method when !property->has_value().
  //
  // Note; we could also store the TTL in the cache-value itself.  That would
  // be useful if we derived the TTL from the data or other transients.  But
  // our envisioned usage has the TTL coming from a configuration that is
  // available at read-time, so for now we just use that.
  bool IsExpired(const PropertyValue* property_value, int64 ttl_ms) const;

  // Updates the value of a property, tracking stability & discarding
  // writes when the existing data is more up-to-date.
  void UpdateValue(const StringPiece& value, PropertyValue* property) const;

  void set_mutations_per_1000_writes_threshold(int x) {
    mutations_per_1000_writes_threshold_ = x;
  }

  // Establishes a new Cohort for this property cache.
  const Cohort* AddCohort(const StringPiece& cohort_name);

  // Returns the specified Cohort* or NULL if not found.  Cohorts must
  // be established at startup time, via AddCohort before any pages
  // are processed via Read & Write.
  const Cohort* GetCohort(const StringPiece& cohort_name) const;

  // Allows turning off all reads/writes with a switch.  Writes to a
  // disabled cache are ignored.  Reads cause Done(false) to be called
  // immediately.
  void set_enabled(bool x) { enabled_ = x; }

  // Indicates if the property cache is enabled.
  bool enabled() const { return enabled_; }

  // Gets the underlying key associated with cache_key and a Cohort.
  // This is the key used for the CacheInterface provided to the
  // constructor.  This is made visible for testing, to make it
  // possible to inject delays into the cache via DelayCache::DelayKey.
  static GoogleString CacheKey(const StringPiece& key, const Cohort* cohort);

  // TODO(jmarantz): add a some statistics tracking for stomps, stability, etc.

 private:
  CacheInterface* cache_;
  Timer* timer_;
  ThreadSystem* thread_system_;

  int mutations_per_1000_writes_threshold_;
  typedef std::set<Cohort> CohortSet;
  CohortSet cohorts_;

  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(PropertyCache);
};

// Holds the property values associated with a single key.  See more
// extensive comment for PropertyPage above.
class PropertyPage {
 public:
  virtual ~PropertyPage();

  // Gets a property given the property name.  The property can then be
  // mutated, prior to the PropertyPage being written back to the cache.
  //
  // The returned PropertyValue object is owned by the PropertyPage and
  // should not be deleted by the caller.
  //
  // This function creates the PropertyValue if it didn't already
  // exist, either from a previous call or a cache-read.
  //
  // It is a programming error to call GetProperty on a PropertyPage
  // that has not yet been read.
  //
  // Note that all the properties in all the Cohorts on a Page are read
  // via PropertyCache::Read.  This allows cache implementations that support
  // batching to do so on the read.  However, properties are written back to
  // cache one Cohort at a time, via PropertyCache::WriteCohort.
  PropertyValue* GetProperty(const PropertyCache::Cohort* cohort,
                             const StringPiece& property_name);

 protected:
  // The Page takes ownership of the mutex.
  explicit PropertyPage(AbstractMutex* mutex)
      : mutex_(mutex),
        was_read_(false) {
  }

  // Called as a result of PropertyCache::Read when the data is available.
  virtual void Done(bool success) = 0;

 private:
  class CallbackCollector;
  friend class CallbackCollector;
  friend class PropertyCache::CacheInterfaceCallback;
  friend class PropertyCache;

  // Serializes the values in this cohort into a string.  Returns
  // false if there were no values to serialize.
  bool EncodeCacheEntry(const PropertyCache::Cohort* cohort,
                        GoogleString* value);

  void CallDone(bool success) {
    was_read_ = true;
    Done(success);
  }

  void AddValueFromProtobuf(const PropertyCache::Cohort* cohort,
                            const PropertyValueProtobuf& proto);

  typedef std::map<GoogleString, PropertyValue*> PropertyMap;
  typedef std::map<const PropertyCache::Cohort*, PropertyMap*> CohortDataMap;
  CohortDataMap cohort_data_map_;
  scoped_ptr<AbstractMutex> mutex_;
  bool was_read_;

  DISALLOW_COPY_AND_ASSIGN(PropertyPage);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_PROPERTY_CACHE_H_
