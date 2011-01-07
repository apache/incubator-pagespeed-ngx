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

#include <stdlib.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/headers.h"
#include "net/instaweb/util/public/meta_data.h"  // HttpAttributes, HttpStatus
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class HttpResponseHeaders;
class StringMultiMapInsensitive;
class Writer;

// Read/write API for HTTP response headers.
class ResponseHeaders : public Headers<HttpResponseHeaders> {
 public:
  ResponseHeaders();
  ~ResponseHeaders();

  void Clear();

  void CopyFrom(const ResponseHeaders& other);

  // Add a new header.
  void Add(const StringPiece& name, const StringPiece& value);

  // Remove all headers by name.
  void RemoveAll(const char* name);

  // Serialize HTTP response header to a binary stream.
  bool WriteAsBinary(Writer* writer, MessageHandler* message_handler);

  // Read HTTP response header from a binary string.  Note that this
  // is distinct from HTTP response-header parsing, which is in
  // ResponseHeadersParser.
  bool ReadFromBinary(const StringPiece& buf, MessageHandler* message_handler);

  // Serialize HTTP response header in HTTP format so it can be re-parsed
  bool WriteAsHttp(Writer* writer, MessageHandler* message_handler) const;

  // Compute caching information.  The current time is used to compute
  // the absolute time when a cache resource will expire.  The timestamp
  // is in milliseconds since 1970.  It is an error to call any of the
  // accessors before ComputeCaching is called.
  void ComputeCaching();
  bool IsCacheable() const;
  bool IsProxyCacheable() const;
  int64 CacheExpirationTimeMs() const;
  void SetDate(int64 date_ms);
  void SetLastModified(int64 last_modified_ms);

  // TODO(jmarantz): consider an alternative representation
  bool headers_complete() const { return has_major_version(); }

  int status_code() const;
  int64 timestamp_ms() const;
  bool has_timestamp_ms() const;
  void set_status_code(const int code);
  const char* reason_phrase() const;
  void set_reason_phrase(const StringPiece& reason_phrase);

  std::string ToString() const;

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

  // Parses a date header such as HttpAttributes::kDate or
  // HttpAttributes::kExpires, returning the timestamp as
  // number of milliseconds since 1970.
  bool ParseDateHeader(const char* attr, int64* date_ms) const;

  // Updates a date header using time specified as a number of milliseconds
  // since 1970.
  void UpdateDateHeader(const char* attr, int64 date_ms);

  // Set whole first line.
  void set_first_line(int major_version,
                      int minor_version,
                      int status_code,
                      const StringPiece& reason_phrase) {
    set_major_version(major_version);
    set_minor_version(minor_version);
    set_status_code(status_code);
    set_reason_phrase(reason_phrase);
  }

 private:
  friend class ResponseHeadersTest;

  bool cache_fields_dirty_;

  DISALLOW_COPY_AND_ASSIGN(ResponseHeaders);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_RESPONSE_HEADERS_H_
