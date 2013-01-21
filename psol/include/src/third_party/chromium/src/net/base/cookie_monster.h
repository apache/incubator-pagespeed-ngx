// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by the letter D and the number 2.

#ifndef NET_BASE_COOKIE_MONSTER_H_
#define NET_BASE_COOKIE_MONSTER_H_
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "base/time.h"
#include "net/base/cookie_store.h"
#include "net/base/net_api.h"

class GURL;

namespace base {
class Histogram;
}

namespace net {

class CookieList;

// The cookie monster is the system for storing and retrieving cookies. It has
// an in-memory list of all cookies, and synchronizes non-session cookies to an
// optional permanent storage that implements the PersistentCookieStore
// interface.
//
// This class IS thread-safe. Normally, it is only used on the I/O thread, but
// is also accessed directly through Automation for UI testing.
//
// TODO(deanm) Implement CookieMonster, the cookie database.
//  - Verify that our domain enforcement and non-dotted handling is correct
class NET_API CookieMonster : public CookieStore {
 public:
  class CanonicalCookie;
  class Delegate;
  class ParsedCookie;
  class PersistentCookieStore;

  // Terminology:
  //    * The 'top level domain' (TLD) of an internet domain name is
  //      the terminal "." free substring (e.g. "com" for google.com
  //      or world.std.com).
  //    * The 'effective top level domain' (eTLD) is the longest
  //      "." initiated terminal substring of an internet domain name
  //      that is controlled by a general domain registrar.
  //      (e.g. "co.uk" for news.bbc.co.uk).
  //    * The 'effective top level domain plus one' (eTLD+1) is the
  //      shortest "." delimited terminal substring of an internet
  //      domain name that is not controlled by a general domain
  //      registrar (e.g. "bbc.co.uk" for news.bbc.co.uk, or
  //      "google.com" for news.google.com).  The general assumption
  //      is that all hosts and domains under an eTLD+1 share some
  //      administrative control.

  // CookieMap is the central data structure of the CookieMonster.  It
  // is a map whose values are pointers to CanonicalCookie data
  // structures (the data structures are owned by the CookieMonster
  // and must be destroyed when removed from the map).  There are two
  // possible keys for the map, controlled on a per-CookieMonster basis
  // by expiry_and_key_scheme_/SetExpiryAndKeyScheme()
  // (defaulted by expiry_and_key_default_):

  // If expiry_and_key_scheme_ is EKS_KEEP_RECENT_AND_PURGE_ETLDP1
  // (default), then the key is based on the effective domain of the
  // cookies.  If the domain of the cookie has an eTLD+1, that is the
  // key for the map.  If the domain of the cookie does not have an eTLD+1,
  // the key of the map is the host the cookie applies to (it is not
  // legal to have domain cookies without an eTLD+1).  This rule
  // excludes cookies for, e.g, ".com", ".co.uk", or ".internalnetwork".
  // This behavior is the same as the behavior in Firefox v 3.6.10.

  // If use_effective_domain_key_scheme_ is EKS_DISCARD_RECENT_AND_PURGE_DOMAIN,
  // then the key is just the domain of the cookie.  Eventually, this
  // option will be removed.

  // NOTE(deanm):
  // I benchmarked hash_multimap vs multimap.  We're going to be query-heavy
  // so it would seem like hashing would help.  However they were very
  // close, with multimap being a tiny bit faster.  I think this is because
  // our map is at max around 1000 entries, and the additional complexity
  // for the hashing might not overcome the O(log(1000)) for querying
  // a multimap.  Also, multimap is standard, another reason to use it.
  // TODO(rdsmith): This benchmark should be re-done now that we're allowing
  // subtantially more entries in the map.
  typedef std::multimap<std::string, CanonicalCookie*> CookieMap;
  typedef std::pair<CookieMap::iterator, CookieMap::iterator> CookieMapItPair;

  // The key and expiry scheme to be used by the monster.
  // EKS_KEEP_RECENT_AND_PURGE_ETLDP1 means to use
  // the new key scheme based on effective domain and save recent cookies
  // in global garbage collection.  EKS_DISCARD_RECENT_AND_PURGE_DOMAIN
  // means to use the old key scheme based on full domain and be ruthless
  // about purging.
  enum ExpiryAndKeyScheme {
    EKS_KEEP_RECENT_AND_PURGE_ETLDP1,
    EKS_DISCARD_RECENT_AND_PURGE_DOMAIN,
    EKS_LAST_ENTRY
  };

