// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_DELEGATE_H_
#define NET_BASE_NETWORK_DELEGATE_H_
#pragma once

#include "base/string16.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"

class GURL;

namespace net {

// NOTE: Layering violations!
// We decided to accept these violations (depending
// on other net/ submodules from net/base/), because otherwise NetworkDelegate
// would have to be broken up into too many smaller interfaces targeted to each
// submodule. Also, since the lower levels in net/ may callback into higher
// levels, we may encounter dangerous casting issues.
//
// NOTE: It is not okay to add any compile-time dependencies on symbols outside
// of net/base here, because we have a net_base library. Forward declarations
// are ok.
class HostPortPair;
class HttpRequestHeaders;
class URLRequest;
class URLRequestJob;

class NetworkDelegate : public base::NonThreadSafe {
 public:
  virtual ~NetworkDelegate() {}

  // Notification interface called by the network stack. Note that these
  // functions mostly forward to the private virtuals. They also add some sanity
  // checking on parameters. See the corresponding virtuals for explanations of
  // the methods and their arguments.
  int NotifyBeforeURLRequest(URLRequest* request,
                             CompletionCallback* callback,
                             GURL* new_url);
  int NotifyBeforeSendHeaders(uint64 request_id,
                              CompletionCallback* callback,
                              HttpRequestHeaders* headers);
  void NotifyRequestSent(uint64 request_id,
                         const HostPortPair& socket_address,
                         const HttpRequestHeaders& headers);
  void NotifyBeforeRedirect(URLRequest* request,
                            const GURL& new_location);
  void NotifyResponseStarted(URLRequest* request);
  void NotifyRawBytesRead(const URLRequest& request, int bytes_read);
  void NotifyCompleted(URLRequest* request);
  void NotifyURLRequestDestroyed(URLRequest* request);
  void NotifyHttpTransactionDestroyed(uint64 request_id);
  void NotifyPACScriptError(int line_number, const string16& error);

 private:
  // This is the interface for subclasses of NetworkDelegate to implement. This
  // member functions will be called by the respective public notification
  // member function, which will perform basic sanity checking.

  // Called before a request is sent. Allows the delegate to rewrite the URL
  // being fetched by modifying |new_url|. |callback| and |new_url| are valid
  // only until OnURLRequestDestroyed is called for this request. Returns a net
  // status code, generally either OK to continue with the request or
  // ERR_IO_PENDING if the result is not ready yet.
  virtual int OnBeforeURLRequest(URLRequest* request,
                                 CompletionCallback* callback,
                                 GURL* new_url) = 0;

  // Called right before the HTTP headers are sent. Allows the delegate to
  // read/write |headers| before they get sent out. |callback| and |headers| are
  // valid only until OnHttpTransactionDestroyed is called for this request.
  // Returns a net status code.
  virtual int OnBeforeSendHeaders(uint64 request_id,
                                  CompletionCallback* callback,
                                  HttpRequestHeaders* headers) = 0;

  // Called right after the HTTP headers have been sent and notifies where
  // the request has actually been sent to.
  virtual void OnRequestSent(uint64 request_id,
                             const HostPortPair& socket_address,
                             const HttpRequestHeaders& headers) = 0;

  // Called right after a redirect response code was received.
  virtual void OnBeforeRedirect(URLRequest* request,
                                const GURL& new_location) = 0;

  // This corresponds to URLRequestDelegate::OnResponseStarted.
  virtual void OnResponseStarted(URLRequest* request) = 0;

  // Called every time we read raw bytes.
  virtual void OnRawBytesRead(const URLRequest& request, int bytes_read) = 0;

  // Indicates that the URL request has been completed or failed.
  virtual void OnCompleted(URLRequest* request) = 0;

  // Called when an URLRequest is being destroyed. Note that the request is
  // being deleted, so it's not safe to call any methods that may result in
  // a virtual method call.
  virtual void OnURLRequestDestroyed(URLRequest* request) = 0;

  // Called when the HttpTransaction for the request with the given ID is
  // destroyed.
  virtual void OnHttpTransactionDestroyed(uint64 request_id) = 0;

  // Corresponds to ProxyResolverJSBindings::OnError.
  virtual void OnPACScriptError(int line_number, const string16& error) = 0;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_DELEGATE_H_
