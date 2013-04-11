// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HOST_RESOLVER_H_
#define NET_BASE_HOST_RESOLVER_H_

#include <string>

#include "net/base/address_family.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/net_util.h"
#include "net/base/request_priority.h"

namespace base {
class Value;
}

namespace net {

class AddressList;
class BoundNetLog;
class HostCache;
class HostResolverProc;
class NetLog;

// This class represents the task of resolving hostnames (or IP address
// literal) to an AddressList object.
//
// HostResolver can handle multiple requests at a time, so when cancelling a
// request the RequestHandle that was returned by Resolve() needs to be
// given.  A simpler alternative for consumers that only have 1 outstanding
// request at a time is to create a SingleRequestHostResolver wrapper around
// HostResolver (which will automatically cancel the single request when it
// goes out of scope).
class NET_EXPORT HostResolver {
 public:
  // The parameters for doing a Resolve(). A hostname and port are required,
  // the rest are optional (and have reasonable defaults).
  class NET_EXPORT RequestInfo {
   public:
    explicit RequestInfo(const HostPortPair& host_port_pair);

    const HostPortPair& host_port_pair() const { return host_port_pair_; }
    void set_host_port_pair(const HostPortPair& host_port_pair) {
      host_port_pair_ = host_port_pair;
    }

    int port() const { return host_port_pair_.port(); }
    const std::string& hostname() const { return host_port_pair_.host(); }

    AddressFamily address_family() const { return address_family_; }
    void set_address_family(AddressFamily address_family) {
      address_family_ = address_family;
    }

    HostResolverFlags host_resolver_flags() const {
      return host_resolver_flags_;
    }
    void set_host_resolver_flags(HostResolverFlags host_resolver_flags) {
      host_resolver_flags_ = host_resolver_flags;
    }

    bool allow_cached_response() const { return allow_cached_response_; }
    void set_allow_cached_response(bool b) { allow_cached_response_ = b; }

    bool is_speculative() const { return is_speculative_; }
    void set_is_speculative(bool b) { is_speculative_ = b; }

    RequestPriority priority() const { return priority_; }
    void set_priority(RequestPriority priority) { priority_ = priority; }

   private:
    // The hostname to resolve, and the port to use in resulting sockaddrs.
    HostPortPair host_port_pair_;

    // The address family to restrict results to.
    AddressFamily address_family_;

    // Flags to use when resolving this request.
    HostResolverFlags host_resolver_flags_;

    // Whether it is ok to return a result from the host cache.
    bool allow_cached_response_;

    // Whether this request was started by the DNS prefetcher.
    bool is_speculative_;

    // The priority for the request.
    RequestPriority priority_;
  };

  // Opaque type used to cancel a request.
  typedef void* RequestHandle;

  // This value can be passed into CreateSystemHostResolver as the
  // |max_concurrent_resolves| parameter. It will select a default level of
  // concurrency.
  static const size_t kDefaultParallelism = 0;

  // This value can be passed into CreateSystemHostResolver as the
  // |max_retry_attempts| parameter. This is the maximum number of times we
  // will retry for host resolution.
  static const size_t kDefaultRetryAttempts = -1;

  // If any completion callbacks are pending when the resolver is destroyed,
  // the host resolutions are cancelled, and the completion callbacks will not
  // be called.
  virtual ~HostResolver();

  // Resolves the given hostname (or IP address literal), filling out the
  // |addresses| object upon success.  The |info.port| parameter will be set as
  // the sin(6)_port field of the sockaddr_in{6} struct.  Returns OK if
  // successful or an error code upon failure.  Returns
  // ERR_NAME_NOT_RESOLVED if hostname is invalid, or if it is an
  // incompatible IP literal (e.g. IPv6 is disabled and it is an IPv6
  // literal).
  //
  // If the operation cannot be completed synchronously, ERR_IO_PENDING will
  // be returned and the real result code will be passed to the completion
  // callback.  Otherwise the result code is returned immediately from this
  // call.
  //
  // If |out_req| is non-NULL, then |*out_req| will be filled with a handle to
  // the async request. This handle is not valid after the request has
  // completed.
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  virtual int Resolve(const RequestInfo& info,
                      AddressList* addresses,
                      const CompletionCallback& callback,
                      RequestHandle* out_req,
                      const BoundNetLog& net_log) = 0;