  // The store passed in should not have had Init() called on it yet. This
  // class will take care of initializing it. The backing store is NOT owned by
  // this class, but it must remain valid for the duration of the cookie
  // monster's existence. If |store| is NULL, then no backing store will be
  // updated. If |delegate| is non-NULL, it will be notified on
  // creation/deletion of cookies.
  CookieMonster(PersistentCookieStore* store, Delegate* delegate);

  // Only used during unit testing.
  CookieMonster(PersistentCookieStore* store,
                Delegate* delegate,
                int last_access_threshold_milliseconds);

  // Parses the string with the cookie time (very forgivingly).
  static base::Time ParseCookieTime(const std::string& time_string);

  // Returns true if a domain string represents a host-only cookie,
  // i.e. it doesn't begin with a leading '.' character.
  static bool DomainIsHostOnly(const std::string& domain_string);

  // Sets a cookie given explicit user-provided cookie attributes. The cookie
  // name, value, domain, etc. are each provided as separate strings. This
  // function expects each attribute to be well-formed. It will check for
  // disallowed characters (e.g. the ';' character is disallowed within the
  // cookie value attribute) and will return false without setting the cookie
  // if such characters are found.
  bool SetCookieWithDetails(const GURL& url,
                            const std::string& name,
                            const std::string& value,
                            const std::string& domain,
                            const std::string& path,
                            const base::Time& expiration_time,
                            bool secure, bool http_only);

  // Returns all the cookies, for use in management UI, etc. This does not mark
  // the cookies as having been accessed.
  // The returned cookies are ordered by longest path, then by earliest
  // creation date.
  CookieList GetAllCookies();

  // Returns all the cookies, for use in management UI, etc. Filters results
  // using given url scheme, host / domain and path and options. This does not
  // mark the cookies as having been accessed.
  // The returned cookies are ordered by longest path, then earliest
  // creation date.
  CookieList GetAllCookiesForURLWithOptions(const GURL& url,
                                            const CookieOptions& options);

  // Invokes GetAllCookiesForURLWithOptions with options set to include HTTP
  // only cookies.
  CookieList GetAllCookiesForURL(const GURL& url);

  // Deletes all of the cookies.
  int DeleteAll(bool sync_to_store);
  // Deletes all of the cookies that have a creation_date greater than or equal
  // to |delete_begin| and less than |delete_end|
  int DeleteAllCreatedBetween(const base::Time& delete_begin,
                              const base::Time& delete_end,
                              bool sync_to_store);
  // Deletes all of the cookies that have a creation_date more recent than the
  // one passed into the function via |delete_after|.
  int DeleteAllCreatedAfter(const base::Time& delete_begin, bool sync_to_store);

  // Deletes all cookies that match the host of the given URL
  // regardless of path.  This includes all http_only and secure cookies,
  // but does not include any domain cookies that may apply to this host.
  // Returns the number of cookies deleted.
  int DeleteAllForHost(const GURL& url);

  // Deletes one specific cookie.
  bool DeleteCanonicalCookie(const CanonicalCookie& cookie);

  // Override the default list of schemes that are allowed to be set in
  // this cookie store.  Calling his overrides the value of
  // "enable_file_scheme_".
  // If this this method is called, it must be called before first use of
  // the instance (i.e. as part of the instance initialization process).
  void SetCookieableSchemes(const char* schemes[], size_t num_schemes);

  // Overrides the default key and expiry scheme.  See comments
  // before CookieMap and Garbage collection constants for details.  This
  // function must be called before initialization.
  void SetExpiryAndKeyScheme(ExpiryAndKeyScheme key_scheme);

  // Instructs the cookie monster to not delete expired cookies. This is used
  // in cases where the cookie monster is used as a data structure to keep
  // arbitrary cookies.
  void SetKeepExpiredCookies();

