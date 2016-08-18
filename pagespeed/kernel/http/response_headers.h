// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef PAGESPEED_KERNEL_HTTP_RESPONSE_HEADERS_H_
#define PAGESPEED_KERNEL_HTTP_RESPONSE_HEADERS_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/headers.h"
#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"

namespace net_instaweb {

class GoogleUrl;
class HttpResponseHeaders;
class MessageHandler;
class Writer;

// Read/write API for HTTP response headers.
class ResponseHeaders : public Headers<HttpResponseHeaders> {
 public:
  enum VaryOption { kRespectVaryOnResources, kIgnoreVaryOnResources };
  enum ValidatorOption { kHasValidator, kNoValidator };

  // This constructor with options explicitly set should be used by all callers.
  explicit ResponseHeaders(const HttpOptions& options) { Init(options); }

  explicit ResponseHeaders(const ResponseHeaders& other);
  ResponseHeaders& operator=(const ResponseHeaders& other);

  // This default constructor should only be used in tests.
  // TODO(sligocki): Phase this out so that nobody uses this one by accident.
  ResponseHeaders() { Init(kDeprecatedDefaultHttpOptions); }

  virtual ~ResponseHeaders();

  // Returns true if the resource with given date and TTL is going to expire
  // shortly and should hence be proactively re-fetched. All the parameters are
  // absolute times.
  static bool IsImminentlyExpiring(
      int64 start_date_ms, int64 expire_ms, int64 now_ms,
      const HttpOptions& options);

  // This will set Date and (if supplied in the first place, Expires)
  // header to now if the delta of date header wrt now_ms is more than
  // a tolerance.  Leaves the ComputeCaching state dirty if it came in
  // dirty, or clean if it came in clean.
  void FixDateHeaders(int64 now_ms);

  // Returns whether the computed cache fields have been dirtied by
  // a mutation since ComputeCache was called.  This is exposed for
  // debug assertions.
  bool cache_fields_dirty() const { return cache_fields_dirty_; }

  virtual void Clear();

  void CopyFrom(const ResponseHeaders& other);

  // Merge the new content_type with what is already in the headers.
  // Returns true if the existing content-type header was changed.
  // If the new content_type contains non-printable characters, the
  // change will be rejected silently (and false will be returned).
  bool MergeContentType(const StringPiece& content_type);

  // Merge headers. Replaces all headers specified both here and in
  // other with the version in other. Useful for updating headers
  // when recieving 304 Not Modified responses.
  // Note: We must use Headers<HttpResponseHeaders> instead of ResponseHeaders
  // so that we don't expose the base UpdateFrom (and to avoid "hiding" errors).
  virtual void UpdateFrom(const Headers<HttpResponseHeaders>& other);

  // Initializes the response headers with the one in proto, clearing the
  // existing fields.
  void UpdateFromProto(const HttpResponseHeaders& proto);

  // Serialize HTTP response header to a binary stream.
  virtual bool WriteAsBinary(Writer* writer, MessageHandler* message_handler);

  // Read HTTP response header from a binary string.  Note that this
  // is distinct from HTTP response-header parsing, which is in
  // ResponseHeadersParser.
  virtual bool ReadFromBinary(const StringPiece& buf, MessageHandler* handler);

  // Serialize HTTP response header in HTTP format so it can be re-parsed.
  virtual bool WriteAsHttp(Writer* writer, MessageHandler* handler) const;

  // Compute caching information.  The current time is used to compute
  // the absolute time when a cache resource will expire.  The timestamp
  // is in milliseconds since 1970.  It is an error to call any of the
  // accessors before ComputeCaching is called.
  void ComputeCaching();

  // Returns true if these response headers indicate the response is
  // publicly cacheable if it was fetched w/o special authorization
  // headers.
  //
  // See also RequiresProxyRevalidation(), which must be used to
  // determine whether stale content can be re-used by a proxy.
  //
  // The difference between HTML and non-HTML is tolerance for Vary:Cookie.
  // In HTML we are willing to cache cookieless responses and serve them
  // to other cookieless requests, but this requires the requests to
  // be validated.  Callers can indicate their ability to validate requests
  // by passing kHasRequestValidator for has_request_validator.
  bool IsProxyCacheable(RequestHeaders::Properties properties,
                        // TODO(sligocki): Use http_options_.respect_vary.
                        // Some inconsistencies need to be worked out first.
                        VaryOption respect_vary_on_resources,
                        ValidatorOption has_request_validator) const;

