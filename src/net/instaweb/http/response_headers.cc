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

#include "net/instaweb/http/public/response_headers.h"

#include <cstdio>  // for fprintf, stderr, snprintf
#include <map>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/http.pb.h"  // for HttpResponseHeaders
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/headers.h"
#include "net/instaweb/http/public/meta_data.h"  // for HttpAttributes, etc
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_multi_map.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/writer.h"
#include "pagespeed/core/resource_util.h"

namespace net_instaweb {

class MessageHandler;

ResponseHeaders::ResponseHeaders() {
  proto_.reset(new HttpResponseHeaders);
  Clear();
}

ResponseHeaders::~ResponseHeaders() {
  Clear();
}

void ResponseHeaders::CopyFrom(const ResponseHeaders& other) {
  map_.reset(NULL);
  *(proto_.get()) = *(other.proto_.get());
  cache_fields_dirty_ = other.cache_fields_dirty_;
}

void ResponseHeaders::Clear() {
  Headers<HttpResponseHeaders>::Clear();

  proto_->set_cacheable(false);
  proto_->set_proxy_cacheable(false);   // accurate only if !cache_fields_dirty_
  proto_->clear_expiration_time_ms();
  proto_->clear_fetch_time_ms();
  proto_->clear_last_modified_time_ms();
  proto_->clear_status_code();
  proto_->clear_reason_phrase();
  proto_->clear_header();
  cache_fields_dirty_ = false;
}

int ResponseHeaders::status_code() const {
  return proto_->status_code();
}

void ResponseHeaders::set_status_code(int code) {
  proto_->set_status_code(code);
}

bool ResponseHeaders::has_status_code() const {
  return proto_->has_status_code();
}

const char* ResponseHeaders::reason_phrase() const {
  return proto_->has_reason_phrase()
      ? proto_->reason_phrase().c_str()
      : "(null)";
}

void ResponseHeaders::set_reason_phrase(const StringPiece& reason_phrase) {
  proto_->set_reason_phrase(reason_phrase.data(), reason_phrase.size());
}

int64 ResponseHeaders::last_modified_time_ms() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before last_modified_time_ms()";
  return proto_->last_modified_time_ms();
}

int64 ResponseHeaders::fetch_time_ms() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before fetch_time_ms()";
  return proto_->fetch_time_ms();
}

int64 ResponseHeaders::cache_ttl_ms() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before cache_ttl_ms()";
  return proto_->cache_ttl_ms();
}

bool ResponseHeaders::has_fetch_time_ms() const {
  return proto_->has_fetch_time_ms();
}

void ResponseHeaders::Add(const StringPiece& name, const StringPiece& value) {
  Headers<HttpResponseHeaders>::Add(name, value);
  cache_fields_dirty_ = true;
}

bool ResponseHeaders::Remove(const StringPiece& name,
                             const StringPiece& value) {
  if (Headers<HttpResponseHeaders>::Remove(name, value)) {
    cache_fields_dirty_ = true;
    return true;
  }
  return false;
}

bool ResponseHeaders::RemoveAll(const StringPiece& name) {
  if (Headers<HttpResponseHeaders>::RemoveAll(name)) {
    cache_fields_dirty_ = true;
    return true;
  }
  return false;
}

bool ResponseHeaders::RemoveAllFromSet(const StringSetInsensitive& names) {
  if (Headers<HttpResponseHeaders>::RemoveAllFromSet(names)) {
    cache_fields_dirty_ = true;
    return true;
  }
  return false;
}

void ResponseHeaders::Replace(
    const StringPiece& name, const StringPiece& value) {
  cache_fields_dirty_ = true;
  Headers<HttpResponseHeaders>::Replace(name, value);
}

void ResponseHeaders::UpdateFrom(const Headers<HttpResponseHeaders>& other) {
  cache_fields_dirty_ = true;
  Headers<HttpResponseHeaders>::UpdateFrom(other);
}

bool ResponseHeaders::WriteAsBinary(Writer* writer, MessageHandler* handler) {
  if (cache_fields_dirty_) {
    ComputeCaching();
  }
  return Headers<HttpResponseHeaders>::WriteAsBinary(writer, handler);
}

bool ResponseHeaders::ReadFromBinary(const StringPiece& buf,
                                     MessageHandler* message_handler) {
  cache_fields_dirty_ = false;
  return Headers<HttpResponseHeaders>::ReadFromBinary(buf, message_handler);
}

// Serialize meta-data to a binary stream.
bool ResponseHeaders::WriteAsHttp(Writer* writer, MessageHandler* handler)
    const {
  bool ret = true;
  char buf[100];
  snprintf(buf, sizeof(buf), "HTTP/%d.%d %d ",
           major_version(), minor_version(), status_code());
  ret &= writer->Write(buf, handler);
  ret &= writer->Write(reason_phrase(), handler);
  ret &= writer->Write("\r\n", handler);
  ret &= Headers<HttpResponseHeaders>::WriteAsHttp(writer, handler);
  return ret;
}