  // Delegates the call to set the |clear_local_store_on_exit_| flag of the
  // PersistentStore if it exists.
  void SetClearPersistentStoreOnExit(bool clear_local_store);

  // There are some unknowns about how to correctly handle file:// cookies,
  // and our implementation for this is not robust enough. This allows you
  // to enable support, but it should only be used for testing. Bug 1157243.
  // Must be called before creating a CookieMonster instance.
  static void EnableFileScheme();

  // Flush the backing store (if any) to disk and post the given task when done.
  // WARNING: THE CALLBACK WILL RUN ON A RANDOM THREAD. IT MUST BE THREAD SAFE.
  // It may be posted to the current thread, or it may run on the thread that
  // actually does the flushing. Your Task should generally post a notification
  // to the thread you actually want to be notified on.
  void FlushStore(Task* completion_task);

  // CookieStore implementation.

  // Sets the cookies specified by |cookie_list| returned from |url|
  // with options |options| in effect.
  virtual bool SetCookieWithOptions(const GURL& url,
                                    const std::string& cookie_line,
                                    const CookieOptions& options);

  // Gets all cookies that apply to |url| given |options|.
  // The returned cookies are ordered by longest path, then earliest
  // creation date.
  virtual std::string GetCookiesWithOptions(const GURL& url,
                                            const CookieOptions& options);

  virtual void GetCookiesWithInfo(const GURL& url,
                                  const CookieOptions& options,
                                  std::string* cookie_line,
                                  std::vector<CookieInfo>* cookie_info);

  // Deletes all cookies with that might apply to |url| that has |cookie_name|.
  virtual void DeleteCookie(const GURL& url, const std::string& cookie_name);

  virtual CookieMonster* GetCookieMonster();

  // Debugging method to perform various validation checks on the map.
  // Currently just checking that there are no null CanonicalCookie pointers
  // in the map.
  // Argument |arg| is to allow retaining of arbitrary data if the CHECKs
  // in the function trip.  TODO(rdsmith):Remove hack.
  void ValidateMap(int arg);

  // The default list of schemes the cookie monster can handle.
  static const char* kDefaultCookieableSchemes[];
  static const int kDefaultCookieableSchemesCount;