  static VaryOption GetVaryOption(bool respect_vary) {
    return respect_vary ? kRespectVaryOnResources : kIgnoreVaryOnResources;
  }

  // The zero-arg version of IsProxyCacheable gives a pessimistic answer,
  // assuming the request has cookies, there is no validator, and we
  // respect Vary.
  bool IsProxyCacheable() const {
    return IsProxyCacheable(
        RequestHeaders::Properties(), kRespectVaryOnResources, kNoValidator);
  }

  // Returns true if the response is privately cacheable.
  //
  // Generally you want to use IsProxyCacheable*() instead.
  bool IsBrowserCacheable() const;

  // Determines whether must-revalidate is in any Cache-Control setting.
  //
  // Proxies such as PSOL likely want to use RequiresProxyRevalidation()
  // instead.
  bool RequiresBrowserRevalidation() const;

  // Determines whether either must-revalidate or proxy-revalidate is
  // in any Cache-Control setting.  These must be checked to see whether
  // it's OK to serve stale content while freshening in the background.
  bool RequiresProxyRevalidation() const;

  // Note(sligocki): I think CacheExpirationTimeMs will return 0 if !IsCacheable
  // TODO(sligocki): Look through callsites and make sure this is being
  // interpreted correctly.
  int64 CacheExpirationTimeMs() const;

  // Set Date, Cache-Control and Expires headers appropriately.
  // If cache_control_suffix is provided it is appended onto the
  // Cache-Control: "max-age=%d" string.
  // For example, cache_control_suffix = ", private" or ", no-cache, no-store".
  void SetDateAndCaching(int64 date_ms, int64 ttl_ms,
                         const StringPiece& cache_control_suffix);
  void SetDateAndCaching(int64 date_ms, int64 ttl_ms) {
    SetDateAndCaching(date_ms, ttl_ms, "");
  }
  // Returns Cache-Control header values that we might need to preserve. This
  // function is meant to be used with SetDateAndCaching. It currently looks for
  // and returns no-transform and no-store if found.
  GoogleString CacheControlValuesToPreserve();

  // Set a time-based header, converting ms since epoch to a string.
  void SetTimeHeader(const StringPiece& header, int64 time_ms);
  void SetDate(int64 date_ms) { SetTimeHeader(HttpAttributes::kDate, date_ms); }
  void SetLastModified(int64 last_modified_ms) {
    SetTimeHeader(HttpAttributes::kLastModified, last_modified_ms);
  }

  // Sets the cache-control max-age to the specified value leaving the remaining
  // Cache-Control attributes the same. This also updates the Expires header
  // appropriately. Note that all existing max-age values are removed.
  void SetCacheControlMaxAge(int64 ttl_ms);

  // Sets the cache-control to explicitly have 'public', as long as that's
  // not in conflict with other CC headers.
  void SetCacheControlPublic();

  // Sets the x-original-content-length header, used to relay information on
  // the original size of optimized resources.
  void SetOriginalContentLength(int64 content_length);

  // Sets the content-length attribute, removing any matching
  // x-orginal-content-length header.
  void SetContentLength(int64 content_length);

  // Removes hop-by-hop plus cookie headers, and returns true if any changes
  // were made.
  bool Sanitize();

  // Copies the HttpResponseHeaders proto from the response headers to the given
  // input after removing the Set-Cookie fields.
  void GetSanitizedProto(HttpResponseHeaders* proto) const;

  // TODO(jmarantz): consider an alternative representation
  bool headers_complete() const { return has_status_code(); }

  int status_code() const;
  bool has_status_code() const;
  void set_status_code(const int code);
  const char* reason_phrase() const;
  void set_reason_phrase(const StringPiece& reason_phrase);

  const HttpOptions& http_options() const { return http_options_; }

