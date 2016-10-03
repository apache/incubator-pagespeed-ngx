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

#include "pagespeed/kernel/http/response_headers.h"

#include <algorithm>                    // for min
#include <cstddef>
#include <cstdio>     // for fprintf, stderr, snprintf
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "strings/stringpiece_utils.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/escaping.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/time_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/http/caching_headers.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/headers.h"
#include "pagespeed/kernel/http/http.pb.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/kernel/http/query_params.h"
#include "pagespeed/kernel/http/request_headers.h"

namespace net_instaweb {

class MessageHandler;

// Specifies the maximum amount of forward drift we'll allow for a Date
// timestamp.  E.g. if it's 3:00:00 and the Date header says its 3:01:00,
// we'll leave the date-header in the future.  But if it's 3:03:01 then
// we'll set it back to 3:00:00 exactly in FixDateHeaders.
const int64 kMaxAllowedDateDriftMs = 3L * net_instaweb::Timer::kMinuteMs;

ResponseHeaders::ResponseHeaders(const ResponseHeaders& other) {
  CopyFrom(other);
}
ResponseHeaders& ResponseHeaders::operator=(const ResponseHeaders& other) {
  if (&other != this) {
    CopyFrom(other);
  }
  return *this;
}

ResponseHeaders::~ResponseHeaders() {
  Clear();
}

void ResponseHeaders::Init(const HttpOptions& http_options) {
  http_options_ = http_options;

  Headers<HttpResponseHeaders>::SetProto(new HttpResponseHeaders);
  Clear();
}

namespace {

// TODO(pulkitg): Change kRefreshExpirePercent to be configurable via flag.
const int64 kRefreshExpirePercent = 80;

void ApplyTimeDelta(const char* attr, int64 delta_ms,
                    ResponseHeaders* headers) {
  int64 time_ms;
  if (headers->ParseDateHeader(attr, &time_ms)) {
    int64 adjusted_time_ms = time_ms + delta_ms;
    if (adjusted_time_ms > 0) {
      headers->SetTimeHeader(attr, time_ms + delta_ms);
    }
  }
}

}  // namespace

bool ResponseHeaders::IsImminentlyExpiring(
    int64 start_date_ms, int64 expire_ms, int64 now_ms,
    const HttpOptions& http_options) {
  // Consider a resource with 5 minute expiration time (the default
  // assumed by mod_pagespeed when a potentialy cacheable resource
  // lacks a cache control header, which happens a lot).  If the
  // origin TTL was 5 minutes and 4 minutes have expired, then we want
  // to re-fetch it so that we can avoid expiring the data.
  //
  // If we don't do this, then every 5 minutes, someone will see
  // this page unoptimized.  In a site with very low QPS, including
  // test instances of a site, this can happen quite often.
  const int64 ttl_ms = expire_ms - start_date_ms;
  // Only proactively refresh resources that have at least our
  // default expiration of 5 minutes.
  //
  // TODO(jmaessen): Lower threshold when If-Modified-Since checking is in
  // place; consider making this settable.
  // TODO(pradnya): We will freshen only if ttl is greater than the default
  // implicit ttl. If the implicit ttl has been overridden by a site, we will
  // not honor it here. Fix that.

  if (ttl_ms < http_options.implicit_cache_ttl_ms) {
    return false;
  }
  int64 freshen_threshold = std::min(
      http_options.implicit_cache_ttl_ms,
      ((100 - kRefreshExpirePercent) * ttl_ms) / 100);
  return (expire_ms - now_ms < freshen_threshold);
}

void ResponseHeaders::FixDateHeaders(int64 now_ms) {
  int64 date_ms = 0;
  bool has_date = true;

  if (cache_fields_dirty_) {
    // We don't want to call ComputeCaching() right here because it's expensive,
    // and if we decide we need to alter the Date header then we'll have to
    // recompute Caching later anyway.
    has_date = ParseDateHeader(HttpAttributes::kDate, &date_ms);
  } else if (proto()->has_date_ms()) {
    date_ms = proto()->date_ms();
  } else {
    has_date = false;
  }

  // If the Date is missing, set one.  If the Date is present but is older
  // than now_ms, correct it.  Also correct it if it's more than a fixed
  // amount in the future.
  if (!has_date || (date_ms < now_ms) ||
      (date_ms > now_ms + kMaxAllowedDateDriftMs)) {
    bool recompute_caching = !cache_fields_dirty_;
    SetDate(now_ms);
    if (has_date) {
      int64 delta_ms = now_ms - date_ms;
      ApplyTimeDelta(HttpAttributes::kExpires, delta_ms, this);

      // TODO(jmarantz): This code was refactored from http_dump_url_fetcher.cc,
      // which was adjusting the LastModified header when the date was fixed.
      // I wrote that code originally and can't think now why that would make
      // sense, so I'm commenting this out for now.  If this turns out to be
      // a problem replaying old Slurps then this code should be re-instated,
      // possibly based on a flag passed in.
      //     ApplyTimeDelta(HttpAttributes::kLastModified, delta_ms, this);
    } else {
      SetDate(now_ms);
      // TODO(jmarantz): see above.
      //     SetTimeHeader(HttpAttributes::kLastModified, now_ms);

      // If there was no Date header, there cannot possibly be any rationality
      // to an Expires header.  So remove it for now.  We can always add it in
      // if Page Speed computed a TTL.
      RemoveAll(HttpAttributes::kExpires);

      // If Expires was previously set, but there was no date, then
      // try to compute it from the TTL & the current time.  If there
      // was no TTL then we should just remove the Expires headers.
      int64 expires_ms;
      if (ParseDateHeader(HttpAttributes::kExpires, &expires_ms)) {
        ComputeCaching();

        // Page Speed's caching libraries will now compute the expires
        // for us based on the TTL and the date we just set, so we can
        // set a corrected expires header.
        if (proto()->has_expiration_time_ms()) {
          SetTimeHeader(HttpAttributes::kExpires,
                        proto()->expiration_time_ms());
        }
        cache_fields_dirty_ = false;
        recompute_caching = false;
      }
    }

    if (recompute_caching) {
      ComputeCaching();
    }
  }
}

void ResponseHeaders::CopyFrom(const ResponseHeaders& other) {
  Headers<HttpResponseHeaders>::Clear();
  Headers<HttpResponseHeaders>::CopyProto(*other.proto());
  cache_fields_dirty_ = other.cache_fields_dirty_;
  force_cache_ttl_ms_ = other.force_cache_ttl_ms_;
  force_cached_ = other.force_cached_;
  http_options_ = other.http_options_;
}

void ResponseHeaders::Clear() {
  Headers<HttpResponseHeaders>::Clear();

  HttpResponseHeaders* proto = mutable_proto();
  proto->set_browser_cacheable(false);  // accurate iff !cache_fields_dirty_
  proto->set_requires_proxy_revalidation(false);
  proto->set_requires_browser_revalidation(false);
  proto->clear_expiration_time_ms();
  proto->clear_date_ms();
  proto->clear_last_modified_time_ms();
  proto->clear_status_code();
  proto->clear_reason_phrase();
  proto->clear_header();
  proto->clear_is_implicitly_cacheable();
  cache_fields_dirty_ = false;
  force_cache_ttl_ms_ = -1;
  force_cached_ = false;

  // Note: http_options_ are not cleared here!
  // Those should only be set at construction time and never mutated.
}

int ResponseHeaders::status_code() const {
  return proto()->status_code();
}

void ResponseHeaders::set_status_code(int code) {
  cache_fields_dirty_ = true;
  mutable_proto()->set_status_code(code);
}

bool ResponseHeaders::has_status_code() const {
  return proto()->has_status_code();
}

const char* ResponseHeaders::reason_phrase() const {
  return proto()->has_reason_phrase()
      ? proto()->reason_phrase().c_str()
      : "(null)";
}

void ResponseHeaders::set_reason_phrase(const StringPiece& reason_phrase) {
  mutable_proto()->set_reason_phrase(reason_phrase.data(),
                                     reason_phrase.size());
}

bool ResponseHeaders::has_last_modified_time_ms() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before last_modified_time_ms()";
  return proto()->has_last_modified_time_ms();
}