 private:
  // Testing support.
  // For SetCookieWithCreationTime.
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest,
                           TestCookieDeleteAllCreatedAfterTimestamp);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest,
                           TestCookieDeleteAllCreatedBetweenTimestamps);

  // For gargage collection constants.
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestHostGarbageCollection);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestTotalGarbageCollection);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, GarbageCollectionTriggers);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestGCTimes);

  // For validation of key values.
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestDomainTree);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestImport);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, GetKey);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestGetKey);

  // Internal reasons for deletion, used to populate informative histograms
  // and to provide a public cause for onCookieChange notifications.
  //
  // If you add or remove causes from this list, please be sure to also update
  // the Delegate::ChangeCause mapping inside ChangeCauseMapping. Moreover,
  // these are used as array indexes, so avoid reordering to keep the
  // histogram buckets consistent. New items (if necessary) should be added
  // at the end of the list, just before DELETE_COOKIE_LAST_ENTRY.
  enum DeletionCause {
    DELETE_COOKIE_EXPLICIT = 0,
    DELETE_COOKIE_OVERWRITE,
    DELETE_COOKIE_EXPIRED,
    DELETE_COOKIE_EVICTED,
    DELETE_COOKIE_DUPLICATE_IN_BACKING_STORE,
    DELETE_COOKIE_DONT_RECORD,  // e.g. For final cleanup after flush to store.
    DELETE_COOKIE_EVICTED_DOMAIN,
    DELETE_COOKIE_EVICTED_GLOBAL,

    // Cookies evicted during domain level garbage collection that
    // were accessed longer ago than kSafeFromGlobalPurgeDays
    DELETE_COOKIE_EVICTED_DOMAIN_PRE_SAFE,

    // Cookies evicted during domain level garbage collection that
    // were accessed more recently than kSafeFromGlobalPurgeDays
    // (and thus would have been preserved by global garbage collection).
    DELETE_COOKIE_EVICTED_DOMAIN_POST_SAFE,

    // A common idiom is to remove a cookie by overwriting it with an
    // already-expired expiration date. This captures that case.
    DELETE_COOKIE_EXPIRED_OVERWRITE,

    DELETE_COOKIE_LAST_ENTRY
  };

  // Cookie garbage collection thresholds.  Based off of the Mozilla defaults.
  // When the number of cookies gets to k{Domain,}MaxCookies
  // purge down to k{Domain,}MaxCookies - k{Domain,}PurgeCookies.
  // It might seem scary to have a high purge value, but really it's not.
  // You just make sure that you increase the max to cover the increase
  // in purge, and we would have been purging the same amount of cookies.
  // We're just going through the garbage collection process less often.
  // Note that the DOMAIN values are per eTLD+1; see comment for the
  // CookieMap typedef.  So, e.g., the maximum number of cookies allowed for
  // google.com and all of its subdomains will be 150-180.
  //
  // If the expiry and key scheme follows firefox standards (default,
  // set by SetExpiryAndKeyScheme()), any cookies accessed more recently
  // than kSafeFromGlobalPurgeDays will not be evicted by global garbage
  // collection, even if we have more than kMaxCookies.  This does not affect
  // domain garbage collection.
  //
  // Present in .h file to make accessible to tests through FRIEND_TEST.
  // Actual definitions are in cookie_monster.cc.
  static const size_t kDomainMaxCookies;
  static const size_t kDomainPurgeCookies;
  static const size_t kMaxCookies;
  static const size_t kPurgeCookies;

  // The number of days since last access that cookies will not be subject
  // to global garbage collection.
  static const int kSafeFromGlobalPurgeDays;

  // Default value for key and expiry scheme scheme.
  static const ExpiryAndKeyScheme expiry_and_key_default_ =
      EKS_KEEP_RECENT_AND_PURGE_ETLDP1;

  // Record statistics every kRecordStatisticsIntervalSeconds of uptime.
  static const int kRecordStatisticsIntervalSeconds = 10 * 60;

  virtual ~CookieMonster();

  bool SetCookieWithCreationTime(const GURL& url,
                                 const std::string& cookie_line,
                                 const base::Time& creation_time);

  // Called by all non-static functions to ensure that the cookies store has
  // been initialized. This is not done during creating so it doesn't block
  // the window showing.
  // Note: this method should always be called with lock_ held.
  void InitIfNecessary() {
    if (!initialized_) {
      if (store_)
        InitStore();
      initialized_ = true;
    }
  }

  // Initializes the backing store and reads existing cookies from it.
  // Should only be called by InitIfNecessary().
  void InitStore();

  // Checks that |cookies_| matches our invariants, and tries to repair any
  // inconsistencies. (In other words, it does not have duplicate cookies).
  void EnsureCookiesMapIsValid();

  // Checks for any duplicate cookies for CookieMap key |key| which lie between
  // |begin| and |end|. If any are found, all but the most recent are deleted.
  // Returns the number of duplicate cookies that were deleted.
  int TrimDuplicateCookiesForKey(const std::string& key,
                                 CookieMap::iterator begin,
                                 CookieMap::iterator end);

  void SetDefaultCookieableSchemes();

  void FindCookiesForHostAndDomain(const GURL& url,
                                   const CookieOptions& options,
                                   bool update_access_time,
                                   std::vector<CanonicalCookie*>* cookies);

  void FindCookiesForKey(const std::string& key,
                         const GURL& url,
                         const CookieOptions& options,
                         const base::Time& current,
                         bool update_access_time,
                         std::vector<CanonicalCookie*>* cookies);

  // Delete any cookies that are equivalent to |ecc| (same path, domain, etc).
  // If |skip_httponly| is true, httponly cookies will not be deleted.  The
  // return value with be true if |skip_httponly| skipped an httponly cookie.
  // |key| is the key to find the cookie in cookies_; see the comment before
  // the CookieMap typedef for details.
  // NOTE: There should never be more than a single matching equivalent cookie.
  bool DeleteAnyEquivalentCookie(const std::string& key,
                                 const CanonicalCookie& ecc,
                                 bool skip_httponly,
                                 bool already_expired);

  // Takes ownership of *cc.
  void InternalInsertCookie(const std::string& key,
                            CanonicalCookie* cc,
                            bool sync_to_store);

  // Helper function that sets cookies with more control.
  // Not exposed as we don't want callers to have the ability
  // to specify (potentially duplicate) creation times.
  bool SetCookieWithCreationTimeAndOptions(const GURL& url,
                                           const std::string& cookie_line,
                                           const base::Time& creation_time,
                                           const CookieOptions& options);


  // Helper function that sets a canonical cookie, deleting equivalents and
  // performing garbage collection.
  bool SetCanonicalCookie(scoped_ptr<CanonicalCookie>* cc,
                          const base::Time& creation_time,
                          const CookieOptions& options);

  void InternalUpdateCookieAccessTime(CanonicalCookie* cc,
                                      const base::Time& current_time);

  // |deletion_cause| argument is used for collecting statistics and choosing
  // the correct Delegate::ChangeCause for OnCookieChanged notifications.
  void InternalDeleteCookie(CookieMap::iterator it, bool sync_to_store,
                            DeletionCause deletion_cause);

  // If the number of cookies for CookieMap key |key|, or globally, are
  // over the preset maximums above, garbage collect, first for the host and
  // then globally.  See comments above garbage collection threshold
  // constants for details.
  //
  // Returns the number of cookies deleted (useful for debugging).
  int GarbageCollect(const base::Time& current, const std::string& key);

  // Helper for GarbageCollect(); can be called directly as well.  Deletes
  // all expired cookies in |itpair|.  If |cookie_its| is non-NULL, it is
  // populated with all the non-expired cookies from |itpair|.
  //
  // Returns the number of cookies deleted.
  int GarbageCollectExpired(const base::Time& current,
                            const CookieMapItPair& itpair,
                            std::vector<CookieMap::iterator>* cookie_its);

  // Helper for GarbageCollect().  Deletes all cookies in the list
  // that were accessed before |keep_accessed_after|, using DeletionCause
  // |cause|.  If |keep_accessed_after| is null, deletes all cookies in the
  // list.  Returns the number of cookies deleted.
  int GarbageCollectDeleteList(const base::Time& current,
                               const base::Time& keep_accessed_after,
                               DeletionCause cause,
                               std::vector<CookieMap::iterator>& cookie_its);

  // Find the key (for lookup in cookies_) based on the given domain.
  // See comment on keys before the CookieMap typedef.
  std::string GetKey(const std::string& domain) const;

  bool HasCookieableScheme(const GURL& url);

  // Statistics support

  // This function should be called repeatedly, and will record
  // statistics if a sufficient time period has passed.
  void RecordPeriodicStats(const base::Time& current_time);

  // Initialize the above variables; should only be called from
  // the constructor.
  void InitializeHistograms();

  // The resolution of our time isn't enough, so we do something
  // ugly and increment when we've seen the same time twice.
  base::Time CurrentTime();

  // Histogram variables; see CookieMonster::InitializeHistograms() in
  // cookie_monster.cc for details.
  base::Histogram* histogram_expiration_duration_minutes_;
  base::Histogram* histogram_between_access_interval_minutes_;
  base::Histogram* histogram_evicted_last_access_minutes_;
  base::Histogram* histogram_count_;
  base::Histogram* histogram_domain_count_;
  base::Histogram* histogram_etldp1_count_;
  base::Histogram* histogram_domain_per_etldp1_count_;
  base::Histogram* histogram_number_duplicate_db_cookies_;
  base::Histogram* histogram_cookie_deletion_cause_;
  base::Histogram* histogram_time_get_;
  base::Histogram* histogram_time_mac_;
  base::Histogram* histogram_time_load_;

  CookieMap cookies_;

  // Indicates whether the cookie store has been initialized. This happens
  // lazily in InitStoreIfNecessary().
  bool initialized_;

  // Indicates whether this cookie monster uses the new effective domain
  // key scheme or not.
  ExpiryAndKeyScheme expiry_and_key_scheme_;

  scoped_refptr<PersistentCookieStore> store_;

  base::Time last_time_seen_;

  // Minimum delay after updating a cookie's LastAccessDate before we will
  // update it again.
  const base::TimeDelta last_access_threshold_;

  // Approximate date of access time of least recently accessed cookie
  // in |cookies_|.  Note that this is not guaranteed to be accurate, only a)
  // to be before or equal to the actual time, and b) to be accurate
  // immediately after a garbage collection that scans through all the cookies.
  // This value is used to determine whether global garbage collection might
  // find cookies to purge.
  // Note: The default Time() constructor will create a value that compares
  // earlier than any other time value, which is is wanted.  Thus this
  // value is not initialized.
  base::Time earliest_access_time_;

  std::vector<std::string> cookieable_schemes_;

  scoped_refptr<Delegate> delegate_;

  // Lock for thread-safety
  base::Lock lock_;

  base::Time last_statistic_record_time_;

  bool keep_expired_cookies_;

  static bool enable_file_scheme_;

  DISALLOW_COPY_AND_ASSIGN(CookieMonster);
};

