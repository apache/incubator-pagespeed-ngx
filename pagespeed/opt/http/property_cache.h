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
//    "prop/http://example.com/index.html@dom_metrics"
// we'll have a 2-element array of Property values for "num_divs" and
// "num_a_tags".  We'll write to that cache entry; possibly every
// time http://example.com/index.html is rewritten, so that we can track
// how stable the number of divs and a_tags is, so that rewriters that
// might wish to exploit advance knowledge of how many tags are going to
// be in the document can determine how reliable that information is.
//
// In the future we might track real-time & limit the frequency of
// updates for a given entry.

#ifndef PAGESPEED_OPT_HTTP_PROPERTY_CACHE_H_
#define PAGESPEED_OPT_HTTP_PROPERTY_CACHE_H_

#include <map>
#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/opt/http/request_context.h"

namespace net_instaweb {

class AbstractLogRecord;
class AbstractMutex;
class AbstractPropertyStoreGetCallback;
class PropertyCacheValues;
class PropertyValueProtobuf;
class PropertyPage;
class PropertyStore;
class Statistics;
class ThreadSystem;
class Timer;

typedef std::vector<PropertyPage*> PropertyPageStarVector;

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

  // Returns true if the value has not changed for last num_writes_unchanged
  // writes and false otherwise.
  bool IsRecentlyConstant(int num_writes_unchanged) const;

  // Returns true if the index of least set bit for value is less than given
  // index. The results are undefined when index is > 64.
  static bool IsIndexOfLeastSetBitSmaller(uint64 value, int index);

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
  // PropertyPage::UpdateValue only.
  //
  // Updating the value here buffers it in a protobuf, but does not commit
  // it to the cache. PropertyPage::WriteCohort() is required to commit.
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
  // A Cohort is a set of properties that update at roughly the
  // same expected frequency.  The PropertyCache object keeps track of
  // the known set of Cohorts but does not actually keep any data for
  // them.  The data only arrives when we do a lookup.
  class Cohort {
   public:
    explicit Cohort(StringPiece name) {
      name.CopyToString(&name_);
    }
    const GoogleString& name() const { return name_; }

   private:
    GoogleString name_;

    DISALLOW_COPY_AND_ASSIGN(Cohort);
  };

  typedef std::vector<const Cohort*> CohortVector;

  // Does not take ownership of the property_store, timer, stats, or threads
  // objects.
  PropertyCache(PropertyStore* property_store,
                Timer* timer,
                Statistics* stats,
                ThreadSystem* threads);
  ~PropertyCache();

  // Reads all the PropertyValues in all the known Cohorts from
  // cache, calling PropertyPage::Done when done.  It is essential
  // that the Cohorts are established prior to calling this function.
  void Read(PropertyPage* property_page) const;

  // Reads all the PropertyValues in the specified Cohorts from
  // cache, calling PropertyPage::Done when done.
  void ReadWithCohorts(const CohortVector& cohort_list,
                       PropertyPage* property_page) const;

  // Returns all the cohorts from cache.
  const CohortVector& GetAllCohorts() const { return cohort_list_; }

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

  void set_mutations_per_1000_writes_threshold(int x) {
    mutations_per_1000_writes_threshold_ = x;
  }

  // Establishes a new Cohort for this property cache. Note that you must call
  // InitCohortStats prior to calling AddCohort.
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

  // Initialize stats for the specified cohort.
  static void InitCohortStats(const GoogleString& cohort,
                              Statistics* statistics);

  // Creates stats prefix for the given cohort.
  static GoogleString GetStatsPrefix(const GoogleString& cohort_name);

  // Returns timer pointer.
  Timer* timer() const { return timer_; }

  ThreadSystem* thread_system() const { return thread_system_; }

  PropertyStore* property_store() { return property_store_; }

  // TODO(jmarantz): add some statistics tracking for stomps, stability, etc.

 private:
  PropertyStore* property_store_;
  Timer* timer_;
  Statistics* stats_;
  ThreadSystem* thread_system_;

  int mutations_per_1000_writes_threshold_;
  typedef std::map<GoogleString, Cohort*> CohortMap;
  CohortMap cohorts_;
  // For MutltiRead to scan all cohorts.
  CohortVector cohort_list_;
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(PropertyCache);
};

// Abstract interface for implementing a PropertyPage.
class AbstractPropertyPage {
 public:
  virtual ~AbstractPropertyPage();
  // Gets a property given the property name.  The property can then be
  // mutated, prior to the PropertyPage being written back to the cache.
  virtual PropertyValue* GetProperty(
      const PropertyCache::Cohort* cohort,
      const StringPiece& property_name) = 0;

  // Updates the value of a property, tracking stability & discarding
  // writes when the existing data is more up-to-date.
  virtual void UpdateValue(
     const PropertyCache::Cohort* cohort, const StringPiece& property_name,
     const StringPiece& value) = 0;

  // Updates a Cohort of properties into the cache.  It is a
  // programming error (dcheck-fail) to Write a PropertyPage that
  // was not read first.  It is fine to Write after a failed Read.
  virtual void WriteCohort(const PropertyCache::Cohort* cohort) = 0;

  // This function returns the cache state for a given cohort.
  virtual CacheInterface::KeyState GetCacheState(
     const PropertyCache::Cohort* cohort) = 0;

  // Deletes a property given the property name.
  virtual void DeleteProperty(const PropertyCache::Cohort* cohort,
                              const StringPiece& property_name) = 0;
};