// Specific information about cache.  This is all embodied in the
// headers but is centrally parsed so we can try to get it right.
bool ResponseHeaders::IsCacheable() const {
  // We do not compute caching from accessors so that the
  // accessors can be easier to call from multiple threads
  // without mutexing.
  DCHECK(!cache_fields_dirty_) << "Call ComputeCaching() before IsCacheable()";
  return proto_->cacheable();
}

bool ResponseHeaders::IsProxyCacheable() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before IsProxyCacheable()";
  return proto_->proxy_cacheable();
}

// Returns the ms-since-1970 absolute time when this resource
// should be expired out of caches.
int64 ResponseHeaders::CacheExpirationTimeMs() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before CacheExpirationTimeMs()";
  return proto_->expiration_time_ms();
}

void ResponseHeaders::SetDateAndCaching(
    int64 date_ms, int64 ttl_ms, const StringPiece& cache_control_suffix) {
  SetDate(date_ms);
  // Note: We set both Expires and Cache-Control headers so that legacy
  // HTTP/1.0 browsers and proxies correctly cache these resources.
  SetTimeHeader(HttpAttributes::kExpires, date_ms + ttl_ms);
  Replace(HttpAttributes::kCacheControl,
          StrCat("max-age=", Integer64ToString(ttl_ms / Timer::kSecondMs),
                 cache_control_suffix));
}

void ResponseHeaders::SetTimeHeader(const StringPiece& header, int64 time_ms) {
  GoogleString time_string;
  if (ConvertTimeToString(time_ms, &time_string)) {
    Replace(header, time_string);
  }
}

bool ResponseHeaders::Sanitize() {
  bool cookie = RemoveAll(HttpAttributes::kSetCookie);
  bool cookie2 = RemoveAll(HttpAttributes::kSetCookie2);
  return cookie || cookie2;
}

bool ResponseHeaders::VaryCacheable() const {
  if (IsCacheable()) {
    ConstStringStarVector values;
    Lookup(HttpAttributes::kVary, &values);
    bool vary_uncacheable = false;
    for (int i = 0, n = values.size(); i < n; ++i) {
      StringPiece val(*values[i]);
      if (!val.empty() &&
          !StringCaseEqual(HttpAttributes::kAcceptEncoding, val)) {
        vary_uncacheable = true;
        break;
      }
    }
    return !vary_uncacheable;
  } else {
    return false;
  }
}