class NET_API CookieMonster::CanonicalCookie {
 public:

  // These constructors do no validation or canonicalization of their inputs;
  // the resulting CanonicalCookies should not be relied on to be canonical
  // unless the caller has done appropriate validation and canonicalization
  // themselves.
  CanonicalCookie();
  CanonicalCookie(const GURL& url,
                  const std::string& name,
                  const std::string& value,
                  const std::string& domain,
                  const std::string& path,
                  const std::string& mac_key,
                  const std::string& mac_algorithm,
                  const base::Time& creation,
                  const base::Time& expiration,
                  const base::Time& last_access,
                  bool secure,
                  bool httponly,
                  bool has_expires);

  // This constructor does canonicalization but not validation.
  // The result of this constructor should not be relied on in contexts
  // in which pre-validation of the ParsedCookie has not been done.
  CanonicalCookie(const GURL& url, const ParsedCookie& pc);

  ~CanonicalCookie();

  // Supports the default copy constructor.

  // Creates a canonical cookie from unparsed attribute values.
  // Canonicalizes and validates inputs.  May return NULL if an attribute
  // value is invalid.
  static CanonicalCookie* Create(const GURL& url,
                                 const std::string& name,
                                 const std::string& value,
                                 const std::string& domain,
                                 const std::string& path,
                                 const std::string& mac_key,
                                 const std::string& mac_algorithm,
                                 const base::Time& creation,
                                 const base::Time& expiration,
                                 bool secure,
                                 bool http_only);