// Holds the property values associated with a single key.  See more
// extensive comment for PropertyPage above.
class PropertyPage : public AbstractPropertyPage {
 public:
  // The cache type associated with this callback.
  enum PageType {
    kPropertyCachePage,
    kPropertyCacheFallbackPage,
    kPropertyCachePerOriginPage,
  };

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
  virtual PropertyValue* GetProperty(const PropertyCache::Cohort* cohort,
                                     const StringPiece& property_name);

  // Updates the value of a property, tracking stability & discarding
  // writes when the existing data is more up-to-date.
  virtual void UpdateValue(
      const PropertyCache::Cohort* cohort, const StringPiece& property_name,
      const StringPiece& value);

  // Updates a Cohort of properties into the cache.  It is a
  // programming error (dcheck-fail) to Write a PropertyPage that
  // was not read first.  It is fine to Write after a failed Read.
  //
  // Even if a PropertyValue was not changed since it was read, Write
  // should be called periodically to update stability metrics.
  virtual void WriteCohort(const PropertyCache::Cohort* cohort);

  // This function returns the cache state for a given cohort.
  //
  // It is a programming error to call GetCacheState on a PropertyPage
  // that has not yet been read.
  CacheInterface::KeyState GetCacheState(const PropertyCache::Cohort* cohort);

  // This function set the cache state for a given cohort. This is used by test
  // code and CacheCallback to populate the state.
  void SetCacheState(const PropertyCache::Cohort* cohort,
                     CacheInterface::KeyState x);

  // Deletes a property given the property name.
  //
  // This function deletes the PropertyValue if it already exists, otherwise
  // it is a no-op function.
  //
  // It is a programming error to call DeleteProperty on a PropertyPage
  // that has not yet been read.
  //
  // This function actually does not commit it to cache.
  void DeleteProperty(const PropertyCache::Cohort* cohort,
                      const StringPiece& property_name);

  AbstractLogRecord* log_record() {
    return request_context_->log_record();
  }

  // Read the property page from cache.
  void Read(const PropertyCache::CohortVector& cohort_list);

  // Abort the reading of PropertyPage.
  void Abort();

  // Called immediately after the underlying cache lookup is done, from
  // PropertyCache::CacheInterfaceCallback::Done().
  virtual bool IsCacheValid(int64 write_timestamp_ms) const { return true; }

  // Populate PropertyCacheValues to the respective cohort in PropertyPage.
  void AddValueFromProtobuf(const PropertyCache::Cohort* cohort,
                            const PropertyValueProtobuf& proto);

  // Returns the type of the page.
  PageType page_type() { return page_type_; }

  // Returns true if cohort present in the PropertyPage.
  bool IsCohortPresent(const PropertyCache::Cohort* cohort);

  // Finishes lookup for all the cohorts and call PropertyPage::Done() as fast
  // as possible.
  void FastFinishLookup();

  // Generates PropertyCacheValues object from all the properties in the given
  // cohort.
  // Returns false, if cohort does not exists in the PropertyPage or no
  // property is present in the cohort.
  bool EncodePropertyCacheValues(const PropertyCache::Cohort* cohort,
                                 PropertyCacheValues* values);

  // Suffix for property cache keys for given page type.
  static StringPiece PageTypeSuffix(PageType type);

 protected:
  // The Page takes ownership of the mutex.
  // TODO(pulkitg): Instead of passing full PropertyCache object, just pass
  // objects which PropertyPage needs.
  PropertyPage(PageType page_type,
               StringPiece url,
               StringPiece options_signature_hash,
               StringPiece cache_key_suffix,
               const RequestContextPtr& request_context,
               AbstractMutex* mutex,
               PropertyCache* property_cache);

  // Called as a result of PropertyCache::Read when the data is available.
  virtual void Done(bool success) = 0;

 private:
  void SetupCohorts(const PropertyCache::CohortVector& cohort_list);

  // Returns true if for the given cohort any property is deleted.
  bool HasPropertyValueDeleted(const PropertyCache::Cohort* cohort);

  void CallDone(bool success) {
    was_read_ = true;
    Done(success);
  }

  typedef std::map<GoogleString, PropertyValue*> PropertyMap;

  struct PropertyMapStruct {
    explicit PropertyMapStruct(AbstractLogRecord* log)
        : has_deleted_property(false),
          log_record(log),
          has_value(false) {}
    PropertyMap pmap;
    bool has_deleted_property;
    AbstractLogRecord* log_record;
    CacheInterface::KeyState cache_state;
    bool has_value;
  };
  typedef std::map<const PropertyCache::Cohort*, PropertyMapStruct*>
      CohortDataMap;
  CohortDataMap cohort_data_map_;
  scoped_ptr<AbstractMutex> mutex_;
  GoogleString url_;
  GoogleString options_signature_hash_;
  GoogleString cache_key_suffix_;
  RequestContextPtr request_context_;
  bool was_read_;
  PropertyCache* property_cache_;  // Owned by the caller.
  // AbstractPropertyStoreCallback is safe to use until
  // AbstractPropertyStoreCallback::DeleteWhenDone() which is called in
  // PropertyPage destructor, so property_store_callback_ lives longer than
  // PropertyPage.
  AbstractPropertyStoreGetCallback* property_store_callback_;
  PageType page_type_;

  DISALLOW_COPY_AND_ASSIGN(PropertyPage);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_OPT_HTTP_PROPERTY_CACHE_H_
