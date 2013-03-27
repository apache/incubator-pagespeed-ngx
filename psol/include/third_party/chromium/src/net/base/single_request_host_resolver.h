// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SINGLE_REQUEST_HOST_RESOLVER_H_
#define NET_BASE_SINGLE_REQUEST_HOST_RESOLVER_H_

#include "net/base/host_resolver.h"

namespace net {

// This class represents the task of resolving a hostname (or IP address
// literal) to an AddressList object.  It wraps HostResolver to resolve only a
// single hostname at a time and cancels this request when going out of scope.
class NET_EXPORT SingleRequestHostResolver {
 public:
  // |resolver| must remain valid for the lifetime of |this|.
  explicit SingleRequestHostResolver(HostResolver* resolver);

  // If a completion callback is pending when the resolver is destroyed, the
  // host resolution is cancelled, and the completion callback will not be
  // called.
  ~SingleRequestHostResolver();

  // Resolves the given hostname (or IP address literal), filling out the
  // |addresses| object upon success. See HostResolver::Resolve() for details.
  int Resolve(const HostResolver::RequestInfo& info,
              AddressList* addresses,
              const CompletionCallback& callback,
              const BoundNetLog& net_log);

  // Cancels the in-progress request, if any. This prevents the callback
  // from being invoked. Resolve() can be called again after cancelling.
  void Cancel();

 private:
  // Callback for when the request to |resolver_| completes, so we dispatch
  // to the user's callback.
  void OnResolveCompletion(int result);

  // The actual host resolver that will handle the request.
  HostResolver* const resolver_;

  // The current request (if any).
  HostResolver::RequestHandle cur_request_;
  CompletionCallback cur_request_callback_;

  // Completion callback for when request to |resolver_| completes.
  CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SingleRequestHostResolver);
};

}  // namespace net

#endif  // NET_BASE_SINGLE_REQUEST_HOST_RESOLVER_H_