  const std::string& Source() const { return source_; }
  const std::string& Name() const { return name_; }
  const std::string& Value() const { return value_; }
  const std::string& Domain() const { return domain_; }
  const std::string& Path() const { return path_; }
  const std::string& MACKey() const { return mac_key_; }
  const std::string& MACAlgorithm() const { return mac_algorithm_; }
  const base::Time& CreationDate() const { return creation_date_; }
  const base::Time& LastAccessDate() const { return last_access_date_; }
  bool DoesExpire() const { return has_expires_; }
  bool IsPersistent() const { return DoesExpire(); }
  const base::Time& ExpiryDate() const { return expiry_date_; }
  bool IsSecure() const { return secure_; }
  bool IsHttpOnly() const { return httponly_; }
  bool IsDomainCookie() const {
    return !domain_.empty() && domain_[0] == '.'; }
  bool IsHostCookie() const { return !IsDomainCookie(); }

  bool IsExpired(const base::Time& current) {
    return has_expires_ && current >= expiry_date_;
  }

  // Are the cookies considered equivalent in the eyes of RFC 2965.
  // The RFC says that name must match (case-sensitive), domain must
  // match (case insensitive), and path must match (case sensitive).
  // For the case insensitive domain compare, we rely on the domain
  // having been canonicalized (in
  // GetCookieDomainWithString->CanonicalizeHost).
  bool IsEquivalent(const CanonicalCookie& ecc) const {
    // It seems like it would make sense to take secure and httponly into
    // account, but the RFC doesn't specify this.
    // NOTE: Keep this logic in-sync with TrimDuplicateCookiesForHost().
    return (name_ == ecc.Name() && domain_ == ecc.Domain()
            && path_ == ecc.Path());
  }

  void SetLastAccessDate(const base::Time& date) {
    last_access_date_ = date;
  }

  bool IsOnPath(const std::string& url_path) const;
  bool IsDomainMatch(const std::string& scheme, const std::string& host) const;