void ResponseHeaders::ComputeCaching() {
  pagespeed::Resource resource;
  for (int i = 0, n = NumAttributes(); i < n; ++i) {
    resource.AddResponseHeader(Name(i), Value(i));
  }
  resource.SetResponseStatusCode(proto_->status_code());

  ConstStringStarVector values;
  int64 date;
  bool has_date = ParseDateHeader(HttpAttributes::kDate, &date);
  // Compute the timestamp if we can find it
  if (has_date) {
    proto_->set_fetch_time_ms(date);
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
  // 304 Not Modified is not cacheable since as an intermediate server, we have
  // no context.
  bool status_cacheable =
      ((status_code() ==
        HttpStatus::kRememberFetchFailedOrNotCacheableStatusCode) ||
       (status_code() != HttpStatus::kNotModified &&
        pagespeed::resource_util::IsCacheableResourceStatusCode(
            status_code())));
  int64 cache_ttl_ms;
  bool explicit_cacheable =
      pagespeed::resource_util::GetFreshnessLifetimeMillis(
          resource, &cache_ttl_ms) && has_fetch_time_ms();

  proto_->set_cacheable(has_date &&
                        !explicit_no_cache &&
                        (explicit_cacheable || likely_static) &&
                        status_cacheable);
  if (proto_->cacheable()) {
    // TODO(jmarantz): check "Age" resource and use that to reduce
    // the expiration_time_ms_.  This is, says, bmcquade@google.com,
    // typically use to indicate how long a resource has been sitting
    // in a proxy-cache.
    if (!explicit_cacheable) {
      // implicitly cached items stay alive in our system for 5 minutes
      // TODO(jmarantz): consider making this a flag, or getting some
      // other heuristic value from the PageSpeed libraries.
      cache_ttl_ms = kImplicitCacheTtlMs;
    }
    proto_->set_cache_ttl_ms(cache_ttl_ms);
    proto_->set_expiration_time_ms(proto_->fetch_time_ms() + cache_ttl_ms);

    // Assume it's proxy cacheable.  Then iterate over all the headers
    // with key HttpAttributes::kCacheControl, and all the comma-separated
    // values within those values, and look for 'private'.
    proto_->set_proxy_cacheable(true);
    values.clear();
    if (Lookup(HttpAttributes::kCacheControl, &values)) {
      for (int i = 0, n = values.size(); i < n; ++i) {
        const GoogleString* cache_control = values[i];
        pagespeed::resource_util::DirectiveMap directive_map;
        if ((cache_control != NULL)
            && pagespeed::resource_util::GetHeaderDirectives(
                *cache_control, &directive_map)) {
          pagespeed::resource_util::DirectiveMap::iterator p =
              directive_map.find("private");
          if (p != directive_map.end()) {
            proto_->set_proxy_cacheable(false);
            break;
          }
        }
      }
    }
  } else {
    proto_->set_expiration_time_ms(0);
    proto_->set_proxy_cacheable(false);
  }
  cache_fields_dirty_ = false;
}

GoogleString ResponseHeaders::ToString() const {
  GoogleString str;
  StringWriter writer(&str);
  WriteAsHttp(&writer, NULL);
  return str;
}

void ResponseHeaders::SetStatusAndReason(HttpStatus::Code code) {
  set_status_code(code);
  set_reason_phrase(HttpStatus::GetReasonPhrase(code));
}

bool ResponseHeaders::ParseTime(const char* time_str, int64* time_ms) {
  return pagespeed::resource_util::ParseTimeValuedHeader(time_str, time_ms);
}

// Content-coding values are case-insensitive:
// http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html
// See Section 3.5
bool ResponseHeaders::IsGzipped() const {
  ConstStringStarVector v;
  bool found = Lookup(HttpAttributes::kContentEncoding, &v);
  if (found) {
    for (int i = 0, n = v.size(); i < n; ++i) {
      if ((v[i] != NULL) && StringCaseEqual(*v[i], HttpAttributes::kGzip)) {
        return true;
      }
    }
  }
  return false;
}

bool ResponseHeaders::WasGzippedLast() const {
  ConstStringStarVector v;
  bool found = Lookup(HttpAttributes::kContentEncoding, &v);
  if (found) {
    int index = v.size() - 1;
    if ((index > -1) && (v[index] != NULL) &&
        StringCaseEqual(*v[index], HttpAttributes::kGzip)) {
      return true;
    }
  }
  return false;
}

// TODO(sligocki): Perhaps we should take in a URL here and use that to
// guess Content-Type as well. See Resource::DetermineContentType().
const ContentType* ResponseHeaders::DetermineContentType() const {
  const ContentType* content_type = NULL;

  ConstStringStarVector content_types;
  if (Lookup(HttpAttributes::kContentType, &content_types)) {
    for (int i = 0, n = content_types.size(); (i < n) && (content_type == NULL);
         ++i) {
      if (content_types[i] != NULL) {
        content_type = MimeTypeToContentType(*(content_types[i]));
      }
    }
  }

  return content_type;
}



bool ResponseHeaders::ParseDateHeader(
    const StringPiece& attr, int64* date_ms) const {
  const char* date_string = Lookup1(attr);
  return (date_string != NULL) && ConvertStringToTime(date_string, date_ms);
}

bool ResponseHeaders::IsDateLaterThan(int64 time_ms) const {
  int64 date_ms;
  if (!ParseDateHeader(HttpAttributes::kDate, &date_ms)) {
    LOG(DFATAL) << "Could not obtain date header.";
    return true;  // If no date header return true;
  }
  return date_ms > time_ms;
}

void ResponseHeaders::UpdateDateHeader(const StringPiece& attr, int64 date_ms) {
  RemoveAll(attr);
  GoogleString buf;
  if (ConvertTimeToString(date_ms, &buf)) {
    Add(attr, buf);
  }
}

// TODO(sligocki): Unite this with parsing code in ResponseHeadersParser.
void ResponseHeaders::ParseFirstLine(const StringPiece& first_line) {
  int major_version, minor_version, status;
  // We reserve enough to avoid buffer overflow on sscanf command.
  GoogleString reason_phrase(first_line.size(), '\0');
  char* reason_phrase_cstr = &reason_phrase[0];
  if (4 == sscanf(first_line.as_string().c_str(), "HTTP/%d.%d %d %[^\n\t]s",
                  &major_version, &minor_version, &status,
                  reason_phrase_cstr)) {
    set_first_line(major_version, minor_version, status, reason_phrase_cstr);
  } else {
    LOG(WARNING) << "Could not parse first line: " << first_line;
  }
}

namespace {

const char* BoolToString(bool b) {
  return ((b) ? "true" : "false");
}

}  // namespace

void ResponseHeaders::DebugPrint() const {
  fprintf(stderr, "%s\n", ToString().c_str());
  fprintf(stderr, "cache_fields_dirty_ = %s\n",
          BoolToString(cache_fields_dirty_));
  if (!cache_fields_dirty_) {
    fprintf(stderr, "expiration_time_ms_ = %s\n",
            Integer64ToString(proto_->expiration_time_ms()).c_str());
    fprintf(stderr, "last_modified_time_ms_ = %s\n",
            Integer64ToString(last_modified_time_ms()).c_str());
    fprintf(stderr, "fetch_time_ms_ = %s\n",
            Integer64ToString(proto_->fetch_time_ms()).c_str());
    fprintf(stderr, "cacheable_ = %s\n", BoolToString(proto_->cacheable()));
    fprintf(stderr, "proxy_cacheable_ = %s\n",
            BoolToString(proto_->proxy_cacheable()));
  }
}

bool ResponseHeaders::FindContentLength(int64* content_length) {
  const char* val = Lookup1(HttpAttributes::kContentLength);
  return (val != NULL) && StringToInt64(val, content_length);
}

}  // namespace net_instaweb
