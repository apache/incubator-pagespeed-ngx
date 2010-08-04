/**
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_META_DATA_H_
#define NET_INSTAWEB_UTIL_PUBLIC_META_DATA_H_

#include <vector>
#include "base/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class Writer;

namespace HttpStatus {
// Http status codes.
// Grokked from http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
enum Code {
  CONTINUE = 100,
  SWITCHING_PROTOCOLS = 101,

  OK = 200,
  CREATED = 201,
  ACCEPTED = 202,
  NON_AUTHORITATIVE = 203,
  NO_CONTENT = 204,
  RESET_CONTENT = 205,
  PARTIAL_CONTENT = 206,

  MULTIPLE_CHOICES = 300,
  MOVED_PERMANENTLY = 301,
  FOUND = 302,
  SEE_OTHER = 303,
  NOT_MODIFIED = 304,
  USE_PROXY = 305,
  SWITCH_PROXY = 306,  // In old spec; no longer used.
  TEMPORARY_REDIRECT = 307,

  BAD_REQUEST = 400,
  UNAUTHORIZED = 401,
  PAYMENT_REQUIRED = 402,
  FORBIDDEN = 403,
  NOT_FOUND = 404,
  METHOD_NOT_ALLOWED = 405,
  NOT_ACCEPTABLE = 406,
  PROXY_AUTH_REQUIRED = 407,
  REQUEST_TIMEOUT = 408,
  CONFLICT = 409,
  GONE = 410,
  LENGTH_REQUIRED = 411,
  PRECONDITION_FAILED = 412,
  ENTITY_TOO_LARGE = 413,
  URI_TOO_LONG = 414,
  UNSUPPORTED_MEDIA_TYPE = 415,
  RANGE_NOT_SATISFIABLE = 416,
  EXPECTATION_FAILED = 417,

  INTERNAL_SERVER_ERROR = 500,
  NOT_IMPLEMENTED = 501,
  BAD_GATEWAY = 502,
  UNAVAILABLE = 503,
  GATEWAY_TIMEOUT = 504,
  HTTP_VERSION_NOT_SUPPORTED = 505,
};
}  // namespace HttpStatus

// Container for required meta-data.  General HTTP headers can be added
// here as name/value pairs, and caching information can then be derived.
//
// TODO(jmarantz): consider rename to HTTPHeader.
// TODO(sligocki): This represents an HTTP response header. We need a request
// header class as well.
// TODO(sligocki): Stop using char* in interface.
class MetaData {
 public:
  MetaData() {}
  virtual ~MetaData();

  void CopyFrom(const MetaData& other);

  // Reset headers to initial state.
  virtual void Clear() = 0;

  // Raw access for random access to attribute name/value pairs.
  virtual int NumAttributes() const = 0;
  virtual const char* Name(int index) const = 0;
  virtual const char* Value(int index) const = 0;

  // Get the attribute values associated with this name.  Returns
  // false if the attribute is not found.  If it was found, then
  // the values vector is filled in.
  virtual bool Lookup(const char* name, CharStarVector* values) const = 0;

  // Add a new header.
  virtual void Add(const char* name, const char* value) = 0;

  // Remove all headers by name.
  virtual void RemoveAll(const char* name) = 0;

  // Serialize HTTP response header to a stream.
  virtual bool Write(Writer* writer, MessageHandler* handler) const = 0;
  // Serialize just the headers (not the version and response code line).
  virtual bool WriteHeaders(Writer* writer, MessageHandler* handler) const = 0;

  // Parse a chunk of HTTP response header.  Returns number of bytes consumed.
  virtual int ParseChunk(const StringPiece& text,  MessageHandler* handler) = 0;

  // Compute caching information.  The current time is used to compute
  // the absolute time when a cache resource will expire.  The timestamp
  // is in milliseconds since 1970.  It is an error to call any of the
  // accessors before ComputeCaching is called.
  virtual void ComputeCaching() = 0;
  virtual bool IsCacheable() const = 0;
  virtual bool IsProxyCacheable() const = 0;
  virtual int64 CacheExpirationTimeMs() const = 0;
  virtual void SetDate(int64 date_ms) = 0;
  virtual void SetLastModified(int64 last_modified_ms) = 0;

  virtual bool headers_complete() const = 0;

  virtual int major_version() const = 0;
  virtual int minor_version() const = 0;
  virtual int status_code() const = 0;
  virtual const char* reason_phrase() const = 0;
  virtual int64 timestamp_ms() const = 0;
  virtual bool has_timestamp_ms() const = 0;

  virtual void set_major_version(int major_version) = 0;
  virtual void set_minor_version(int minor_version) = 0;

  // Sets the status code and reason_phrase based on an internal table.
  void SetStatusAndReason(HttpStatus::Code code);

  virtual void set_status_code(int status_code) = 0;
  virtual void set_reason_phrase(const StringPiece& reason_phrase) = 0;
  // Set whole first line.
  virtual void set_first_line(int major_version,
                              int minor_version,
                              int status_code,
                              const StringPiece& reason_phrase) {
    set_major_version(major_version);
    set_minor_version(minor_version);
    set_status_code(status_code);
    set_reason_phrase(reason_phrase);
  }

  virtual std::string ToString() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetaData);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_META_DATA_H_