  std::string DebugString() const;

  // Returns the cookie source when cookies are set for |url|.  This function
  // is public for unit test purposes only.
  static std::string GetCookieSourceFromURL(const GURL& url);

 private:
  // The source member of a canonical cookie is the origin of the URL that tried
  // to set this cookie, minus the port number if any.  This field is not
  // persistent though; its only used in the in-tab cookies dialog to show the
  // user the source URL. This is used for both allowed and blocked cookies.
  // When a CanonicalCookie is constructed from the backing store (common case)
  // this field will be null.  CanonicalCookie consumers should not rely on
  // this field unless they guarantee that the creator of those
  // CanonicalCookies properly initialized the field.
  // TODO(abarth): We might need to make this field persistent for MAC cookies.
  std::string source_;
  std::string name_;
  std::string value_;
  std::string domain_;
  std::string path_;
  std::string mac_key_;  // TODO(abarth): Persist to disk.
  std::string mac_algorithm_;  // TODO(abarth): Persist to disk.
  base::Time creation_date_;
  base::Time expiry_date_;
  base::Time last_access_date_;
  bool secure_;
  bool httponly_;
  bool has_expires_;
};

class CookieMonster::Delegate
    : public base::RefCountedThreadSafe<CookieMonster::Delegate> {
 public:
  // The publicly relevant reasons a cookie might be changed.
  enum ChangeCause {
    // The cookie was changed directly by a consumer's action.
    CHANGE_COOKIE_EXPLICIT,
    // The cookie was automatically removed due to an insert operation that
    // overwrote it.
    CHANGE_COOKIE_OVERWRITE,
    // The cookie was automatically removed as it expired.
    CHANGE_COOKIE_EXPIRED,
    // The cookie was automatically evicted during garbage collection.
    CHANGE_COOKIE_EVICTED,
    // The cookie was overwritten with an already-expired expiration date.
    CHANGE_COOKIE_EXPIRED_OVERWRITE
  };

  // Will be called when a cookie is added or removed. The function is passed
  // the respective |cookie| which was added to or removed from the cookies.
  // If |removed| is true, the cookie was deleted, and |cause| will be set
  // to the reason for it's removal. If |removed| is false, the cookie was
  // added, and |cause| will be set to CHANGE_COOKIE_EXPLICIT.
  //
  // As a special case, note that updating a cookie's properties is implemented
  // as a two step process: the cookie to be updated is first removed entirely,
  // generating a notification with cause CHANGE_COOKIE_OVERWRITE.  Afterwards,
  // a new cookie is written with the updated values, generating a notification
  // with cause CHANGE_COOKIE_EXPLICIT.
  virtual void OnCookieChanged(const CookieMonster::CanonicalCookie& cookie,
                               bool removed,
                               ChangeCause cause) = 0;
 protected:
  friend class base::RefCountedThreadSafe<CookieMonster::Delegate>;
  virtual ~Delegate() {}
};

class NET_API CookieMonster::ParsedCookie {
 public:
  typedef std::pair<std::string, std::string> TokenValuePair;
  typedef std::vector<TokenValuePair> PairList;

  // The maximum length of a cookie string we will try to parse
  static const size_t kMaxCookieSize = 4096;
  // The maximum number of Token/Value pairs.  Shouldn't have more than 8.
  static const int kMaxPairs = 16;

  // Construct from a cookie string like "BLAH=1; path=/; domain=.google.com"
  ParsedCookie(const std::string& cookie_line);
  ~ParsedCookie();

  // You should not call any other methods on the class if !IsValid
  bool IsValid() const { return is_valid_; }

  const std::string& Name() const { return pairs_[0].first; }
  const std::string& Token() const { return Name(); }
  const std::string& Value() const { return pairs_[0].second; }