int64 ResponseHeaders::last_modified_time_ms() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before last_modified_time_ms()";
  return proto()->last_modified_time_ms();
}

int64 ResponseHeaders::date_ms() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before date_ms()";
  return proto()->date_ms();
}

int64 ResponseHeaders::cache_ttl_ms() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before cache_ttl_ms()";
  return proto()->cache_ttl_ms();
}

bool ResponseHeaders::has_date_ms() const {
  return proto()->has_date_ms();
}

bool ResponseHeaders::is_implicitly_cacheable() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before is_implicitly_cacheable()";
  return proto()->is_implicitly_cacheable();
}

// Return true if Content type field changed.
// If there's already a content type specified, leave it.
// If there's already a mime type or a charset specified,
// leave that and fill in the missing piece (if specified).
bool ResponseHeaders::CombineContentTypes(const StringPiece& orig,
                                          const StringPiece& fresh) {
  bool ret = false;
  GoogleString mime_type, charset;
  ret = ParseContentType(orig, &mime_type, &charset);
  if (!ret) {
    GoogleString fresh_mime_type, fresh_charset;
    ret = ParseContentType(fresh, &fresh_mime_type, &fresh_charset);
    // Don't replace nothing with a charset only because
    // ; charset=xyz is not a valid ContentType header..
    if (ret && !fresh_mime_type.empty()) {
      Replace(HttpAttributes::kContentType, fresh);
      ret = true;
    } else {
      ret = false;
    }
  } else if (charset.empty() || mime_type.empty()) {
    GoogleString fresh_mime_type, fresh_charset;
    ret = ParseContentType(fresh, &fresh_mime_type, &fresh_charset);
    if (ret) {
      if (charset.empty()) {
        charset = fresh_charset;
      }
      if (mime_type.empty()) {
        mime_type = fresh_mime_type;
      }
      GoogleString full_type = StringPrintf(
          "%s;%s%s",
          mime_type.c_str(),
          charset.empty()? "" : " charset=",
          charset.c_str());
      Replace(HttpAttributes::kContentType, full_type);
      ret = true;
    }
  }
  if (ret) {
    cache_fields_dirty_ = true;
  }
  return ret;
}

bool ResponseHeaders::MergeContentType(const StringPiece& content_type) {
  for (size_t i = 0; i < content_type.size(); i++) {
    if (!IsNonControlAscii(content_type[i])) {
      return false;
    }
  }

  bool ret = false;
  ConstStringStarVector old_values;
  Lookup(HttpAttributes::kContentType, &old_values);
  // If there aren't any content-type headers, we can just add this one.
  // If there is exactly one content-type header, then try to merge it
  // with what we were passed.
  // If there is already more than one content-type header, it's
  // unclear what exactly should happen, so don't change anything.
  if (old_values.size() < 1) {
    ret = CombineContentTypes("", content_type);
  } else if (old_values.size() == 1) {
    StringPiece old_val(*old_values[0]);
    ret = CombineContentTypes(old_val, content_type);
  }
  if (ret) {
    cache_fields_dirty_ = true;
  }
  return ret;
}

void ResponseHeaders::UpdateFrom(const Headers<HttpResponseHeaders>& other) {
  cache_fields_dirty_ = true;
  Headers<HttpResponseHeaders>::UpdateFrom(other);
}

