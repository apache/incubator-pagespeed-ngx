// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HOST_RESOLVER_H_
#define NET_BASE_HOST_RESOLVER_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/base/address_family.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_api.h"
#include "net/base/request_priority.h"

namespace net {

class AddressList;
class BoundNetLog;
class HostResolverImpl;
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
class NET_API HostResolver {
 public:
  // The parameters for doing a Resolve(). A hostname and port are required,
  // the rest are optional (and have reasonable defaults).
  class NET_API RequestInfo {
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

    bool only_use_cached_response() const { return only_use_cached_response_; }
    void set_only_use_cached_response(bool b) { only_use_cached_response_ = b; }

    bool is_speculative() const { return is_speculative_; }
    void set_is_speculative(bool b) { is_speculative_ = b; }

    RequestPriority priority() const { return priority_; }
    void set_priority(RequestPriority priority) { priority_ = priority; }

    const GURL& referrer() const { return referrer_; }
    void set_referrer(const GURL& referrer) { referrer_ = referrer; }

   private:
    // The hostname to resolve, and the port to use in resulting sockaddrs.
    HostPortPair host_port_pair_;

    // The address family to restrict results to.
    AddressFamily address_family_;

    // Flags to use when resolving this request.
    HostResolverFlags host_resolver_flags_;

    // Whether it is ok to return a result from the host cache.
    bool allow_cached_response_;

    // Whether the response will only use the cache.
    bool only_use_cached_response_;

    // Whether this request was started by the DNS prefetcher.
    bool is_speculative_;

    // The priority for the request.
    RequestPriority priority_;

    // Optional data for consumption by observers. This is the URL of the
    // page that lead us to the navigation, for DNS prefetcher's benefit.
    GURL referrer_;
  };

  // Interface for observing the requests that flow through a HostResolver.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called at the start of HostResolver::Resolve(). |id| is a unique number
    // given to the request, so it can be matched up with a corresponding call
    // to OnFinishResolutionWithStatus() or OnCancelResolution().
    virtual void OnStartResolution(int id, const RequestInfo& info) = 0;

    // Called on completion of request |id|. Note that if the request was
    // cancelled, OnCancelResolution() will be called instead.
    virtual void OnFinishResolutionWithStatus(int id, bool was_resolved,
                                              const RequestInfo& info) = 0;

    // Called when request |id| has been cancelled. A request is "cancelled"
    // if either the HostResolver is destroyed while a resolution is in
    // progress, or HostResolver::CancelRequest() is called.
    virtual void OnCancelResolution(int id, const RequestInfo& info) = 0;
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
  // successful or an error code upon failure.
  //
  // When callback is null, the operation completes synchronously.
  //
  // When callback is non-null, the operation may be performed asynchronously.
  // If the operation cannnot be completed synchronously, ERR_IO_PENDING will
  // be returned and the real result code will be passed to the completion
  // callback.  Otherwise the result code is returned immediately from this
  // call.
  // If |out_req| is non-NULL, then |*out_req| will be filled with a handle to
  // the async request. This handle is not valid after the request has
  // completed.
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  virtual int Resolve(const RequestInfo& info,
                      AddressList* addresses,
                      CompletionCallback* callback,
                      RequestHandle* out_req,
                      const BoundNetLog& net_log) = 0;

  // Cancels the specified request. |req| is the handle returned by Resolve().
  // After a request is cancelled, its completion callback will not be called.
  virtual void CancelRequest(RequestHandle req) = 0;

  // Adds an observer to this resolver. The observer will be notified of the
  // start and completion of all requests (excluding cancellation). |observer|
  // must remain valid for the duration of this HostResolver's lifetime.
  virtual void AddObserver(Observer* observer) = 0;

  // Unregisters an observer previously added by AddObserver().
  virtual void RemoveObserver(Observer* observer) = 0;

  // Sets the default AddressFamily to use when requests have left it
  // unspecified. For example, this could be used to restrict resolution
  // results to AF_INET by passing in ADDRESS_FAMILY_IPV4, or to
  // AF_INET6 by passing in ADDRESS_FAMILY_IPV6.
  virtual void SetDefaultAddressFamily(AddressFamily address_family) {}
  virtual AddressFamily GetDefaultAddressFamily() const;

  // Returns |this| cast to a HostResolverImpl*, or NULL if the subclass
  // is not compatible with HostResolverImpl. Used primarily to expose
  // additional functionality on the about:net-internals page.
  virtual HostResolverImpl* GetAsHostResolverImpl();

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
NET_API HostResolver* CreateSystemHostResolver(size_t max_concurrent_resolves,
                                               size_t max_retry_attempts,
                                               NetLog* net_log);

}  // namespace net

#endif  // NET_BASE_HOST_RESOLVER_H_
