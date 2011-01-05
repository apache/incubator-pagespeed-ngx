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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SIMPLE_META_DATA_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SIMPLE_META_DATA_H_

#include <stdlib.h>
#include "net/instaweb/util/public/meta_data.h"
#include <string>
#include "net/instaweb/util/public/string_multi_map.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Very basic implementation of HTTP headers.
//
// TODO(jmarantz): implement caching rules properly.
class SimpleMetaData : public MetaData {
 public:
  SimpleMetaData();
  virtual ~SimpleMetaData();

  virtual void Clear();

  // Raw access for random access to attribute name/value pairs.
  virtual int NumAttributes() const { return map_.num_values(); }
  virtual int NumAttributeNames() const { return map_.num_names(); }
  virtual const char* Name(int index) const { return map_.name(index); }
  virtual const char* Value(int index) const { return map_.value(index); }
  virtual bool Lookup(const char* name, CharStarVector* values) const;

  // Add a new header.
  virtual void Add(const StringPiece& name, const StringPiece& value);

  // Remove all headers by name.
  virtual void RemoveAll(const char* name);

  // Serialize HTTP response header to a stream.
  virtual bool Write(Writer* writer, MessageHandler* message_handler) const;
  // Serialize just the headers (not the version and response code line).
  virtual bool WriteHeaders(Writer* writer, MessageHandler* handler) const;

  // Parse a chunk of HTTP response header.  Returns number of bytes consumed.
  virtual int ParseChunk(const StringPiece& text, MessageHandler* handler);

  // Compute caching information.  The current time is used to compute
  // the absolute time when a cache resource will expire.  The timestamp
  // is in milliseconds since 1970.  It is an error to call any of the
  // accessors before ComputeCaching is called.
  virtual void ComputeCaching();
  virtual bool IsCacheable() const;
  virtual bool IsProxyCacheable() const;
  virtual int64 CacheExpirationTimeMs() const;
  virtual void SetDate(int64 date_ms);
  virtual void SetLastModified(int64 last_modified_ms);

  virtual bool headers_complete() const { return headers_complete_; }
  virtual void set_headers_complete(bool x) { headers_complete_ = x; }

  virtual int major_version() const { return major_version_; }
  virtual int minor_version() const { return minor_version_; }
  virtual int status_code() const { return status_code_; }
  virtual const char* reason_phrase() const {
    return reason_phrase_.c_str();
  }
  virtual int64 timestamp_ms() const { return timestamp_ms_; }
  virtual bool has_timestamp_ms() const;

  virtual void set_major_version(const int major_version) {
    major_version_ = major_version;
  }
  virtual void set_minor_version(const int minor_version) {
    minor_version_ = minor_version;
  }
  virtual void set_status_code(const int code) { status_code_ = code; }
  virtual void set_reason_phrase(const StringPiece& reason_phrase) {
    reason_phrase.CopyToString(&reason_phrase_);
  }

  virtual std::string ToString() const;

 private:
  bool GrabLastToken(const std::string& input, std::string* output);

  friend class SimpleMetaDataTest;

  StringMultiMapInsensitive map_;

  bool parsing_http_;
  bool parsing_value_;
  bool headers_complete_;
  bool cache_fields_dirty_;
  bool is_cacheable_;         // accurate only if !cache_fields_dirty_
  bool is_proxy_cacheable_;   // accurate only if !cache_fields_dirty_
  int64 expiration_time_ms_;  // accurate only if !cache_fields_dirty_
  int64 timestamp_ms_;        // accurate only if !cache_fields_dirty_
  std::string parse_name_;
  std::string parse_value_;

  int major_version_;
  int minor_version_;
  int status_code_;
  std::string reason_phrase_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMetaData);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SIMPLE_META_DATA_H_
