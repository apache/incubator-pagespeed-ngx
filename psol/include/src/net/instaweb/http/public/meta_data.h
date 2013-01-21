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
// Meta-data associated with a rewriting resource.  This is
// primarily a key-value store, but additionally we want to
// get easy access to the cache expiration time.

#ifndef NET_INSTAWEB_HTTP_PUBLIC_META_DATA_H_
#define NET_INSTAWEB_HTTP_PUBLIC_META_DATA_H_

namespace net_instaweb {

// Global constants for common HTML attributes names and values.
//
// TODO(jmarantz): Proactively change all the occurrences of the static strings
// to use these shared constants.
struct HttpAttributes {
  static const char kAcceptEncoding[];
  static const char kAuthorization[];
  static const char kCacheControl[];
  static const char kConnection[];
  static const char kContentEncoding[];
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
  static const char kLastModified[];
  static const char kLocation[];
  static const char kNoCache[];
  static const char kPragma[];
  static const char kProxyAuthorization[];
  static const char kReferer[];  // sic
  static const char kServer[];
  static const char kSetCookie[];
  static const char kSetCookie2[];
  static const char kTransferEncoding[];
  static const char kUserAgent[];
  static const char kVary[];
  static const char kWarning[];
  static const char kXmlHttpRequest[];
  static const char kXAssociatedContent[];
  static const char kXForwardedFor[];
  static const char kXForwardedProto[];
  static const char kXGooglePagespeedClientId[];
  static const char kXGoogleRequestEventId[];
  // If this header's value matches the configured blocking rewrite key, then
  // all rewrites are completed before the response is sent to the client.
  static const char kXPsaBlockingRewrite[];

  // If this header is present on an incoming request it will be treated as if
  // it came over a SPDY connection for purposes of applying special
  // configuration or optimizations.
  static const char kXPsaOptimizeForSpdy[];

  // This header is set on optional fetches that got dropped due to load.
  static const char kXPsaLoadShed[];
  static const char kXRequestedWith[];

  // This header is set on optimized responses to indicate the original
  // content length.
  static const char kXOriginalContentLength[];
  static const char kXUACompatible[];
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

  // Instaweb-specific response codes: these are intentionally chosen to be
  // outside the normal HTTP range, but we consider these response codes
  // to be 'cacheable' in our own cache.
  kRememberFetchFailedStatusCode = 10001,
  // Note that this includes all non-200 status code responses that are not
  // cacheable.
  kRememberNotCacheableStatusCode = 10002,
  // This includes all 200 status code responses that are not cacheable.
  kRememberNotCacheableAnd200StatusCode = 10003,
  // Status code used when the actual status code of the response is unknown at
  // the time of ProxyFetchPropertyCallbackCollector::Detach().
  kUnknownStatusCode = 10004,
};

// Transform a status code into the equivalent reason phrase.
const char* GetReasonPhrase(Code rc);

}  // namespace HttpStatus

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_META_DATA_H_