  bool HasPath() const { return path_index_ != 0; }
  const std::string& Path() const { return pairs_[path_index_].second; }
  bool HasDomain() const { return domain_index_ != 0; }
  const std::string& Domain() const { return pairs_[domain_index_].second; }
  bool HasMACKey() const { return mac_key_index_ != 0; }
  const std::string& MACKey() const { return pairs_[mac_key_index_].second; }
  bool HasMACAlgorithm() const { return mac_algorithm_index_ != 0; }
  const std::string& MACAlgorithm() const {
    return pairs_[mac_algorithm_index_].second;
  }
  bool HasExpires() const { return expires_index_ != 0; }
  const std::string& Expires() const { return pairs_[expires_index_].second; }
  bool HasMaxAge() const { return maxage_index_ != 0; }
  const std::string& MaxAge() const { return pairs_[maxage_index_].second; }
  bool IsSecure() const { return secure_index_ != 0; }
  bool IsHttpOnly() const { return httponly_index_ != 0; }

  // Returns the number of attributes, for example, returning 2 for:
  //   "BLAH=hah; path=/; domain=.google.com"
  size_t NumberOfAttributes() const { return pairs_.size() - 1; }

  // For debugging only!
  std::string DebugString() const;

  // Returns an iterator pointing to the first terminator character found in
  // the given string.
  static std::string::const_iterator FindFirstTerminator(const std::string& s);

  // Given iterators pointing to the beginning and end of a string segment,
  // returns as output arguments token_start and token_end to the start and end
  // positions of a cookie attribute token name parsed from the segment, and
  // updates the segment iterator to point to the next segment to be parsed.
  // If no token is found, the function returns false.
  static bool ParseToken(std::string::const_iterator* it,
                         const std::string::const_iterator& end,
                         std::string::const_iterator* token_start,
                         std::string::const_iterator* token_end);

  // Given iterators pointing to the beginning and end of a string segment,
  // returns as output arguments value_start and value_end to the start and end
  // positions of a cookie attribute value parsed from the segment, and updates
  // the segment iterator to point to the next segment to be parsed.
  static void ParseValue(std::string::const_iterator* it,
                         const std::string::const_iterator& end,
                         std::string::const_iterator* value_start,
                         std::string::const_iterator* value_end);

  // Same as the above functions, except the input is assumed to contain the
  // desired token/value and nothing else.
  static std::string ParseTokenString(const std::string& token);
  static std::string ParseValueString(const std::string& value);

 private:
  static const char kTerminator[];
  static const int  kTerminatorLen;
  static const char kWhitespace[];
  static const char kValueSeparator[];
  static const char kTokenSeparator[];

  void ParseTokenValuePairs(const std::string& cookie_line);
  void SetupAttributes();

  PairList pairs_;
  bool is_valid_;
  // These will default to 0, but that should never be valid since the
  // 0th index is the user supplied token/value, not an attribute.
  // We're really never going to have more than like 8 attributes, so we
  // could fit these into 3 bits each if we're worried about size...
  size_t path_index_;
  size_t domain_index_;
  size_t mac_key_index_;
  size_t mac_algorithm_index_;
  size_t expires_index_;
  size_t maxage_index_;
  size_t secure_index_;
  size_t httponly_index_;

  DISALLOW_COPY_AND_ASSIGN(ParsedCookie);
};

typedef base::RefCountedThreadSafe<CookieMonster::PersistentCookieStore>
    RefcountedPersistentCookieStore;

class CookieMonster::PersistentCookieStore
    : public RefcountedPersistentCookieStore {
 public:
  virtual ~PersistentCookieStore() {}

  // Initializes the store and retrieves the existing cookies. This will be
  // called only once at startup.
  virtual bool Load(std::vector<CookieMonster::CanonicalCookie*>* cookies) = 0;

  virtual void AddCookie(const CanonicalCookie& cc) = 0;
  virtual void UpdateCookieAccessTime(const CanonicalCookie& cc) = 0;
  virtual void DeleteCookie(const CanonicalCookie& cc) = 0;

  // Sets the value of the user preference whether the persistent storage
  // must be deleted upon destruction.
  virtual void SetClearLocalStateOnExit(bool clear_local_state) = 0;

  // Flush the store and post the given Task when complete.
  virtual void Flush(Task* completion_task) = 0;

 protected:
  PersistentCookieStore() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistentCookieStore);
};

class CookieList : public std::vector<CookieMonster::CanonicalCookie> {
};

}  // namespace net

#endif  // NET_BASE_COOKIE_MONSTER_H_
