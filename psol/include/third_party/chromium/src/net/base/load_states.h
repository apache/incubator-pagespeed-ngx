// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_STATES_H__
#define NET_BASE_LOAD_STATES_H__
#pragma once

namespace net {

// These states correspond to the lengthy periods of time that a resource load
// may be blocked and unable to make progress.
enum LoadState {
  // This is the default state.  It corresponds to a resource load that has
  // either not yet begun or is idle waiting for the consumer to do something
  // to move things along (e.g., the consumer of an URLRequest may not have
  // called Read yet).
  LOAD_STATE_IDLE,

  // This state corresponds to a resource load that is blocked waiting for
  // access to a resource in the cache.  If multiple requests are made for the
  // same resource, the first request will be responsible for writing (or
  // updating) the cache entry and the second request will be deferred until
  // the first completes.  This may be done to optimize for cache reuse.
  LOAD_STATE_WAITING_FOR_CACHE,

  // This state corresponds to a resource load that is blocked waiting for a
  // proxy autoconfig script to return a proxy server to use.  This state may
  // take a while if the proxy script needs to resolve the IP address of the
  // host before deciding what proxy to use.
  LOAD_STATE_RESOLVING_PROXY_FOR_URL,

  // This state indicates that we're in the process of establishing a tunnel
  // through the proxy server.
  LOAD_STATE_ESTABLISHING_PROXY_TUNNEL,

  // This state corresponds to a resource load that is blocked waiting for a
  // host name to be resolved.  This could either indicate resolution of the
  // origin server corresponding to the resource or to the host name of a proxy
  // server used to fetch the resource.
  LOAD_STATE_RESOLVING_HOST,

  // This state corresponds to a resource load that is blocked waiting for a
  // TCP connection (or other network connection) to be established.  HTTP
  // requests that reuse a keep-alive connection skip this state.
  LOAD_STATE_CONNECTING,

  // This state corresponds to a resource load that is blocked waiting for the
  // SSL handshake to complete.
  LOAD_STATE_SSL_HANDSHAKE,

  // This state corresponds to a resource load that is blocked waiting to
  // completely upload a request to a server.  In the case of a HTTP POST
  // request, this state includes the period of time during which the message
  // body is being uploaded.
  LOAD_STATE_SENDING_REQUEST,

  // This state corresponds to a resource load that is blocked waiting for the
  // response to a network request.  In the case of a HTTP transaction, this
  // corresponds to the period after the request is sent and before all of the
  // response headers have been received.
  LOAD_STATE_WAITING_FOR_RESPONSE,

  // This state corresponds to a resource load that is blocked waiting for a
  // read to complete.  In the case of a HTTP transaction, this corresponds to
  // the period after the response headers have been received and before all of
  // the response body has been downloaded.  (NOTE: This state only applies for
  // an URLRequest while there is an outstanding Read operation.)
  LOAD_STATE_READING_RESPONSE,
};

}  // namespace net

#endif  // NET_BASE_LOAD_STATES_H__
