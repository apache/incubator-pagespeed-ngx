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

#include "net/instaweb/util/public/simple_meta_data.h"

#include <stdio.h>
#include "base/logging.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/writer.h"
#include "pagespeed/core/resource_util.h"

namespace {

const int64 TIME_UNINITIALIZED = -1;
const int64 kImplicitCacheTtlMs = 5 * net_instaweb::Timer::kMinuteMs;

}

namespace net_instaweb {

SimpleMetaData::SimpleMetaData() {
  Clear();
}

SimpleMetaData::~SimpleMetaData() {
  Clear();
}

void SimpleMetaData::Clear() {
  map_.Clear();

  parsing_http_ = false;
  parsing_value_ = false;
  headers_complete_ = false;
  cache_fields_dirty_ = false;
  is_cacheable_ = false;
  is_proxy_cacheable_ = false;   // accurate only if !cache_fields_dirty_
  expiration_time_ms_= TIME_UNINITIALIZED;
  timestamp_ms_= TIME_UNINITIALIZED;
  parse_name_.clear();
  parse_value_.clear();

  major_version_ = 0;
  minor_version_ = 0;
  status_code_ = 0;
  reason_phrase_.clear();
}

bool SimpleMetaData::Lookup(const char* name, CharStarVector* values) const {
  return map_.Lookup(name, values);
}

void SimpleMetaData::Add(const StringPiece& name, const StringPiece& value) {
  map_.Add(name, value);
  cache_fields_dirty_ = true;
}

void SimpleMetaData::RemoveAll(const char* name) {
  map_.RemoveAll(name);
  cache_fields_dirty_ = true;
}

// Serialize meta-data to a stream.
bool SimpleMetaData::Write(Writer* writer, MessageHandler* handler) const {
  bool ret = true;
  char buf[100];
  snprintf(buf, sizeof(buf), "HTTP/%d.%d %d ",
           major_version_, minor_version_, status_code_);
  ret &= writer->Write(buf, handler);
  ret &= writer->Write(reason_phrase_, handler);
  ret &= writer->Write("\r\n", handler);
  ret &= WriteHeaders(writer, handler);
  return ret;
}

bool SimpleMetaData::WriteHeaders(Writer* writer,
                                  MessageHandler* handler) const {
  bool ret = true;
  for (int i = 0, n = map_.num_values(); ret && (i < n); ++i) {
    ret &= writer->Write(map_.name(i), handler);
    ret &= writer->Write(": ", handler);
    ret &= writer->Write(map_.value(i), handler);
    ret &= writer->Write("\r\n", handler);
  }
  ret &= writer->Write("\r\n", handler);
  return ret;
}

// TODO(jmaessen): http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
// I bet we're doing this wrong:
//  Header fields can be extended over multiple lines by preceding each extra
//  line with at least one SP or HT.
int SimpleMetaData::ParseChunk(const StringPiece& text,
                               MessageHandler* handler) {
  CHECK(!headers_complete_);
  int num_consumed = 0;
  int num_bytes = text.size();

  for (; num_consumed < num_bytes; ++num_consumed) {
    char c = text[num_consumed];
    if ((c == '/') && (parse_name_ == "HTTP")) {
      if (major_version_ != 0) {
        handler->Message(kError, "Multiple HTTP Lines");
      } else {
        parsing_http_ = true;
        parsing_value_ = true;
      }
    } else if (!parsing_value_ && (c == ':')) {
      parsing_value_ = true;
    } else if (c == '\r') {
      // Just ignore CRs for now, and break up headers on newlines for
      // simplicity.  It's not clear to me if it's important that we
      // reject headers that lack the CR in front of the LF.
    } else if (c == '\n') {
      if (parse_name_.empty()) {
        // blank line.  This marks the end of the headers.
        ++num_consumed;
        headers_complete_ = true;
        ComputeCaching();
        break;
      }
      if (parsing_http_) {
        // Parsing "1.0 200 OK\r", using sscanf for the integers, and
        // private method GrabLastToken for the "OK".
        if ((sscanf(parse_value_.c_str(), "%d.%d %d ",  // NOLINT
                    &major_version_, &minor_version_, &status_code_) != 3) ||
            !GrabLastToken(parse_value_, &reason_phrase_)) {
          // TODO(jmarantz): capture the filename/url, track the line numbers.
          handler->Message(kError, "Invalid HTML headers: %s",
                           parse_value_.c_str());
        }
        parsing_http_ = false;
      } else {
        Add(parse_name_.c_str(), parse_value_.c_str());
      }
      parsing_value_ = false;
      parse_name_.clear();
      parse_value_.clear();
    } else if (parsing_value_) {
      // Skip leading whitespace
      if (!parse_value_.empty() || !isspace(c)) {
        parse_value_ += c;
      }
    } else {
      parse_name_ += c;
    }
  }
  return num_consumed;
}

// Specific information about cache.  This is all embodied in the
// headers but is centrally parsed so we can try to get it right.
bool SimpleMetaData::IsCacheable() const {
  // We do not compute caching from accessors so that the
  // accessors can be easier to call from multiple threads
  // without mutexing.
  CHECK(!cache_fields_dirty_);
  return is_cacheable_;
}

bool SimpleMetaData::IsProxyCacheable() const {
  CHECK(!cache_fields_dirty_);
  return is_proxy_cacheable_;
}

// Returns the ms-since-1970 absolute time when this resource
// should be expired out of caches.
int64 SimpleMetaData::CacheExpirationTimeMs() const {
  CHECK(!cache_fields_dirty_);
  return expiration_time_ms_;
}

void SimpleMetaData::SetDate(int64 date_ms) {
  std::string time_string;
  if (ConvertTimeToString(date_ms, &time_string)) {
    Add("Date", time_string.c_str());
  }
}

void SimpleMetaData::SetLastModified(int64 last_modified_ms) {
  std::string time_string;
  if (ConvertTimeToString(last_modified_ms, &time_string)) {
    Add(HttpAttributes::kLastModified, time_string.c_str());
  }
}

void SimpleMetaData::ComputeCaching() {
  pagespeed::Resource resource;
  for (int i = 0, n = NumAttributes(); i < n; ++i) {
    resource.AddResponseHeader(Name(i), Value(i));
  }
  resource.SetResponseStatusCode(status_code_);

  CharStarVector values;
  int64 date;
  // Compute the timestamp if we can find it
  if (Lookup("Date", &values) && (values.size() == 1) &&
      ConvertStringToTime(values[0], &date)) {
    timestamp_ms_ = date;
  }

  // TODO(jmarantz): Should we consider as cacheable a resource
  // that simply has no cacheable hints at all?  For now, let's
  // make that assumption.  We should review this policy with bmcquade,
  // souders, etc, but first let's try to measure some value with this
  // optimistic intrepretation.
  //
  // TODO(jmarantz): get from bmcquade a comprehensive ways in which these
  // policies will differ for Instaweb vs Pagespeed.
  bool explicit_no_cache =
      pagespeed::resource_util::HasExplicitNoCacheDirective(resource);
  bool likely_static =
      pagespeed::resource_util::IsLikelyStaticResource(resource);

  // status_cacheable implies that either the resource content was
  // cacheable, or the status code indicated some other aspect of
  // our system that we want to remember in the cache, such as
  // that fact that a fetch failed for a resource, and we don't want
  // to try again until some time has passed.
  bool status_cacheable =
      ((status_code_ == HttpStatus::kRememberNotFoundStatusCode) ||
       pagespeed::resource_util::IsCacheableResourceStatusCode(status_code_));
  int64 freshness_lifetime_ms;
  bool explicit_cacheable =
      pagespeed::resource_util::GetFreshnessLifetimeMillis(
          resource, &freshness_lifetime_ms) && has_timestamp_ms();
  is_cacheable_ = (!explicit_no_cache &&
                   (explicit_cacheable || likely_static) &&
                   status_cacheable);

  if (is_cacheable_) {
    if (explicit_cacheable) {
      // TODO(jmarantz): check "Age" resource and use that to reduce
      // the expiration_time_ms_.  This is, says, bmcquade@google.com,
      // typically use to indicate how long a resource has been sitting
      // in a proxy-cache.
      expiration_time_ms_ = timestamp_ms_ + freshness_lifetime_ms;
    } else {
      // implicitly cached items stay alive in our system for 5 minutes
      // TODO(jmarantz): consider making this a flag, or getting some
      // other heuristic value from the PageSpeed libraries.
      expiration_time_ms_ = timestamp_ms_ + kImplicitCacheTtlMs;
    }

    // Assume it's proxy cacheable.  Then iterate over all the headers
    // with key HttpAttributes::kCacheControl, and all the comma-separated
    // values within those values, and look for 'private'.
    is_proxy_cacheable_ = true;
    values.clear();
    if (Lookup(HttpAttributes::kCacheControl, &values)) {
      for (int i = 0, n = values.size(); i < n; ++i) {
        const char* cache_control = values[i];
        pagespeed::resource_util::DirectiveMap directive_map;
        if (pagespeed::resource_util::GetHeaderDirectives(
                cache_control, &directive_map)) {
          pagespeed::resource_util::DirectiveMap::iterator p =
              directive_map.find("private");
          if (p != directive_map.end()) {
            is_proxy_cacheable_ = false;
            break;
          }
        }
      }
    }
  } else {
    expiration_time_ms_ = 0;
    is_proxy_cacheable_ = false;
  }
  cache_fields_dirty_ = false;
}

bool SimpleMetaData::has_timestamp_ms() const {
  return timestamp_ms_ != TIME_UNINITIALIZED;
}

std::string SimpleMetaData::ToString() const {
  std::string str;
  StringWriter writer(&str);
  Write(&writer, NULL);
  return str;
}

// Grabs the last non-whitespace token from 'input' and puts it in 'output'.
bool SimpleMetaData::GrabLastToken(const std::string& input,
                                   std::string* output) {
  bool ret = false;
  // Safely grab the response code string from the end of parse_value_.
  int last_token_char = -1;
  for (int i = input.size() - 1; i >= 0; --i) {
    char c = input[i];
    if (isspace(c)) {
      if (last_token_char >= 0) {
        // We found the whole token.
        const char* token_start = input.c_str() + i + 1;
        int token_len = last_token_char - i;
        output->append(token_start, token_len);
        ret = true;
        break;
      }
    } else if (last_token_char == -1) {
      last_token_char = i;
    }
  }
  return ret;
}

}  // namespace net_instaweb
