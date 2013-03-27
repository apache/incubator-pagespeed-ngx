// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_DELEGATE_H_
#define NET_BASE_NETWORK_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/string16.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/auth.h"
#include "net/base/completion_callback.h"
#include "net/cookies/canonical_cookie.h"

class FilePath;
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
class CookieOptions;
class HttpRequestHeaders;
class HttpResponseHeaders;
class SocketStream;
class URLRequest;

class NetworkDelegate : public base::NonThreadSafe {
 public:
  // AuthRequiredResponse indicates how a NetworkDelegate handles an
  // OnAuthRequired call. It's placed in this file to prevent url_request.h
  // from having to include network_delegate.h.
  enum AuthRequiredResponse {
    AUTH_REQUIRED_RESPONSE_NO_ACTION,
    AUTH_REQUIRED_RESPONSE_SET_AUTH,
    AUTH_REQUIRED_RESPONSE_CANCEL_AUTH,
    AUTH_REQUIRED_RESPONSE_IO_PENDING,
  };
  typedef base::Callback<void(AuthRequiredResponse)> AuthCallback;

  enum RequestWaitState {
    REQUEST_WAIT_STATE_CACHE_START,
    REQUEST_WAIT_STATE_CACHE_FINISH,
    REQUEST_WAIT_STATE_NETWORK_START,
    REQUEST_WAIT_STATE_NETWORK_FINISH,
    REQUEST_WAIT_STATE_RESET
  };

  virtual ~NetworkDelegate() {}

  // Notification interface called by the network stack. Note that these
  // functions mostly forward to the private virtuals. They also add some sanity
  // checking on parameters. See the corresponding virtuals for explanations of
  // the methods and their arguments.
  int NotifyBeforeURLRequest(URLRequest* request,
                             const CompletionCallback& callback,
                             GURL* new_url);
  int NotifyBeforeSendHeaders(URLRequest* request,
                              const CompletionCallback& callback,
                              HttpRequestHeaders* headers);
  void NotifySendHeaders(URLRequest* request,
                         const HttpRequestHeaders& headers);
  int NotifyHeadersReceived(
      URLRequest* request,
      const CompletionCallback& callback,
      HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers);
  void NotifyBeforeRedirect(URLRequest* request,
                            const GURL& new_location);
  void NotifyResponseStarted(URLRequest* request);
  void NotifyRawBytesRead(const URLRequest& request, int bytes_read);
  void NotifyCompleted(URLRequest* request, bool started);
  void NotifyURLRequestDestroyed(URLRequest* request);
  void NotifyPACScriptError(int line_number, const string16& error);
  AuthRequiredResponse NotifyAuthRequired(URLRequest* request,
                                          const AuthChallengeInfo& auth_info,
                                          const AuthCallback& callback,
                                          AuthCredentials* credentials);
  bool CanGetCookies(const URLRequest& request,
                     const CookieList& cookie_list);
  bool CanSetCookie(const URLRequest& request,
                    const std::string& cookie_line,
                    CookieOptions* options);
  bool CanAccessFile(const URLRequest& request,
                     const FilePath& path) const;
  bool CanThrottleRequest(const URLRequest& request) const;

  int NotifyBeforeSocketStreamConnect(SocketStream* socket,
                                      const CompletionCallback& callback);

  void NotifyRequestWaitStateChange(const URLRequest& request,
                                    RequestWaitState state);

 private:
  // This is the interface for subclasses of NetworkDelegate to implement. These
  // member functions will be called by the respective public notification
  // member function, which will perform basic sanity checking.

  // Called before a request is sent. Allows the delegate to rewrite the URL
  // being fetched by modifying |new_url|. |callback| and |new_url| are valid
  // only until OnURLRequestDestroyed is called for this request. Returns a net
  // status code, generally either OK to continue with the request or
  // ERR_IO_PENDING if the result is not ready yet. A status code other than OK
  // and ERR_IO_PENDING will cancel the request and report the status code as
  // the reason.
  virtual int OnBeforeURLRequest(URLRequest* request,
                                 const CompletionCallback& callback,
                                 GURL* new_url) = 0;

  // Called right before the HTTP headers are sent. Allows the delegate to
  // read/write |headers| before they get sent out. |callback| and |headers| are
  // valid only until OnCompleted or OnURLRequestDestroyed is called for this
  // request.
  // Returns a net status code.
  virtual int OnBeforeSendHeaders(URLRequest* request,
                                  const CompletionCallback& callback,
                                  HttpRequestHeaders* headers) = 0;