void ResponseHeaders::UpdateFromProto(const HttpResponseHeaders& proto) {
  Clear();
  cache_fields_dirty_ = true;
  Headers<HttpResponseHeaders>::CopyProto(proto);
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
bool ResponseHeaders::IsBrowserCacheable() const {
  // We do not compute caching from accessors so that the
  // accessors can be easier to call from multiple threads
  // without mutexing.
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before IsBrowserCacheable()";
  return proto()->browser_cacheable();
}

bool ResponseHeaders::RequiresBrowserRevalidation() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before RequiresBrowserRevalidation()";
  return proto()->requires_browser_revalidation();
}

bool ResponseHeaders::RequiresProxyRevalidation() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before RequiresProxyRevalidation()";
  return proto()->requires_proxy_revalidation();
}

bool ResponseHeaders::IsProxyCacheable(
    RequestHeaders::Properties req_properties,
    VaryOption respect_vary,
    ValidatorOption has_request_validator) const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before IsProxyCacheable()";

  if (!proto()->proxy_cacheable()) {
    return false;
  }

  // For something requested with authorization to be cacheable, it must
  // either  be something that goes through revalidation (which we currently
  // do not do) or something that has a Cache-Control: public.
  // See RFC2616, 14.8
  // (http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.8)
  if (req_properties.has_authorization &&
      !HasValue(HttpAttributes::kCacheControl, "public")) {
    return false;
  }

  ConstStringStarVector values;
  Lookup(HttpAttributes::kVary, &values);
  bool is_html_like = IsHtmlLike();
  for (int i = 0, n = values.size(); i < n; ++i) {
    StringPiece val(*values[i]);
    if (!val.empty() &&
        !StringCaseEqual(HttpAttributes::kAcceptEncoding, val)) {
      if (StringCaseEqual(HttpAttributes::kCookie, val)) {
        // We check Vary:Cookie independent of whether RespectVary is specified.
        // For HTML, we are OK caching and re-serving content served with
        // Vary:Cookie, as long as there is no cookie in the header.  However
        // for resources we elect not to do this due to the possibility of us
        // not seeing the original cookie after domain-mapping.
        if (req_properties.has_cookie || !is_html_like ||
            (has_request_validator == kNoValidator)) {
          return false;
        }
      } else if (StringCaseEqual(HttpAttributes::kCookie2, val)) {
        if (req_properties.has_cookie2 || !is_html_like ||
            (has_request_validator == kNoValidator)) {
          return false;
        }
      } else if ((respect_vary == kRespectVaryOnResources) || is_html_like) {
        // We never cache HTML with other Vary headers, and we don't
        // do so for resources either if respect_vary is set.
        return false;
      }
    }
  }
  return true;
}

// Returns the ms-since-1970 absolute time when this resource
// should be expired out of caches.
int64 ResponseHeaders::CacheExpirationTimeMs() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before CacheExpirationTimeMs()";
  return proto()->expiration_time_ms();
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

void ResponseHeaders::SetCacheControlPublic() {
  ConstStringStarVector values;
  if (Lookup(HttpAttributes::kCacheControl, &values)) {
    for (int i = 0, n = values.size(); i < n; ++i) {
      StringPiece val = *(values[i]);
      if (StringCaseEqual(val, "private") ||
          StringCaseEqual(val, "public") ||
          StringCaseEqual(val, "no-cache") ||
          StringCaseEqual(val, "no-store")) {
        return;
      }
    }
  }

  // Note that adding 'public' to a non-private cache-control does
  // not change the value of any of the precomputed bools we've stored,
  // so make the 'dirty' bit unchanged across this operation.
  bool dirty = cache_fields_dirty_;
  GoogleString new_value = JoinStringStar(values, ", ");
  StrAppend(&new_value, new_value.empty() ? "public" : ", public");
  Replace(HttpAttributes::kCacheControl, new_value);
  cache_fields_dirty_ = dirty;
}

void ResponseHeaders::SetTimeHeader(const StringPiece& header, int64 time_ms) {
  GoogleString time_string;
  if (ConvertTimeToString(time_ms, &time_string)) {
    Replace(header, time_string);
  }
}

void ResponseHeaders::SetContentLength(int64 content_length) {
  // Setting the content-length to the same value as the
  // x-original-content-length should clear any x-original-content-length.
  // This happens when serving a cached gzipped value to a client that
  // does not accept gzip.  However, only remove the original-content-length
  // if it is the same as the new resulting content length, because the
  // content may have been minified to a smaller value, and we want to
  // retain evidence of the cost savings in that case.
  bool dirty = cache_fields_dirty_;
  GoogleString content_length_str = Integer64ToString(content_length);
  Remove(HttpAttributes::kXOriginalContentLength, content_length_str);
  Replace(HttpAttributes::kContentLength, content_length_str);
  cache_fields_dirty_ = dirty;
}

void ResponseHeaders::SetOriginalContentLength(int64 content_length) {
  // This does not impact caching headers, so avoid ComputeCaching()
  // by restoring cache_fields_dirty_ after we set the header.
  if (!Has(HttpAttributes::kXOriginalContentLength)) {
    bool dirty = cache_fields_dirty_;
    Add(HttpAttributes::kXOriginalContentLength,
        Integer64ToString(content_length));
    cache_fields_dirty_ = dirty;
  }
}

