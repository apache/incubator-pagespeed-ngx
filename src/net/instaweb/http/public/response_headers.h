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

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/http/public/headers.h"
#include "net/instaweb/http/public/meta_data.h"  // HttpAttributes, HttpStatus
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class MessageHandler;
class HttpResponseHeaders;
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

  virtual void Clear();

  void CopyFrom(const ResponseHeaders& other);

  // Add a new header.
  virtual void Add(const StringPiece& name, const StringPiece& value);

  // Remove headers by name and value.
  virtual bool Remove(const StringPiece& name, const StringPiece& value);

  // Remove all headers by name.
  virtual bool RemoveAll(const StringPiece& name);

  // Remove all headers whose name is in |names|.
  virtual void RemoveAllFromSet(const StringSet& names);

  // Similar to RemoveAll followed by Add.  Note that the attribute
  // order may be changed as a side effect of this operation.
  virtual void Replace(const StringPiece& name, const StringPiece& value);

  // Merge headers. Replaces all headers specified both here and in
  // other with the version in other. Useful for updating headers
  // when recieving 304 Not Modified responses.
  // Note: We must use Headers<HttpResponseHeaders> instead of ResponseHeaders
  // so that we don't expose the base UpdateFrom (and to avoid "hiding" errors).
  virtual void UpdateFrom(const Headers<HttpResponseHeaders>& other);

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
  bool IsProxyCacheable() const;
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

  // Set a time-based header, converting ms since epoch to a string.
  void SetTimeHeader(const StringPiece& header, int64 time_ms);
  void SetDate(int64 date_ms) { SetTimeHeader(HttpAttributes::kDate, date_ms); }
  void SetLastModified(int64 last_modified_ms) {
    SetTimeHeader(HttpAttributes::kLastModified, last_modified_ms);
  }

  // TODO(jmarantz): consider an alternative representation
  bool headers_complete() const { return has_status_code(); }

  int status_code() const;
  bool has_status_code() const;
  void set_status_code(const int code);
  const char* reason_phrase() const;
  void set_reason_phrase(const StringPiece& reason_phrase);

  int64 last_modified_time_ms() const;
  int64 fetch_time_ms() const;  // Timestamp from Date header.
  bool has_fetch_time_ms() const;
  int64 cache_ttl_ms() const;

  GoogleString ToString() const;

  // Sets the status code and reason_phrase based on an internal table.
  void SetStatusAndReason(HttpStatus::Code code);

  void DebugPrint() const;

  // Parses an arbitrary string into milliseconds since 1970
  static bool ParseTime(const char* time_str, int64* time_ms);

  // Returns true if our status denotes the request failing
  inline bool IsErrorStatus() {
    int status = status_code();
    return status >= 400 && status <= 599;
  }

  // Determines whether a response header is marked as gzipped.
  bool IsGzipped() const;
  bool WasGzippedLast() const;

  // Parses a date header such as HttpAttributes::kDate or
  // HttpAttributes::kExpires, returning the timestamp as
  // number of milliseconds since 1970.
  bool ParseDateHeader(const StringPiece& attr, int64* date_ms) const;

  // Updates a date header using time specified as a number of milliseconds
  // since 1970.
  void UpdateDateHeader(const StringPiece& attr, int64 date_ms);

  void ParseFirstLine(const StringPiece& first_line);
  // Set whole first line.
  void set_first_line(int major_version, int minor_version, int status_code,
                      const StringPiece& reason_phrase) {
    set_major_version(major_version);
    set_minor_version(minor_version);
    set_status_code(status_code);
    set_reason_phrase(reason_phrase);
  }

  // Returns whether or not we can cache these headers if we take into
  // account the Vary: headers.
  bool VaryCacheable() const;

  // Finds Content-Length in the response headers, returning true and putting
  // it in *content_length if successful.
  bool FindContentLength(int64* content_length);

 private:
  friend class ResponseHeadersTest;
  bool cache_fields_dirty_;

  DISALLOW_COPY_AND_ASSIGN(ResponseHeaders);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_RESPONSE_HEADERS_H_