  // Called right before the HTTP request(s) are being sent to the network.
  // |headers| is only valid until OnCompleted or OnURLRequestDestroyed is
  // called for this request.
  virtual void OnSendHeaders(URLRequest* request,
                             const HttpRequestHeaders& headers) = 0;

  // Called for HTTP requests when the headers have been received. Returns a net
  // status code, generally either OK to continue with the request or
  // ERR_IO_PENDING if the result is not ready yet.  A status code other than OK
  // and ERR_IO_PENDING will cancel the request and report the status code as
  // the reason.
  // |original_response_headers| contains the headers as received over the
  // network, these must not be modified. |override_response_headers| can be set
  // to new values, that should be considered as overriding
  // |original_response_headers|.
  // |callback|, |original_response_headers|, and |override_response_headers|
  // are only valid until OnURLRequestDestroyed is called for this request.
  virtual int OnHeadersReceived(
      URLRequest* request,
      const CompletionCallback& callback,
      HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers) = 0;

  // Called right after a redirect response code was received.
  // |new_location| is only valid until OnURLRequestDestroyed is called for this
  // request.
  virtual void OnBeforeRedirect(URLRequest* request,
                                const GURL& new_location) = 0;

  // This corresponds to URLRequestDelegate::OnResponseStarted.
  virtual void OnResponseStarted(URLRequest* request) = 0;

  // Called every time we read raw bytes.
  virtual void OnRawBytesRead(const URLRequest& request, int bytes_read) = 0;

  // Indicates that the URL request has been completed or failed.
  // |started| indicates whether the request has been started. If false,
  // some information like the socket address is not available.
  virtual void OnCompleted(URLRequest* request, bool started) = 0;

  // Called when an URLRequest is being destroyed. Note that the request is
  // being deleted, so it's not safe to call any methods that may result in
  // a virtual method call.
  virtual void OnURLRequestDestroyed(URLRequest* request) = 0;

  // Corresponds to ProxyResolverJSBindings::OnError.
  virtual void OnPACScriptError(int line_number, const string16& error) = 0;

  // Called when a request receives an authentication challenge
  // specified by |auth_info|, and is unable to respond using cached
  // credentials. |callback| and |credentials| must be non-NULL, and must
  // be valid until OnURLRequestDestroyed is called for |request|.
  //
  // The following return values are allowed:
  //  - AUTH_REQUIRED_RESPONSE_NO_ACTION: |auth_info| is observed, but
  //    no action is being taken on it.
  //  - AUTH_REQUIRED_RESPONSE_SET_AUTH: |credentials| is filled in with
  //    a username and password, which should be used in a response to
  //    |auth_info|.
  //  - AUTH_REQUIRED_RESPONSE_CANCEL_AUTH: The authentication challenge
  //    should not be attempted.
  //  - AUTH_REQUIRED_RESPONSE_IO_PENDING: The action will be decided
  //    asynchronously. |callback| will be invoked when the decision is made,
  //    and one of the other AuthRequiredResponse values will be passed in with
  //    the same semantics as described above.
  virtual AuthRequiredResponse OnAuthRequired(
      URLRequest* request,
      const AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      AuthCredentials* credentials) = 0;

  // Called when reading cookies to allow the network delegate to block access
  // to the cookie. This method will never be invoked when
  // LOAD_DO_NOT_SEND_COOKIES is specified.
  virtual bool OnCanGetCookies(const URLRequest& request,
                               const CookieList& cookie_list) = 0;

  // Called when a cookie is set to allow the network delegate to block access
  // to the cookie. This method will never be invoked when
  // LOAD_DO_NOT_SAVE_COOKIES is specified.
  virtual bool OnCanSetCookie(const URLRequest& request,
                              const std::string& cookie_line,
                              CookieOptions* options) = 0;


  // Called when a file access is attempted to allow the network delegate to
  // allow or block access to the given file path.  Returns true if access is
  // allowed.
  virtual bool OnCanAccessFile(const URLRequest& request,
                               const FilePath& path) const = 0;

  // Returns true if the given request may be rejected when the
  // URLRequestThrottlerManager believes the server servicing the
  // request is overloaded or down.
  virtual bool OnCanThrottleRequest(const URLRequest& request) const = 0;

  // Called before a SocketStream tries to connect.
  virtual int OnBeforeSocketStreamConnect(
      SocketStream* socket, const CompletionCallback& callback) = 0;

  // Called when the completion of a URLRequest is blocking on a cache
  // action or a network action, or when that is no longer the case.
  // REQUEST_WAIT_STATE_RESET indicates for a given URLRequest
  // cancellation of any pending waits for this request.
  virtual void OnRequestWaitStateChange(const URLRequest& request,
                                        RequestWaitState state) = 0;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_DELEGATE_H_