  // TODO(sligocki): Remove these setters (and getters) once we make sure
  // that all values are set at construction time.
  int64 implicit_cache_ttl_ms() const {
    return http_options_.implicit_cache_ttl_ms;
  }
  void set_implicit_cache_ttl_ms(const int64 ttl) {
    http_options_.implicit_cache_ttl_ms = ttl;
  }

  bool has_last_modified_time_ms() const;
  int64 last_modified_time_ms() const;
  int64 date_ms() const;  // Timestamp from Date header.
  bool has_date_ms() const;
  int64 cache_ttl_ms() const;
  bool is_implicitly_cacheable() const;

  GoogleString ToString() const;

  // Sets the status code and reason_phrase based on an internal table.
  void SetStatusAndReason(HttpStatus::Code code);

  void DebugPrint() const;

  // Parses an arbitrary string into milliseconds since 1970
  static bool ParseTime(const char* time_str, int64* time_ms);

  // Returns true if our status denotes the request failing.
  inline bool IsErrorStatus() const {
    int status = status_code();
    return status >= 400 && status <= 599;
  }

  // Returns true if our status denotes a server side error.
  inline bool IsServerErrorStatus() const {
    int status = status_code();
    return status >= 500 && status <= 599;
  }

  // Returns true if our status denotes a redirect.
  bool IsRedirectStatus() const {
    int status = status_code();
    return status >= 300 && status <= 399 && status != 304;
  }

  // Determines whether a response header is marked as gzipped.
  bool IsGzipped() const;
  bool WasGzippedLast() const;

  // Get ContentType. NULL if none set or it isn't in our predefined set of
  // known content types.
  const ContentType* DetermineContentType() const;

  // Does this header have an HTML-like Content-Type (HTML, XHTML, ...).
  bool IsHtmlLike() const {
    const ContentType* type = DetermineContentType();
    return (type != NULL && type->IsHtmlLike());
  }

  // Get the charset. Empty string if none set in a Content-Type header.
  GoogleString DetermineCharset() const;

  // Determine both the charset and content-type as above. See
  // DetermineContentType() and DetermineCharset() for details.
  // You may also pass in NULL for those of _out parameters you do not
  // need (but in that case the individual functions would be more convenient)
  void DetermineContentTypeAndCharset(const ContentType** content_type_out,
                                      GoogleString* charset_out) const;


  // Parses a date header such as HttpAttributes::kDate or
  // HttpAttributes::kExpires, returning the timestamp as
  // number of milliseconds since 1970.
  bool ParseDateHeader(const StringPiece& attr, int64* date_ms) const;

  // Parses the first line of an HTTP response, including the "HTTP/".
  void ParseFirstLine(const StringPiece& first_line);

  // Parses the first line of an HTTP response, skipping the "HTTP/".
  void ParseFirstLineHelper(const StringPiece& first_line);

  // Set whole first line.
  void set_first_line(int major_version, int minor_version, int status_code,
                      const StringPiece& reason_phrase) {
    set_major_version(major_version);
    set_minor_version(minor_version);
    set_status_code(status_code);
    set_reason_phrase(reason_phrase);
  }

  // Finds Content-Length in the response headers, returning true and putting
  // it in *content_length if successful.
  bool FindContentLength(int64* content_length) const;

  // Force cache the response with the given TTL even if it is private. Note
  // that this does not change any of the headers. The values of cache_ttl_ms,
  // IsCacheable and IsProxyCacheable are updated once ComputeCaching() is
  // called.
  // Note that for responses which were originally cacheable, the effective
  // cache TTL is the maximum of the original TTL and ttl_ms.
  // For responses which were originally uncacheable, the new cache TTL is
  // ttl_ms.
  void ForceCaching(int64 ttl_ms);

  // Update the caching headers if the response has force cached.
  bool UpdateCacheHeadersIfForceCached();

  // Returns estimated size in bytes of these headers (if transferred over
  // HTTP, not SPDY or other protocols). This is an estimate because it may not
  // properly account for things like spacing around : or whether multiple
  // headers were on a single or multiple lines.
  int64 SizeEstimate() const;

