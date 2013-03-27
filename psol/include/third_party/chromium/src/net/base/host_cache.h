// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HOST_CACHE_H_
#define NET_BASE_HOST_CACHE_H_

#include <functional>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/expiring_cache.h"
#include "net/base/net_export.h"

namespace net {

// Cache used by HostResolver to map hostnames to their resolved result.
class NET_EXPORT HostCache : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // Stores the latest address list that was looked up for a hostname.
  struct Entry {
    Entry(int error, const AddressList& addrlist);
    ~Entry();

    // The resolve results for this entry.
    int error;
    AddressList addrlist;
  };

  struct Key {
    Key(const std::string& hostname, AddressFamily address_family,
        HostResolverFlags host_resolver_flags)
        : hostname(hostname),
          address_family(address_family),
          host_resolver_flags(host_resolver_flags) {}

    bool operator<(const Key& other) const {
      // |address_family| and |host_resolver_flags| are compared before
      // |hostname| under assumption that integer comparisons are faster than
      // string comparisons.
      if (address_family != other.address_family)
        return address_family < other.address_family;
      if (host_resolver_flags != other.host_resolver_flags)
        return host_resolver_flags < other.host_resolver_flags;
      return hostname < other.hostname;
    }

    std::string hostname;
    AddressFamily address_family;
    HostResolverFlags host_resolver_flags;
  };

  typedef ExpiringCache<Key, Entry, base::TimeTicks,
                        std::less<base::TimeTicks> > EntryMap;

  // Constructs a HostCache that stores up to |max_entries|.
  explicit HostCache(size_t max_entries);

  ~HostCache();

  // Returns a pointer to the entry for |key|, which is valid at time
  // |now|. If there is no such entry, returns NULL.
  const Entry* Lookup(const Key& key, base::TimeTicks now);

  // Overwrites or creates an entry for |key|.
  // (|error|, |addrlist|) is the value to set, |now| is the current time
  // |ttl| is the "time to live".
  void Set(const Key& key,
           int error,
           const AddressList& addrlist,
           base::TimeTicks now,
           base::TimeDelta ttl);

  // Empties the cache
  void clear();

  // Returns the number of entries in the cache.
  size_t size() const;

  // Following are used by net_internals UI.
  size_t max_entries() const;

  const EntryMap& entries() const;

  // Creates a default cache.
  static HostCache* CreateDefaultCache();

 private:
  FRIEND_TEST_ALL_PREFIXES(HostCacheTest, NoCache);

  // Returns true if this HostCache can contain no entries.
  bool caching_is_disabled() const {
    return entries_.max_entries() == 0;
  }

  // Map from hostname (presumably in lowercase canonicalized format) to
  // a resolved result entry.
  EntryMap entries_;

  DISALLOW_COPY_AND_ASSIGN(HostCache);
};

}  // namespace net

#endif  // NET_BASE_HOST_CACHE_H_
