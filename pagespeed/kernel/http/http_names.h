/*
 * Copyright 2010 Google Inc.
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
// String and numeric constants for common HTTP Attributes & status codes.

#ifndef PAGESPEED_KERNEL_HTTP_HTTP_NAMES_H_
#define PAGESPEED_KERNEL_HTTP_HTTP_NAMES_H_

#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

// Global constants for common HTML attributes names and values.
//
// TODO(jmarantz): Proactively change all the occurrences of the static strings
// to use these shared constants.
struct HttpAttributes {
  static const char kAccept[];
  static const char kAcceptEncoding[];
  static const char kAcceptRanges[];
  static const char kAccessControlAllowOrigin[];
  static const char kAccessControlAllowCredentials[];
  static const char kAge[];
  static const char kAllow[];
  static const char kAltSvc[];
  static const char kAlternateProtocol[];
  static const char kAttachment[];
  static const char kAuthorization[];
  static const char kCacheControl[];
  static const char kConnection[];
  static const char kContentEncoding[];
  static const char kContentDisposition[];
  static const char kContentLanguage[];
  static const char kContentLength[];
  static const char kContentType[];
  static const char kCookie[];
  static const char kCookie2[];
  static const char kDate[];
  static const char kDeflate[];
  static const char kDnt[];
  static const char kEtag[];
  static const char kExpires[];
  static const char kGzip[];
  static const char kHost[];
  static const char kIfModifiedSince[];
  static const char kIfNoneMatch[];
  static const char kKeepAlive[];
  static const char kLastModified[];
  static const char kLink[];
  static const char kLocation[];
  static const char kMaxAge[];
  static const char kNoCache[];
  static const char kNoCacheMaxAge0[];
  static const char kNoStore[];
  static const char kNosniff[];
  static const char kOrigin[];
  static const char kPragma[];
  static const char kPrivate[];
  static const char kProxyAuthenticate[];
  static const char kProxyAuthorization[];
  static const char kPublic[];
  static const char kPurpose[];
  static const char kReferer[];  // sic
  static const char kRefresh[];
  static const char kSaveData[];
  static const char kServer[];
  static const char kSetCookie[];
  static const char kSetCookie2[];
  static const char kTE[];
  static const char kTrailers[];
  static const char kTransferEncoding[];
  static const char kUpgrade[];
  static const char kUserAgent[];
  static const char kVary[];
  static const char kVia[];
  static const char kWarning[];
  static const char kXmlHttpRequest[];
  static const char kXAssociatedContent[];
  static const char kXContentTypeOptions[];
  static const char kXForwardedFor[];
  static const char kXForwardedProto[];
  static const char kXGooglePagespeedClientId[];
  static const char kXGoogleRequestEventId[];
  // If this header's value matches the configured blocking rewrite key, then
  // all rewrites are completed before the response is sent to the client.
  static const char kXPsaBlockingRewrite[];
  // This header determines how the blocking rewrite will behave; will it wait
  // for async events or not.
  static const char kXPsaBlockingRewriteMode[];
  // Value of the kXPsaBlockingRewriteMode header which makes the blocking
  // rewrite wait for async events.
  // TODO(bharathbhushan): Does this belong somewhere else?
  static const char kXPsaBlockingRewriteModeSlow[];

  // A request header for client to specify client options.
  static const char kXPsaClientOptions[];

  // This header is set on optional fetches that got dropped due to load.
  static const char kXPsaLoadShed[];

  static const char kXRequestedWith[];

  // This header is set on optimized responses to indicate the original
  // content length.
  static const char kXOriginalContentLength[];
  static const char kXUACompatible[];

  // Sendfile type responses.
  static const char kXSendfile[];
  static const char kXAccelRedirect[];
  // PageSpeed Loop detection for proxy mode.
  static const char kXPageSpeedLoop[];

  // Gets a sorted StringPieceVector containing all the end-to-end headers.
  // Any fields listed in here should be ignored during sanitization when they
  // are specified in a Connection: header.
  // See http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html#sec13.5.1
  // and https://www.mnot.net/blog/2011/07/11/what_proxies_must_do
  static const StringPieceVector& SortedEndToEndHeaders();

  // Gets a sorted StringPieceVector containing all the hop-by-hop headers,
  // plus Set-Cookie and Set-Cookie2, per
  //
  // http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html
  // Note: The returned vector contains NULL-terminated char* entries but
  // returning it via StringPieceVector causes us to lose this guarantee and we
  // end up creating temporary GoogleStrings to convert these back to char*.
  // This performance overhead might be revisited if considered important.
  static const StringPieceVector& SortedHopByHopHeaders();

  // Gets a StringPieceVector containing the caching-related headers that should
  // be removed from responses.
  // Note: The returned vector contains NULL-terminated char* entries but
  // returning it via StringPieceVector causes us to lose this guarantee and we
  // end up creating temporary GoogleStrings to convert these back to char*.
  // This performance overhead might be revisited if considered important.
  static const StringPieceVector& CachingHeadersToBeRemoved();
};

namespace HttpStatus {
// Http status codes.
// Grokked from http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
enum Code {
  kContinue = 100,
  kSwitchingProtocols = 101,

  kOK = 200,
  kCreated = 201,
  kAccepted = 202,
  kNonAuthoritative = 203,
  kNoContent = 204,
  kResetContent = 205,
  kPartialContent = 206,

  kMultipleChoices = 300,
  kMovedPermanently = 301,
  kFound = 302,
  kSeeOther = 303,
  kNotModified = 304,
  kUseProxy = 305,
  kSwitchProxy = 306,  // In old spec; no longer used.
  kTemporaryRedirect = 307,

  kBadRequest = 400,
  kUnauthorized = 401,
  kPaymentRequired = 402,
  kForbidden = 403,
  kNotFound = 404,
  kMethodNotAllowed = 405,
  kNotAcceptable = 406,
  kProxyAuthRequired = 407,
  kRequestTimeout = 408,
  kConflict = 409,
  kGone = 410,
  kLengthRequired = 411,
  kPreconditionFailed = 412,
  kEntityTooLarge = 413,
  kUriTooLong = 414,
  kUnsupportedMediaType = 415,
  kRangeNotSatisfiable = 416,
  kExpectationFailed = 417,
  kImATeapot = 418,

  kInternalServerError = 500,
  kNotImplemented = 501,
  kBadGateway = 502,
  kUnavailable = 503,
  kGatewayTimeout = 504,
  kHttpVersionNotSupported = 505,

  // Instaweb-specific proxy failure constants.
  kProxyPublisherFailure = 520,
  kProxyFailure = 521,
  kProxyConfigurationFailure = 522,
  kProxyDeclinedRequest = 523,
  kProxyDnsLookupFailure = 524,

  // Instaweb-specific response codes: these are intentionally chosen to be
  // outside the normal HTTP range, but we consider these response codes
  // to be 'cacheable' in our own cache. These particular ones correspond
  // to values of the FetchResponseStatus enum (non-numerically).
  kRememberFailureRangeStart = 10001,

  // Corresponds to kFetchStatusOtherError
  kRememberFetchFailedStatusCode = 10001,

  // Note that this includes all non-200 status code responses that are not
  // cacheable. Corresponds to kFetchStatusUncacheableError.
  kRememberNotCacheableStatusCode = 10002,

  // This includes all 200 status code responses that are not cacheable.
  // Corresponds to kFetchStatusUncacheable200.
  kRememberNotCacheableAnd200StatusCode = 10003,

  // Corresponds to kFetchStatus4xxError.
  kRememberFetchFailed4xxCode = 10004,

  // We do not allow caching empty resources. Remember that.
  // Corresponds to kFetchStatusEmpty.
  kRememberEmptyStatusCode = 10005,

  // For remembering that we load-shed an attempt to fetch this recently.
  // Corresponds to kFetchStatusDropped.
  kRememberDroppedStatusCode = 10006,

  // End point of failure caching range, in the usual [a, b) meaning.
  kRememberFailureRangeEnd,

  // Status code used when the actual status code of the response is unknown at
  // the time of ProxyFetchPropertyCallbackCollector::Detach().
  kUnknownStatusCode = 10020,
};

// Transform a status code into the equivalent reason phrase.
const char* GetReasonPhrase(Code rc);

}  // namespace HttpStatus

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTTP_HTTP_NAMES_H_
