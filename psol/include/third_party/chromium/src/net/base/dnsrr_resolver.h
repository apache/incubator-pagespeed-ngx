// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DNSRR_RESOLVER_H_
#define NET_BASE_DNSRR_RESOLVER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "build/build_config.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace net {

// RRResponse contains the result of a successful request for a resource record.
struct NET_EXPORT_PRIVATE RRResponse {
  RRResponse();
  ~RRResponse();

  // HasExpired returns true if |fetch_time| + |ttl| is less than
  // |current_time|.
  bool HasExpired(base::Time current_time) const;

#if defined(OS_POSIX) && !defined(OS_ANDROID)
  // For testing only
  bool ParseFromResponse(const uint8* data, unsigned len,
                         uint16 rrtype_requested);
#endif

  // name contains the canonical name of the resulting domain. If the queried
  // name was a CNAME then this can differ.
  std::string name;
  // ttl contains the TTL of the resource records.
  uint32 ttl;
  // dnssec is true if the response was DNSSEC validated.
  bool dnssec;
  std::vector<std::string> rrdatas;
  // sigs contains the RRSIG records returned.
  std::vector<std::string> signatures;
  // fetch_time is the time at which the response was received from the
  // network.
  base::Time fetch_time;
  // negative is true if this is a negative cache entry, i.e. is a placeholder
  // to remember that a given RR doesn't exist.
  bool negative;
};

class BoundNetLog;
class RRResolverWorker;
class RRResolverJob;

// DnsRRResolver resolves arbitary DNS resource record types. It should not be
// confused with HostResolver and should not be used to resolve A/AAAA records.
//
// HostResolver exists to lookup addresses and there are many details about
// address resolution over and above DNS (i.e. Bonjour, VPNs etc).
//
// DnsRRResolver should only be used when the data is specifically DNS data and
// the name is a fully qualified DNS domain.
//
// A DnsRRResolver must be used from the MessageLoop which created it.
class NET_EXPORT DnsRRResolver
    : NON_EXPORTED_BASE(public base::NonThreadSafe),
      public NetworkChangeNotifier::IPAddressObserver {
 public:
  typedef intptr_t Handle;

  enum {
    kInvalidHandle = 0,
  };

  enum {
    // Try harder to get a DNSSEC signed response. This doesn't mean that the
    // RRResponse will always have the dnssec bit set.
    FLAG_WANT_DNSSEC = 1,
  };

  DnsRRResolver();
  virtual ~DnsRRResolver();

  uint64 requests() const { return requests_; }
  uint64 cache_hits() const { return cache_hits_; }
  uint64 inflight_joins() const { return inflight_joins_; }

  // Resolve starts the resolution process. When complete, |callback| is called
  // with a result. If the result is |OK| then |response| is filled with the
  // result of the resolution. Note that |callback| is called via the current
  // MessageLoop.
  //
  // This returns a handle value which can be passed to |CancelResolve|. If
  // this function returns kInvalidHandle then the resolution failed
  // immediately because it was improperly formed.
  Handle Resolve(const std::string& name, uint16 rrtype,
                 uint16 flags, const CompletionCallback& callback,
                 RRResponse* response, int priority,
                 const BoundNetLog& netlog);

  // CancelResolve cancels an inflight lookup. The callback for this lookup
  // must not have already been called.
  void CancelResolve(Handle handle);

  // Implementation of NetworkChangeNotifier::IPAddressObserver
  virtual void OnIPAddressChanged() OVERRIDE;

 private:
  friend class RRResolverWorker;

  void HandleResult(const std::string& name, uint16 rrtype, int result,
                    const RRResponse& response);

  // cache_ maps from a request to a cached response. The cached answer may
  // have expired and the size of |cache_| must be <= kMaxCacheEntries.
  //                < name      , rrtype>
  std::map<std::pair<std::string, uint16>, RRResponse> cache_;
  // inflight_ maps from a request to an active resolution which is taking
  // place.
  std::map<std::pair<std::string, uint16>, RRResolverJob*> inflight_;

  uint64 requests_;
  uint64 cache_hits_;
  uint64 inflight_joins_;

  bool in_destructor_;

  DISALLOW_COPY_AND_ASSIGN(DnsRRResolver);
};

}  // namespace net

#endif  // NET_BASE_DNSRR_RESOLVER_H_