  // Resolves the given hostname (or IP address literal) out of cache or HOSTS
  // file (if enabled) only. This is guaranteed to complete synchronously.
  // This acts like |Resolve()| if the hostname is IP literal, or cached value
  // or HOSTS entry exists. Otherwise, ERR_DNS_CACHE_MISS is returned.
  virtual int ResolveFromCache(const RequestInfo& info,
                               AddressList* addresses,
                               const BoundNetLog& net_log) = 0;

  // Cancels the specified request. |req| is the handle returned by Resolve().
  // After a request is canceled, its completion callback will not be called.
  // CancelRequest must NOT be called after the request's completion callback
  // has already run or the request was canceled.
  virtual void CancelRequest(RequestHandle req) = 0;

  // Sets the default AddressFamily to use when requests have left it
  // unspecified. For example, this could be used to restrict resolution
  // results to AF_INET by passing in ADDRESS_FAMILY_IPV4, or to
  // AF_INET6 by passing in ADDRESS_FAMILY_IPV6.
  virtual void SetDefaultAddressFamily(AddressFamily address_family) {}
  virtual AddressFamily GetDefaultAddressFamily() const;

  // Continuously observe whether IPv6 is supported, and set the allowable
  // address family to IPv4 iff IPv6 is not supported.
  virtual void ProbeIPv6Support();

  // Returns the HostResolverCache |this| uses, or NULL if there isn't one.
  // Used primarily to clear the cache and for getting debug information.
  virtual HostCache* GetHostCache();

  // Returns the current DNS configuration |this| is using, as a Value, or NULL
  // if it's configured to always use the system host resolver.  Caller takes
  // ownership of the returned Value.
  virtual base::Value* GetDnsConfigAsValue() const;

 protected:
  HostResolver();

 private:
  DISALLOW_COPY_AND_ASSIGN(HostResolver);
};

// Creates a HostResolver implementation that queries the underlying system.
// (Except if a unit-test has changed the global HostResolverProc using
// ScopedHostResolverProc to intercept requests to the system).
// |max_concurrent_resolves| is how many resolve requests will be allowed to
// run in parallel. Pass HostResolver::kDefaultParallelism to choose a
// default value.
// |max_retry_attempts| is the maximum number of times we will retry for host
// resolution. Pass HostResolver::kDefaultRetryAttempts to choose a default
// value.
// The created HostResolver uses an instance of DnsConfigService to retrieve
// system DNS configuration.
// This resolver should not be used in test context. Instead, use
// MockHostResolver from net/base/mock_host_resolver.h.
NET_EXPORT HostResolver* CreateSystemHostResolver(
    size_t max_concurrent_resolves,
    size_t max_retry_attempts,
    NetLog* net_log);

// As above, but the created HostResolver does not use a cache.
NET_EXPORT HostResolver* CreateNonCachingSystemHostResolver(
    size_t max_concurrent_resolves,
    size_t max_retry_attempts,
    NetLog* net_log);

// As above, but the HostResolver will use the asynchronous DNS client in
// DnsTransaction, which will be configured using DnsConfigService to match
// the system DNS settings. If the client fails, the resolver falls back to
// the global HostResolverProc.
NET_EXPORT HostResolver* CreateAsyncHostResolver(size_t max_concurrent_resolves,
                                                 size_t max_retry_attempts,
                                                 NetLog* net_log);
}  // namespace net

#endif  // NET_BASE_HOST_RESOLVER_H_
