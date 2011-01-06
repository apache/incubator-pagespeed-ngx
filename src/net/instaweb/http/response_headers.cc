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

#include "base/logging.h"
#include "net/instaweb/http/http.pb.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/string_multi_map.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/writer.h"
#include "pagespeed/core/resource_util.h"

namespace {

const int64 kImplicitCacheTtlMs = 5 * net_instaweb::Timer::kMinuteMs;

}  // namespace

namespace net_instaweb {

ResponseHeaders::ResponseHeaders() {
  proto_.reset(new HttpResponseHeaders);
  Clear();
}

ResponseHeaders::~ResponseHeaders() {
  Clear();
}

void ResponseHeaders::Clear() {
  map_.reset(NULL);

  proto_->set_cacheable(false);
  proto_->set_proxy_cacheable(false);   // accurate only if !cache_fields_dirty_
  proto_->clear_expiration_time_ms();
  proto_->clear_timestamp_ms();
  proto_->clear_major_version();
  proto_->clear_minor_version();
  proto_->clear_status_code();
  proto_->clear_reason_phrase();
  proto_->clear_header();
  cache_fields_dirty_ = false;
}

int ResponseHeaders::NumAttributes() const {
  return proto_->header_size();
}

const char* ResponseHeaders::Name(int i) const {
  return proto_->header(i).name().c_str();
}

const char* ResponseHeaders::Value(int i) const {
  return proto_->header(i).value().c_str();
}

int ResponseHeaders::major_version() const {
  return proto_->major_version();
}

bool ResponseHeaders::has_major_version() const {
  return proto_->has_major_version();
}

int ResponseHeaders::minor_version() const {
  return proto_->minor_version();
}

int ResponseHeaders::status_code() const {
  return proto_->status_code();
}

const char* ResponseHeaders::reason_phrase() const {
  return proto_->reason_phrase().c_str();
}

int64 ResponseHeaders::timestamp_ms() const {
  return proto_->timestamp_ms();
}

bool ResponseHeaders::has_timestamp_ms() const {
  return proto_->has_timestamp_ms();
}

void ResponseHeaders::set_major_version(int major_version) {
  proto_->set_major_version(major_version);
}

void ResponseHeaders::set_minor_version(int minor_version) {
  proto_->set_minor_version(minor_version);
}

void ResponseHeaders::set_status_code(int code) {
  proto_->set_status_code(code);
}

void ResponseHeaders::set_reason_phrase(const StringPiece& reason_phrase) {
  proto_->set_reason_phrase(reason_phrase.data(), reason_phrase.size());
}

void ResponseHeaders::PopulateMap() const {
  if (map_.get() == NULL) {
    map_.reset(new StringMultiMapInsensitive);
    for (int i = 0, n = NumAttributes(); i < n; ++i) {
      map_->Add(Name(i), Value(i));
    }
  }
}

int ResponseHeaders::NumAttributeNames() const {
  PopulateMap();
  return map_->num_names();
}

bool ResponseHeaders::Lookup(const char* name, CharStarVector* values) const {
  PopulateMap();
  return map_->Lookup(name, values);
}

void ResponseHeaders::Add(const StringPiece& name, const StringPiece& value) {
  // TODO(jmarantz): Parse comma-separated values.  bmcquade sez:
  // you probably want to normalize these by splitting on commas and
  // adding a separate k,v pair for each comma-separated value. then
  // it becomes very easy to do things like search for individual
  // Content-Type tokens. Otherwise the client has to assume that
  // every single value could be comma-separated and they have to
  // parse it as such.  the list of header names that are not safe to
  // comma-split is at
  // http://src.chromium.org/viewvc/chrome/trunk/src/net/http/http_util.cc
  // (search for IsNonCoalescingHeader)

  NameValue* name_value = proto_->add_header();
  name_value->set_name(name.data(), name.size());
  name_value->set_value(value.data(), value.size());
  if (map_.get() != NULL) {
    map_->Add(name, value);
  }
  cache_fields_dirty_ = true;
}

void ResponseHeaders::RemoveAll(const char* name) {
  // Protobufs lack a convenient remove method for array elements, so
  // we copy the data into the map and do the remove there, then
  // reconstruct the protobuf.
  PopulateMap();
  CharStarVector values;
  if (map_->Lookup(name, &values)) {
    proto_->clear_header();
    map_->RemoveAll(name);
    for (int i = 0, n = map_->num_values(); i < n; ++i) {
      NameValue* name_value = proto_->add_header();
      name_value->set_name(map_->name(i));
      name_value->set_value(map_->value(i));
    }
    cache_fields_dirty_ = true;
  }
}

bool ResponseHeaders::WriteAsBinary(Writer* writer, MessageHandler* handler) {
  if (cache_fields_dirty_) {
    ComputeCaching();
  }
  std::string buf;
  {
    StringOutputStream sstream(&buf);
    proto_->SerializeToZeroCopyStream(&sstream);
  }
  return writer->Write(buf, handler);
}

bool ResponseHeaders::ReadFromBinary(const StringPiece& buf,
                                     MessageHandler* message_handler) {
  Clear();
  ArrayInputStream input(buf.data(), buf.size());
  cache_fields_dirty_ = false;
  return proto_->ParseFromZeroCopyStream(&input);
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
  for (int i = 0, n = NumAttributes(); ret && (i < n); ++i) {
    ret &= writer->Write(Name(i), handler);
    ret &= writer->Write(": ", handler);
    ret &= writer->Write(Value(i), handler);
    ret &= writer->Write("\r\n", handler);
  }
  ret &= writer->Write("\r\n", handler);
  return ret;
}

// Specific information about cache.  This is all embodied in the
// headers but is centrally parsed so we can try to get it right.
bool ResponseHeaders::IsCacheable() const {
  // We do not compute caching from accessors so that the
  // accessors can be easier to call from multiple threads
  // without mutexing.
  CHECK(!cache_fields_dirty_);
  return proto_->cacheable();
}

bool ResponseHeaders::IsProxyCacheable() const {
  CHECK(!cache_fields_dirty_);
  return proto_->proxy_cacheable();
}

// Returns the ms-since-1970 absolute time when this resource
// should be expired out of caches.
int64 ResponseHeaders::CacheExpirationTimeMs() const {
  CHECK(!cache_fields_dirty_);
  return proto_->expiration_time_ms();
}

void ResponseHeaders::SetDate(int64 date_ms) {
  std::string time_string;
  if (ConvertTimeToString(date_ms, &time_string)) {
    Add("Date", time_string.c_str());
  }
}

void ResponseHeaders::SetLastModified(int64 last_modified_ms) {
  std::string time_string;
  if (ConvertTimeToString(last_modified_ms, &time_string)) {
    Add(HttpAttributes::kLastModified, time_string.c_str());
  }
}

void ResponseHeaders::ComputeCaching() {
  pagespeed::Resource resource;
  for (int i = 0, n = NumAttributes(); i < n; ++i) {
    resource.AddResponseHeader(Name(i), Value(i));
  }
  resource.SetResponseStatusCode(proto_->status_code());

  CharStarVector values;
  int64 date;
  // Compute the timestamp if we can find it
  if (Lookup("Date", &values) && (values.size() == 1) &&
      ConvertStringToTime(values[0], &date)) {
    proto_->set_timestamp_ms(date);
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
      ((status_code() == HttpStatus::kRememberNotFoundStatusCode) ||
       pagespeed::resource_util::IsCacheableResourceStatusCode(status_code()));
  int64 freshness_lifetime_ms;
  bool explicit_cacheable =
      pagespeed::resource_util::GetFreshnessLifetimeMillis(
          resource, &freshness_lifetime_ms) && has_timestamp_ms();
  proto_->set_cacheable(!explicit_no_cache &&
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
      freshness_lifetime_ms = kImplicitCacheTtlMs;
    }
    proto_->set_expiration_time_ms(proto_->timestamp_ms() +
                                  freshness_lifetime_ms);

    // Assume it's proxy cacheable.  Then iterate over all the headers
    // with key HttpAttributes::kCacheControl, and all the comma-separated
    // values within those values, and look for 'private'.
    proto_->set_proxy_cacheable(true);
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

std::string ResponseHeaders::ToString() const {
  std::string str;
  StringWriter writer(&str);
  WriteAsHttp(&writer, NULL);
  return str;
}

void ResponseHeaders::SetStatusAndReason(HttpStatus::Code code) {
  set_status_code(code);
  set_reason_phrase(HttpStatus::GetReasonPhrase(code));
}

}  // namespace net_instaweb
