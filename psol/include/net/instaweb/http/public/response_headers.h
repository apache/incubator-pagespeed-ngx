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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_RESPONSE_HEADERS_H_
#define NET_INSTAWEB_HTTP_PUBLIC_RESPONSE_HEADERS_H_

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/headers.h"
#include "net/instaweb/http/public/meta_data.h"  // HttpAttributes, HttpStatus
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class HttpResponseHeaders;
class RequestHeaders;
class MessageHandler;
class Writer;

// Read/write API for HTTP response headers.
class ResponseHeaders : public Headers<HttpResponseHeaders> {
 public:
  // The number of milliseconds of cache TTL we assign to resources that
  // are "likely cacheable" (e.g. images, js, css, not html) and have no
  // explicit cache ttl or expiration date.
  static const int64 kImplicitCacheTtlMs = 5 * Timer::kMinuteMs;

  ResponseHeaders();
  virtual ~ResponseHeaders();

  // This will set Date and (if supplied in the first place, Expires)
  // header to now if the delta of date header wrt now_ms is more than
  // a tolerance.  Leaves the ComputeCaching state dirty if it came in
  // dirty, or clean if it came in clean.
  void FixDateHeaders(int64 now_ms);

  virtual void Clear();

  void CopyFrom(const ResponseHeaders& other);

  // Add a new header.
  virtual void Add(const StringPiece& name, const StringPiece& value);

  // Merge the new content_type with what is already in the headers.
  // Returns true if the existing content-type header was changed.
  bool MergeContentType(const StringPiece& content_type);

  // Remove headers by name and value.
  virtual bool Remove(const StringPiece& name, const StringPiece& value);

  // Remove all headers by name.
  virtual bool RemoveAll(const StringPiece& name);

  // Remove all headers whose name is in |names|.
  virtual bool RemoveAllFromSet(const StringSetInsensitive& names);

  // Similar to RemoveAll followed by Add.  Note that the attribute
  // order may be changed as a side effect of this operation.
  virtual void Replace(const StringPiece& name, const StringPiece& value);

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
  bool IsCacheable() const;

  // Returns true if these response headers indicate the response is cacheable
  // if it was fetched w/o special authorization headers.
  //
  // Generally you want to use IsProxyCacheableGivenRequest() instead which will
  // also take the request headers into account, unless you know the request
  // was synthesized with known headers which do not include authorization.
  bool IsProxyCacheable() const;

  // Returns true if these response header indicate the response is cacheable
  // if it was fetched with given 'request_headers'.
  bool IsProxyCacheableGivenRequest(const RequestHeaders& req_headers) const;

  // Note(sligocki): I think CacheExpirationTimeMs will return 0 if !IsCacheable
  // TODO(sligocki): Look through callsites and make sure this is being
  // interpretted correctly.
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

  // Sets the original content length header, used to relay information on
  // the original size of optimized resources.
  void SetOriginalContentLength(int64 content_length);

  // Removes cookie headers, and returns true if any changes were made.
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
  int64 implicit_cache_ttl_ms() const { return implicit_cache_ttl_ms_; }
  void set_implicit_cache_ttl_ms(const int64 ttl) {
    implicit_cache_ttl_ms_ = ttl;
  }

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
  inline bool IsErrorStatus() {
    int status = status_code();
    return status >= 400 && status <= 599;
  }

  // Returns true if our status denotes a server side error.
  inline bool IsServerErrorStatus() {
    int status = status_code();
    return status >= 500 && status <= 599;
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

  // Returns true if the date header is later than time_ms. Used in invalidation
  // of http cache.
  bool IsDateLaterThan(int64 time_ms) const {
    return date_ms() > time_ms;
  }

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

  // Returns whether or not we can cache these headers if we take into
  // account the Vary: headers. Note that we consider Vary: Cookie as cacheable
  // if request_has_cookie is false.
  bool VaryCacheable(bool request_has_cookie) const;

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

  // Returns true in the response headers have a cookie attribute with the given
  // name. values gives the associated values.
  // name=value results in "value" in values.
  // name=      results in "" in values.
  // name       results in nothing being added to values.
  // The return value is true in all the above cases.
  // It is a limitation of this API that a cookie value of "name=value;name" is
  // indistinguishable from a cookie value of "name=value".
  bool HasCookie(StringPiece name, StringPieceVector* values) const;

 private:
  // Parse the original and fresh content types, and add a new header based
  // on the two of them, giving preference to the original.
  // e.g. if the original specified charset=UTF-8 and the new one specified
  // charset=UTF-16, the resulting header would have charset=UTF-8.
  // Returns true if the headers were changed.
  bool CombineContentTypes(const StringPiece& orig, const StringPiece& fresh);

  friend class ResponseHeadersTest;
  bool cache_fields_dirty_;

  // The number of milliseconds of cache TTL we assign to resources that are
  // likely cacheable and have no explicit cache ttl or expiration date.
  int64 implicit_cache_ttl_ms_;

  // The number of milliseconds of cache TTL for which we should cache the
  // response even if it was originally uncacheable.
  int64 force_cache_ttl_ms_;
  // Indicates if the response was force cached.
  bool force_cached_;

  DISALLOW_COPY_AND_ASSIGN(ResponseHeaders);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_RESPONSE_HEADERS_H_