bool ResponseHeaders::Sanitize() {
  ConstStringStarVector v;
  bool changed = false;

  // Sanitize any fields marked as hop-by-hop via the Connection: header
  if (Lookup(HttpAttributes::kConnection, &v)) {
    for (int i = 0, n = v.size(); i < n; ++i) {
      StringPiece val = *v[i];
      if (!IsHopByHopIndication(val)) {
        continue;
      }

      if (StringCaseEqual(val, HttpAttributes::kConnection)) {
        // Don't want to remove it since we have pointers into it, and it's
        // already hop-by-hop.
        continue;
      }

      changed = RemoveAll(*v[i]) || changed;
    }
  }

  // Remove cookies plus any well-known hop-by-hop headers, which we will never
  // store in a cache.
  const StringPieceVector& names_to_sanitize =
      HttpAttributes::SortedHopByHopHeaders();
  changed = RemoveAllFromSortedArray(
      &names_to_sanitize[0], names_to_sanitize.size()) || changed;
  return changed;
}

void ResponseHeaders::GetSanitizedProto(HttpResponseHeaders* proto) const {
  Headers<HttpResponseHeaders>::CopyToProto(proto);
  protobuf::RepeatedPtrField<NameValue>* headers = proto->mutable_header();

  // Note that these need to be deep-copies as we are mutating the underlying
  // data.
  StringVector more_names;

  // Mark all headers marked as hop-by-hop in "Connection: " for sanitization.
  for (int i = 0, n = headers->size(); i < n; ++i) {
    if (StringCaseEqual(headers->Get(i).name(), HttpAttributes::kConnection)) {
      StringCompareInsensitive compare;
      StringPieceVector split;
      SplitStringPieceToVector(headers->Get(i).value(), ",", &split, true);
      if (split.empty()) {
        split.push_back(headers->Get(i).value());
      }

      // Check each value in Connection: val1, val2, ...
      for (int j = 0, m = split.size(); j < m; ++j) {
        StringPiece val = split[j];
        TrimWhitespace(&val);
        // Skip values that are connection-tokens, empty, or are defined
        // as being end-to-end.
        if (!IsHopByHopIndication(val)) {
          continue;
        }

        // Find the position at which we should insert to keep the list sorted.
        StringVector::iterator up = std::lower_bound(
            more_names.begin(), more_names.end(), val, compare);

        // Insert when the entry is not already contained.
        if (up == more_names.end() || !StringCaseEqual(*up, val)) {
          more_names.insert(up, val.as_string());
        }
      }
    }
  }

  const StringPieceVector& names_to_sanitize =
      HttpAttributes::SortedHopByHopHeaders();
  RemoveFromHeaders(&names_to_sanitize[0], names_to_sanitize.size(), headers);
  // The common case will be that more_names is empty.
  if (more_names.size() > 0) {
    RemoveFromHeaders(&more_names[0], more_names.size(), headers);
  }
}

namespace {

// Subclass of pagespeed's cache computer to deal with our slightly different
// policies.
//
// The differences are:
//  1) TODO(sligocki): We can consider HTML to be cacheable by default
//     depending upon a user option.
//  2) We only consider HTTP status code 200, 301 and our internal use codes
//     to be cacheable. Others (such as 203, 206 and 304) are not cacheable
//     for us.
//
// This also abstracts away the pagespeed::Resource/ResponseHeaders distinction.
class InstawebCacheComputer : public CachingHeaders {
 public:
  explicit InstawebCacheComputer(const ResponseHeaders& headers)
      : CachingHeaders(headers.status_code()),
        response_headers_(headers) {
  }

  virtual ~InstawebCacheComputer() {}

  // Which status codes are cacheable by default.
  virtual bool IsCacheableResourceStatusCode() const {
    switch (status_code()) {
      // For our purposes, only a few status codes are cacheable.
      // Others like 203, 206 and 304 depend upon input headers and other state.
      case HttpStatus::kOK:
      case HttpStatus::kMovedPermanently:
        return true;
      default:
        // We have some additional internal status codes we use to remember
        // failures, those are cacheable as their entire purpose is to record
        // that failures happened in the cache.
        // TODO(morlovich): This could be expressed as
        // HttpCacheFailure::IsFailureCachingStatus --- but it's not in the
        // right layer.
        return (status_code() >= HttpStatus::kRememberFailureRangeStart &&
                status_code() < HttpStatus::kRememberFailureRangeEnd);
    }
  }

  // Which status codes do we allow to cache at all. Others will not be cached
  // even if explicitly marked as such because we may not be able to cache
  // them correctly (say 304 or 206, which depend upon input headers).
  bool IsAllowedCacheableStatusCode() {
    // For now it's identical to the default cacheable list.
    return IsCacheableResourceStatusCode();

    // Note: We have made a conscious decision not to allow caching
    // 302 Found or 307 Temporary Redirect even if they explicitly
    // ask to be cached because most webmasters use 301 Moved Permanently
    // for redirects they actually want cached.
  }

  virtual bool IsLikelyStaticResourceType() const {
    if (IsRedirectStatusCode()) {
      return true;  // redirects are cacheable
    }
    const ContentType* type = response_headers_.DetermineContentType();
    return (type != NULL) && type->IsLikelyStaticResource();
  }

  virtual bool Lookup(const StringPiece& key, StringPieceVector* values) {
    ConstStringStarVector value_strings;
    bool ret = response_headers_.Lookup(key, &value_strings);
    if (ret) {
      values->resize(value_strings.size());
      for (int i = 0, n = value_strings.size(); i < n; ++i) {
        (*values)[i] = *value_strings[i];
      }
    } else {
      values->clear();
    }
    return ret && !values->empty();
  }

 private:
  const ResponseHeaders& response_headers_;
  DISALLOW_COPY_AND_ASSIGN(InstawebCacheComputer);
};

}  // namespace