  // Returns true if the response headers have cookies and false otherwise.
  // If cookies are found then it sets them in cookie_str in javascript array
  // format.
  bool GetCookieString(GoogleString* cookie_str) const;

  // Returns true if the response headers have a cookie with the given name.
  // 'values' gives the associated values. 'attributes' gives the attributes.
  // name            results in "" in values and nothing in attributes.
  // name=; HttpOnly results in "" in values and "HttpOnly" in attributes.
  // name=value      results in "value" in values and nothing in attributes.
  // name=value; Expires=yaddayadda; HttpOnly results in "value" in values and
  //                 " Expires=yaddayadda" and " HttpOnly" in attributes.
  //                 Note that the attributes are not trimmed of whitespace.
  // The return value is true in all the above cases.
  // It is a limitation of this API that a cookie with no value set is
  // indistinguishable from a cookie with an empty value. Furthermore, if the
  // cookie is set in multiple headers, values and attributes will be the union
  // of those headers' contents.
  // TODO(matterbury): Fix this to implement the correct behavior, which should
  // take into account the domain and path of the cookie as part of uniqueness.
  bool HasCookie(StringPiece name, StringPieceVector* values,
                 StringPieceVector* attributes) const;

  // Returns true if any cookies in the response headers have an attribute with
  // the given name, returning the value for the first one found in
  // '*attribute_value' iff it isn't NULL.
  bool HasAnyCookiesWithAttribute(StringPiece attribute_name,
                                  StringPiece* attribute_value);

  // Set or clears the given query parameters as response header cookies,
  // skipping any in to_exclude. query_params and option_cookies are both
  // query parameters (name=value separated by '&'s) and are treated as
  // untrusted data. Sets the cookies' Expires attributes to the given value.
  // Returns true if any cookies were set, false if not.
  bool SetQueryParamsAsCookies(const GoogleUrl& gurl, StringPiece query_params,
                               const StringPieceVector& to_exclude,
                               int64 expiration_time);
  bool ClearOptionCookies(const GoogleUrl& gurl, StringPiece option_cookies,
                          const StringPieceVector& to_exclude);

  // Returns true if the headers may contain a Link: rel = canonical entry.
  // (Slightly approximate, will never say 'no' if it's there).
  bool HasLinkRelCanonical() const;

  // Constructs a <url>; rel="canonical" value for use with a Link header.
  static GoogleString RelCanonicalHeaderValue(StringPiece url);

  // Gives a new value for the cache control header, making it more restrictive
  // by adding s-maxage=<s_maxage_sec>.  Takes into account existing s-maxage
  // and maxage segments:
  // * If there is no s-maxage:
  //   * If there's a maxage <= s_maxage_sec:
  //     * Make no changes.
  //   * Otherwise append an s_maxage
  // * Otherwise, bring s-maxage down to s_maxage_sec if it's larger.
  void SetSMaxAge(int s_maxage_sec);
  // Stand-alone version of SetSMaxAge.  If there are changes to make, returns
  // true and sets updated_cache_control.
  static bool ApplySMaxAge(int s_maxage_sec,
                           StringPiece existing_cache_control,
                           GoogleString* updated_cache_control);

  // Returns true if the given value should be interpreted as a header being
  // marked as hop by hop when listed as a value in a Connection: header.
  static bool IsHopByHopIndication(StringPiece val);

 protected:
  virtual void UpdateHook();

 private:
  void Init(const HttpOptions& options);

  // Parse the original and fresh content types, and add a new header based
  // on the two of them, giving preference to the original.
  // e.g. if the original specified charset=UTF-8 and the new one specified
  // charset=UTF-16, the resulting header would have charset=UTF-8.
  // Returns true if the headers were changed.
  bool CombineContentTypes(const StringPiece& orig, const StringPiece& fresh);

  friend class ResponseHeadersTest;
  bool cache_fields_dirty_;

  // TODO(sligocki): Make this const.
  HttpOptions http_options_;

  // The number of milliseconds of cache TTL for which we should cache the
  // response even if it was originally uncacheable.
  int64 force_cache_ttl_ms_;
  // Indicates if the response was force cached.
  bool force_cached_;

  // Allow copy and assign.
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTTP_RESPONSE_HEADERS_H_