void ResponseHeaders::ComputeCaching() {
  if (!cache_fields_dirty_) {
    return;
  }

  HttpResponseHeaders* proto = mutable_proto();

  ConstStringStarVector values;
  int64 date_ms, last_modified_ms;
  bool has_date = ParseDateHeader(HttpAttributes::kDate, &date_ms);
  // Compute the timestamp if we can find it
  if (has_date) {
    proto->set_date_ms(date_ms);
  }

  bool has_last_modified = ParseDateHeader(HttpAttributes::kLastModified,
                                           &last_modified_ms);
  // Compute the timestamp if we can find it
  if (has_last_modified) {
    proto->set_last_modified_time_ms(last_modified_ms);
  } else {
    proto->clear_last_modified_time_ms();
  }

  // Computes caching info.
  InstawebCacheComputer computer(*this);

  // Can we force cache this response?
  bool force_caching_enabled = false;

  const ContentType* type = DetermineContentType();
  if ((force_cache_ttl_ms_) > 0 &&
      (status_code() == HttpStatus::kOK)) {
    force_caching_enabled = (type == NULL) || !type->IsHtmlLike();
  }

  // Note: Unlike pagespeed algorithm, we are very conservative about calling
  // a resource cacheable. Many status codes are technically cacheable but only
  // based upon precise input headers. Since we do not check those headers we
  // only allow a few hand-picked status codes to be cacheable at all.
  // Note that if force caching is enabled, we consider a privately cacheable
  // resource as cacheable.
  bool is_browser_cacheable = computer.IsCacheable();
  proto->set_browser_cacheable(
      has_date &&
      computer.IsAllowedCacheableStatusCode() &&
      (force_caching_enabled || is_browser_cacheable));
  proto->set_requires_browser_revalidation(computer.MustRevalidate());
  proto->set_requires_proxy_revalidation(
      computer.ProxyRevalidate() || proto->requires_browser_revalidation());
  if (proto->browser_cacheable()) {
    // TODO(jmarantz): check "Age" resource and use that to reduce
    // the expiration_time_ms_.  This is, says, bmcquade@google.com,
    // typically use to indicate how long a resource has been sitting
    // in a proxy-cache. Or perhaps this should be part of the pagespeed
    // ResourceCacheComputer algorithms.
    // See: http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html
    //
    // Implicitly cached items stay alive in our system for the specified
    // implicit ttl ms.
    bool is_proxy_cacheable = computer.IsProxyCacheable();
    int64 cache_ttl_ms = http_options_.implicit_cache_ttl_ms;
    if (computer.IsExplicitlyCacheable()) {
      // TODO(sligocki): Do we care about the return value.
      computer.GetFreshnessLifetimeMillis(&cache_ttl_ms);
    }
    if (force_caching_enabled &&
        (force_cache_ttl_ms_ > cache_ttl_ms || !is_proxy_cacheable)) {
      // We consider the response to have been force cached only if force
      // caching was enabled and the forced cache TTL is larger than the
      // original TTL or the original response wasn't cacheable.
      cache_ttl_ms = force_cache_ttl_ms_;
      force_cached_ = true;
    }

    proto->set_cache_ttl_ms(cache_ttl_ms);
    proto->set_expiration_time_ms(proto->date_ms() + cache_ttl_ms);
    proto->set_proxy_cacheable(force_cached_ || is_proxy_cacheable);

    // Do not cache HTML or redirects with Set-Cookie / Set-Cookie2 header even
    // though they may have explicit caching directives. This is to prevent the
    // caching of user sensitive data due to misconfigured caching headers.
    if (((type != NULL && type->IsHtmlLike()) ||
         computer.IsRedirectStatusCode()) &&
        (Has(HttpAttributes::kSetCookie) || Has(HttpAttributes::kSetCookie2))) {
      proto->set_proxy_cacheable(false);
    }

    if (proto->proxy_cacheable() && !force_cached_ &&
        !computer.IsExplicitlyCacheable()) {
      // If the resource is proxy cacheable but it does not have explicit
      // caching headers and is not force cached, explicitly set the caching
      // headers.
      DCHECK(has_date);
      DCHECK(cache_ttl_ms == http_options_.implicit_cache_ttl_ms);
      proto->set_is_implicitly_cacheable(true);
      SetDateAndCaching(date_ms, cache_ttl_ms,
                        CacheControlValuesToPreserve());
    }
  } else {
    proto->set_expiration_time_ms(0);
    proto->set_proxy_cacheable(false);
  }
  cache_fields_dirty_ = false;
}

GoogleString ResponseHeaders::CacheControlValuesToPreserve() {
  GoogleString to_preserve;
  if (HasValue(HttpAttributes::kCacheControl, "no-transform")) {
    to_preserve = ", no-transform";
  }
  if (HasValue(HttpAttributes::kCacheControl, "no-store")) {
    to_preserve += ", no-store";
  }

  ConstStringStarVector cc_values;
  Lookup(HttpAttributes::kCacheControl, &cc_values);
  for (auto value : cc_values) {
    if (StringCaseStartsWith(*value, "s-maxage=")) {
      to_preserve += ", " + *value;
    }
  }

  return to_preserve;
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
  return ConvertStringToTime(time_str, time_ms);
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
void ResponseHeaders::DetermineContentTypeAndCharset(
    const ContentType** content_type_out, GoogleString* charset_out) const {
  ConstStringStarVector content_types;

  if (content_type_out != NULL) {
    *content_type_out = NULL;
  }

  if (charset_out != NULL) {
    charset_out->clear();
  }

  // If there is more than one content-type header, we pick the LAST one,
  // (even if it's invalid!) as that's the behavior specified by the mime
  // sniffing spec (http://mimesniff.spec.whatwg.org/). We also use the
  // charset that comes with the same header.
  if (Lookup(HttpAttributes::kContentType, &content_types) &&
      !content_types.empty()) {
    GoogleString mime_type, charset;
    if (!ParseContentType(*content_types.back(), &mime_type, &charset)) {
      mime_type.clear();
      charset.clear();
    }

    if (content_type_out != NULL) {
      *content_type_out = MimeTypeToContentType(mime_type);
    }

    if (charset_out != NULL) {
      *charset_out = charset;
    }
  }
}

GoogleString ResponseHeaders::DetermineCharset() const {
  GoogleString charset;
  DetermineContentTypeAndCharset(NULL, &charset);
  return charset;
}

const ContentType* ResponseHeaders::DetermineContentType() const {
  const ContentType* content_type = NULL;
  DetermineContentTypeAndCharset(&content_type, NULL);
  return content_type;
}

bool ResponseHeaders::ParseDateHeader(
    const StringPiece& attr, int64* date_ms) const {
  const char* date_string = Lookup1(attr);
  return (date_string != NULL) && ConvertStringToTime(date_string, date_ms);
}

void ResponseHeaders::ParseFirstLine(const StringPiece& first_line) {
  if (strings::StartsWith(first_line, "HTTP/")) {
    ParseFirstLineHelper(first_line.substr(5));
  } else {
    LOG(WARNING) << "Could not parse first line: " << first_line;
  }
}

void ResponseHeaders::ParseFirstLineHelper(const StringPiece& first_line) {
  int major_version, minor_version, status;
  // We reserve enough to avoid buffer overflow on sscanf command.
  GoogleString reason_phrase(first_line.size(), '\0');
  char* reason_phrase_cstr = &reason_phrase[0];
  int num_scanned = sscanf(
      first_line.as_string().c_str(), "%d.%d %d %[^\n\t]s",
      &major_version, &minor_version, &status,
      reason_phrase_cstr);
  if (num_scanned < 3) {
    LOG(WARNING) << "Could not parse first line: " << first_line;
  } else {
    if (num_scanned == 3) {
      reason_phrase = HttpStatus::GetReasonPhrase(
          static_cast<HttpStatus::Code>(status));
      reason_phrase_cstr = &reason_phrase[0];
    }
    set_first_line(major_version, minor_version, status, reason_phrase_cstr);
  }
}

void ResponseHeaders::SetCacheControlMaxAge(int64 ttl_ms) {
  // If the cache fields were not dirty before this call, recompute caching
  // before returning.
  bool recompute_caching = !cache_fields_dirty_;

  SetTimeHeader(HttpAttributes::kExpires, date_ms() + ttl_ms);

  ConstStringStarVector values;
  Lookup(HttpAttributes::kCacheControl, &values);

  GoogleString new_cache_control_value =
      StrCat("max-age=", Integer64ToString(ttl_ms / Timer::kSecondMs));

  for (int i = 0, n = values.size(); i < n; ++i) {
    if (values[i] != NULL) {
      StringPiece val(*values[i]);
      if (!val.empty() && !StringCaseStartsWith(val, "max-age")) {
        StrAppend(&new_cache_control_value, ",", val);
      }
    }
  }
  Replace(HttpAttributes::kCacheControl, new_cache_control_value);

  if (recompute_caching) {
    ComputeCaching();
  }
}

void ResponseHeaders::DebugPrint() const {
  fputs(ToString().c_str(), stderr);
  fputs("\ncache_fields_dirty_ = ", stderr);
  fputs(BoolToString(cache_fields_dirty_), stderr);
  fputs("\nis_implicitly_cacheable = ", stderr);
  fputs(BoolToString(proto()->is_implicitly_cacheable()), stderr);
  fputs("\nhttp_options_.implicit_cache_ttl_ms = ", stderr);
  fputs(Integer64ToString(http_options_.implicit_cache_ttl_ms).c_str(), stderr);
  if (!cache_fields_dirty_) {
    fputs("\nexpiration_time_ms_ = ", stderr);
    fputs(Integer64ToString(proto()->expiration_time_ms()).c_str(), stderr);
    fputs("\nlast_modified_time_ms_ = ", stderr);
    fputs(Integer64ToString(last_modified_time_ms()).c_str(), stderr);
    fputs("\ndate_ms_ = ", stderr);
    fputs(Integer64ToString(proto()->date_ms()).c_str(), stderr);
    fputs("\ncache_ttl_ms_ = ", stderr);
    fputs(Integer64ToString(proto()->cache_ttl_ms()).c_str(), stderr);
    fputs("\nbrowser_cacheable_ = ", stderr);
    fputs(BoolToString(proto()->browser_cacheable()), stderr);
    fputs("\nproxy_cacheable_ = ", stderr);
    fputs(BoolToString(proto()->proxy_cacheable()), stderr);
  }
  fputc('\n', stderr);
}

bool ResponseHeaders::FindContentLength(int64* content_length) const {
  const char* val = Lookup1(HttpAttributes::kContentLength);
  return (val != NULL) && StringToInt64(val, content_length);
}

void ResponseHeaders::ForceCaching(int64 ttl_ms) {
  // If the cache fields were not dirty before this call, recompute caching
  // before returning.
  bool recompute_caching = !cache_fields_dirty_;
  if (ttl_ms > 0) {
    force_cache_ttl_ms_ = ttl_ms;
    cache_fields_dirty_ = true;
    if (recompute_caching) {
      ComputeCaching();
    }
  }
}

bool ResponseHeaders::UpdateCacheHeadersIfForceCached() {
  if (cache_fields_dirty_) {
    LOG(DFATAL)  << "Call ComputeCaching() before "
                 << "UpdateCacheHeadersIfForceCached";
    return false;
  }
  if (force_cached_) {
    int64 date = date_ms();
    int64 ttl = cache_ttl_ms();
    RemoveAll(HttpAttributes::kPragma);
    RemoveAll(HttpAttributes::kCacheControl);
    SetDateAndCaching(date, ttl);
    ComputeCaching();
    return true;
  }
  return false;
}

int64 ResponseHeaders::SizeEstimate() const {
  int64 len = STATIC_STRLEN("HTTP/1.x 123 ") +  // All statuses are 3 digits.
              strlen(reason_phrase()) + STATIC_STRLEN("\r\n");
  for (int i = 0, n = NumAttributes(); i < n; ++i) {
    len += Name(i).length() + STATIC_STRLEN(": ") +
           Value(i).length() + STATIC_STRLEN("\r\n");
  }
  len += STATIC_STRLEN("\r\n");
  return len;
}

bool ResponseHeaders::GetCookieString(GoogleString* cookie_str) const {
  // NOTE: Although our superclass has a cookie map we could use, we don't
  // because we are interested in the raw header lines not the parsed results.
  cookie_str->clear();
  ConstStringStarVector cookies;
  if (!Lookup(HttpAttributes::kSetCookie, &cookies)) {
    return false;
  }

  StrAppend(cookie_str, "[");
  for (int i = 0, n = cookies.size(); i < n; ++i) {
    GoogleString escaped;
    EscapeToJsStringLiteral(*cookies[i], true, &escaped);
    StrAppend(cookie_str, escaped);
    if (i != (n-1)) {
      StrAppend(cookie_str, ",");
    }
  }
  StrAppend(cookie_str, "]");
  return true;
}

bool ResponseHeaders::HasCookie(StringPiece name,
                                StringPieceVector* values,
                                StringPieceVector* attributes) const {
  const CookieMultimap* cookies = PopulateCookieMap(HttpAttributes::kSetCookie);
  CookieMultimapConstIter from = cookies->lower_bound(name);
  CookieMultimapConstIter to = cookies->upper_bound(name);
  for (CookieMultimapConstIter iter = from; iter != to; ++iter) {
    if (values != NULL) {
      values->push_back(iter->second.first);
    }
    if (attributes != NULL) {
      StringPieceVector items;
      SplitStringPieceToVector(iter->second.second, ";", &items, true);
      attributes->insert(attributes->end(), items.begin(), items.end());
    }
  }
  return from != to;
}

bool ResponseHeaders::HasAnyCookiesWithAttribute(StringPiece attribute_name,
                                                 StringPiece* attribute_value) {
  ConstStringStarVector cookies;
  if (Lookup(HttpAttributes::kSetCookie, &cookies)) {
    // Iterate through the cookies.
    for (int i = 0, n = cookies.size(); i < n; ++i) {
      StringPieceVector name_value_pairs;
      SplitStringPieceToVector(*cookies[i], ";", &name_value_pairs, true);
      // Ignore the first name=value which sets the actual cookie.
      for (int i = 1, n = name_value_pairs.size(); i < n; ++i) {
        StringPiece name;
        ExtractNameAndValue(name_value_pairs[i], &name, attribute_value);
        if (StringCaseEqual(attribute_name, name)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool ResponseHeaders::SetQueryParamsAsCookies(
    const GoogleUrl& gurl, StringPiece query_params,
    const StringPieceVector& options_to_exclude, int64 expiration_time) {
  bool result = false;
  // Domain (aka host).
  StringPiece host = gurl.Host();
  // Expiration time.
  GoogleString expires;
  ConvertTimeToString(expiration_time, &expires);
  // Go through each query param and set a cookie for it.
  QueryParams params;
  params.ParseFromUntrustedString(query_params);
  for (int i = 0, n = params.size(); i < n; ++i) {
    StringPiece name = params.name(i);
    bool skipit = false;
    for (int j = 0, n = options_to_exclude.size(); j < n; ++j) {
      if (name == options_to_exclude[j]) {
        skipit = true;
        break;
      }
    }
    if (!skipit) {
      // See RewriteQuery::Scan() for the discussion about why we apparently
      // double-escape by GoogleUrl escaping the QueryParams escaped value.
      const GoogleString* value = params.EscapedValue(i);
      GoogleString escaped_value;
      if (value != NULL) {
        escaped_value = StrCat("=", GoogleUrl::EscapeQueryParam(*value));
      }
      GoogleString cookie = StrCat(
          name, escaped_value, "; Expires=", expires, "; Domain=", host,
          "; Path=/; HttpOnly");
      Add(HttpAttributes::kSetCookie, cookie);
      result = true;
    }
  }
  return result;
}

bool ResponseHeaders::ClearOptionCookies(
    const GoogleUrl& gurl, StringPiece option_cookies,
    const StringPieceVector& options_to_exclude) {
  bool result = false;
  // Domain (aka host).
  StringPiece host = gurl.Host();
  // Expiration time. Zero is "the start of the epoch" and is the conventional
  // way to immediately expire a cookie per:
  // http://en.wikipedia.org/wiki/HTTP_cookie#Expires_and_Max-Age
  GoogleString expires;
  ConvertTimeToString(0, &expires);
  // Go through each option cookie and clear each one.
  QueryParams params;
  params.ParseFromUntrustedString(option_cookies);
  for (int i = 0, n = params.size(); i < n; ++i) {
    StringPiece name = params.name(i);
    bool skipit = false;
    for (int j = 0, n = options_to_exclude.size(); j < n; ++j) {
      if (name == options_to_exclude[j]) {
        skipit = true;
        break;
      }
    }
    if (!skipit) {
      GoogleString cookie = StrCat(params.name(i), "; Expires=", expires,
                                   "; Domain=", host, "; Path=/; HttpOnly");
      Add(HttpAttributes::kSetCookie, cookie);
      result = true;
    }
  }
  return result;
}

void ResponseHeaders::UpdateHook() {
  cache_fields_dirty_ = true;
}

GoogleString ResponseHeaders::RelCanonicalHeaderValue(StringPiece url) {
  return StrCat("<", GoogleUrl::Sanitize(url), ">; rel=\"canonical\"");
}

bool ResponseHeaders::HasLinkRelCanonical() const {
  ConstStringStarVector links;
  Lookup(HttpAttributes::kLink, &links);
  for (int i = 0, n = links.size(); i < n; ++i) {
    StringPiece cand(*links[i]);
    stringpiece_ssize_type rel_pos = cand.find("rel");
    stringpiece_ssize_type can_pos = cand.rfind("canonical");
    if (rel_pos != StringPiece::npos &&
        can_pos != StringPiece::npos &&
        rel_pos < can_pos) {
      return true;
    }
  }
  return false;
}

void ResponseHeaders::SetSMaxAge(int s_maxage_sec) {
  GoogleString updated_cache_control;

  ConstStringStarVector values;
  GoogleString existing_cache_control = "";
  if (Lookup(HttpAttributes::kCacheControl, &values)) {
    // TODO(jefftk): since we've done the work to split it into a vector, it's
    // inefficient to be joining it back to a string to give to ApplySMaxAge
    // which will split to a vector.
    existing_cache_control = JoinStringStar(values, ", ");
  }

  if (ApplySMaxAge(s_maxage_sec,
                   existing_cache_control,
                   &updated_cache_control)) {
    Replace(HttpAttributes::kCacheControl, updated_cache_control);
  }
}

// static
bool ResponseHeaders::ApplySMaxAge(int s_maxage_sec,
                                   StringPiece existing_cache_control,
                                   GoogleString* updated_cache_control) {
  TrimWhitespace(&existing_cache_control);
  GoogleString s_maxage_str = StrCat("s-maxage=",
                                     IntegerToString(s_maxage_sec));
  if (existing_cache_control.empty()) {
    *updated_cache_control = s_maxage_str;
    return true;
  }

  StringPieceVector segments;
  SplitStringPieceToVector(existing_cache_control, ",", &segments,
                           true /* omit empty strings */);
  for (StringPiece& segment : segments) {
    TrimWhitespace(&segment);
    if (StringCaseEqual(segment, "no-transform")) {
      // We're not allowed to touch this, so don't.
      return false;
    }
    if (StringCaseEqual(segment, "no-cache") ||
        StringCaseEqual(segment, "no-store") ||
        StringCaseEqual(segment, "private")) {
      // Downstream shared caches shouldn't be caching these, and adding
      // s-maxage might confuse one into thinking that it should actually go
      // ahead and cache, so if any of these are present, don't add it.
      return false;
    }
  }

  // It's not clear from the RFC what we should do if there are multiple
  // s-maxages with different values.  The most conservative thing is probably
  // to update them individually, so let's do that.
  bool found_existing_s_maxage = false;
  bool updated_existing_s_maxage = false;
  for (StringPiece& segment : segments) {
    if (StringCaseStartsWith(segment, "s-maxage=")) {
      // Found existing s-maxage.  If it's larger than s_maxage_sec update it.
      found_existing_s_maxage = true;

      StringPiece existing_value_s = segment;
      existing_value_s.remove_prefix(STATIC_STRLEN("s-maxage="));
      int existing_value;
      if (!StringToInt(existing_value_s, &existing_value)) {
        // Failed to parse existing s-maxage value; leave it alone.
      } else if (existing_value <= s_maxage_sec) {
        // It's already small enough, don't change this one.  But there might be
        // additional ones that need to be lowered, so keep going.
      } else {
        segment = s_maxage_str;
        updated_existing_s_maxage = true;
      }
    }
  }
  if (found_existing_s_maxage) {
    if (updated_existing_s_maxage) {
      *updated_cache_control = JoinCollection(segments, ", ");
    }
    return updated_existing_s_maxage;
  }

  // Didn't find s-maxage; look for max-age.

  // It's not clear how to handle multiple max-ages either.  Since s-maxage
  // overrides max-age if present, and we don't want to accidentally make
  // something more cacheable, we only add s-maxage if it's lower than the
  // lowest existing max-age header.
  bool found_existing_maxage = false;
  int lowest_existing_maxage_value = s_maxage_sec+1;
  for (StringPiece& segment : segments) {
    if (StringCaseStartsWith(segment, "max-age=")) {
      found_existing_maxage = true;
      StringPiece existing_value_s = segment;
      existing_value_s.remove_prefix(STATIC_STRLEN("max-age="));
      int existing_value;
      if (!StringToInt(existing_value_s, &existing_value)) {
        // Failed to parse existing max-age value; ignore it.
      } else if (existing_value < lowest_existing_maxage_value) {
        lowest_existing_maxage_value = existing_value;
      }
    }
  }
  if (found_existing_maxage && lowest_existing_maxage_value <= s_maxage_sec) {
    return false;
  }
  *updated_cache_control = StrCat(existing_cache_control, ", ", s_maxage_str);
  return true;
}

bool ResponseHeaders::IsHopByHopIndication(StringPiece val) {
  StringCompareInsensitive compare;
  const StringPieceVector& end_to_end = HttpAttributes::SortedEndToEndHeaders();

  if (val.empty() || StringCaseEqual(val, "keep-alive") ||
      StringCaseEqual(val, "close") || StringCaseStartsWith(val, "timeout=") ||
      StringCaseStartsWith(val, "max=") ||
      std::binary_search(end_to_end.begin(), end_to_end.end(), val, compare)) {
    return false;
  }
  return true;
}

}  // namespace net_instaweb
