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

#include <cstdio>     // for fprintf, stderr, snprintf

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
#include "pagespeed/core/resource.h"
#include "pagespeed/core/resource_cache_computer.h"
#include "pagespeed/core/resource_util.h"

namespace net_instaweb {

// Specifies the maximum amount of forward drift we'll allow for a Date
// timestamp.  E.g. if it's 3:00:00 and the Date header says its 3:01:00,
// we'll leave the date-header in the future.  But if it's 3:03:01 then
// we'll set it back to 3:00:00 exactly in FixDateHeaders.
const int64 kMaxAllowedDateDriftMs = 3L * net_instaweb::Timer::kMinuteMs;

class MessageHandler;

const int64 ResponseHeaders::kImplicitCacheTtlMs;

ResponseHeaders::ResponseHeaders() {
  proto_.reset(new HttpResponseHeaders);
  Clear();
}

ResponseHeaders::~ResponseHeaders() {
  Clear();
}

namespace {

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

void ResponseHeaders::FixDateHeaders(int64 now_ms) {
  int64 date_ms = 0;
  bool has_date = true;

  if (cache_fields_dirty_) {
    // We don't want to call ComputeCaching() right here because it's expensive,
    // and if we decide we need to alter the Date header then we'll have to
    // recompute Caching later anyway.
    has_date = ParseDateHeader(HttpAttributes::kDate, &date_ms);
  } else if (proto_->has_date_ms()) {
    date_ms = proto_->date_ms();
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
        if (proto_->has_expiration_time_ms()) {
          SetTimeHeader(HttpAttributes::kExpires, proto_->expiration_time_ms());
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
  map_.reset(NULL);
  *(proto_.get()) = *(other.proto_.get());
  cache_fields_dirty_ = other.cache_fields_dirty_;
}

void ResponseHeaders::Clear() {
  Headers<HttpResponseHeaders>::Clear();

  proto_->set_cacheable(false);
  proto_->set_proxy_cacheable(false);   // accurate only if !cache_fields_dirty_
  proto_->clear_expiration_time_ms();
  proto_->clear_date_ms();
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

int64 ResponseHeaders::date_ms() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before date_ms()";
  return proto_->date_ms();
}

int64 ResponseHeaders::cache_ttl_ms() const {
  DCHECK(!cache_fields_dirty_)
      << "Call ComputeCaching() before cache_ttl_ms()";
  return proto_->cache_ttl_ms();
}

bool ResponseHeaders::has_date_ms() const {
  return proto_->has_date_ms();
}

void ResponseHeaders::Add(const StringPiece& name, const StringPiece& value) {
  Headers<HttpResponseHeaders>::Add(name, value);
  cache_fields_dirty_ = true;
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
class InstawebCacheComputer : public pagespeed::ResourceCacheComputer {
 public:
  static InstawebCacheComputer* NewComputer(const ResponseHeaders& headers) {
    pagespeed::Resource* resource = new pagespeed::Resource;
    for (int i = 0, n = headers.NumAttributes(); i < n; ++i) {
      resource->AddResponseHeader(headers.Name(i), headers.Value(i));
    }
    resource->SetResponseStatusCode(headers.status_code());
    return new InstawebCacheComputer(resource);
  }

  virtual ~InstawebCacheComputer() {}

  virtual bool IsLikelyStaticResourceType() {
    // TODO(sligocki): Change how we treat HTML based on an option.
    return pagespeed::ResourceCacheComputer::IsLikelyStaticResourceType();
  }

  // Which status codes are cacheable by default.
  virtual bool IsCacheableResourceStatusCode() {
    switch (resource_->GetResponseStatusCode()) {
      // For our purposes, only a few status codes are cacheable.
      // Others like 203, 206 and 304 depend upon input headers and other state.
      case HttpStatus::kOK:
      case HttpStatus::kMovedPermanently:
      // These dummy status codes indicate something about our system that we
      // want to remember in the cache.
      case HttpStatus::kRememberNotCacheableStatusCode:
      case HttpStatus::kRememberFetchFailedStatusCode:
        return true;
      default:
        return false;
    }
  }

  // Which status codes do we allow to cache at all. Others will not be cached
  // even if explicitly marked as such because we may not be able to cache
  // them correctly (say 304 or 206, which depend upon input headers).
  bool IsAllowedCacheableStatusCode() {
    // For now it's identical to the default cacheable list.
    return IsCacheableResourceStatusCode();

    // Note: We have made a consious decision not to allow caching
    // 302 Found or 307 Temporary Redirect even if they explicitly
    // ask to be cached because most webmasters use 301 Moved Permanently
    // for redirects they actually want cached.
  }

  pagespeed::ResourceType GetResourceType() const {
    return resource_->GetResourceType();
  }

 private:
  // Takes ownership of resource (used by NewComputer).
  explicit InstawebCacheComputer(pagespeed::Resource* resource)
      : ResourceCacheComputer(resource), resource_(resource) {}

  scoped_ptr<pagespeed::Resource> resource_;

  DISALLOW_COPY_AND_ASSIGN(InstawebCacheComputer);
};

}  // namespace

void ResponseHeaders::ComputeCaching() {
  if (!cache_fields_dirty_) {
    return;
  }

  ConstStringStarVector values;
  int64 date;
  bool has_date = ParseDateHeader(HttpAttributes::kDate, &date);
  // Compute the timestamp if we can find it
  if (has_date) {
    proto_->set_date_ms(date);
  }

  // Computes caching info.
  scoped_ptr<InstawebCacheComputer> computer(
      InstawebCacheComputer::NewComputer(*this));

  // Note: Unlike pagespeed algorithm, we are very conservative about calling
  // a resource cacheable. Many status codes are technically cacheable but only
  // based upon precise input headers. Since we do not check those headers we
  // only allow a few hand-picked status codes to be cacheable at all.
  proto_->set_cacheable(has_date &&
                        computer->IsAllowedCacheableStatusCode() &&
                        computer->IsCacheable());
  if (proto_->cacheable()) {
    // TODO(jmarantz): check "Age" resource and use that to reduce
    // the expiration_time_ms_.  This is, says, bmcquade@google.com,
    // typically use to indicate how long a resource has been sitting
    // in a proxy-cache. Or perhaps this should be part of the pagespeed
    // ResourceCacheComputer algorithms.
    // See: http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html
    //
    // Implicitly cached items stay alive in our system for 5 minutes
    // TODO(jmarantz): Consider making this a flag, or getting some
    // other heuristic value from the PageSpeed libraries.
    int64 cache_ttl_ms = kImplicitCacheTtlMs;
    if (computer->IsExplicitlyCacheable()) {
      // TODO: Do we care about the return value.
      computer->GetFreshnessLifetimeMillis(&cache_ttl_ms);
    }
    proto_->set_cache_ttl_ms(cache_ttl_ms);
    proto_->set_expiration_time_ms(proto_->date_ms() + cache_ttl_ms);

    proto_->set_proxy_cacheable(computer->IsProxyCacheable());

    // Do not cache HTML with Set-Cookie / Set-Cookie2 headers even though it
    // has explicit caching directives. This is to prevent the caching of user
    // sensitive data due to misconfigured caching headers.
    if (computer->GetResourceType() == pagespeed::HTML &&
        (Lookup1(HttpAttributes::kSetCookie) != NULL ||
         Lookup1(HttpAttributes::kSetCookie2) != NULL)) {
      proto_->set_proxy_cacheable(false);
    }

    if (proto_->proxy_cacheable() && !computer->IsExplicitlyCacheable()) {
      // If the resource is proxy cacheable but it does not have explicit
      // caching headers, explicitly set the caching headers.
      DCHECK(has_date);
      DCHECK(cache_ttl_ms == kImplicitCacheTtlMs);
      SetDateAndCaching(date, cache_ttl_ms);
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

GoogleString ResponseHeaders::DetermineCharset() const {
  GoogleString charset;

  // Per the logic in DetermineContentType above we take the first charset
  // specified and ignore Content-Type headers without a charset.
  ConstStringStarVector content_types;
  if (Lookup(HttpAttributes::kContentType, &content_types)) {
    for (int i = 0, n = content_types.size(); i < n && charset.empty(); ++i) {
      GoogleString mime_type;
      ParseContentType(*(content_types[i]), &mime_type, &charset);
    }
  }

  return charset;
}

bool ResponseHeaders::ParseDateHeader(
    const StringPiece& attr, int64* date_ms) const {
  const char* date_string = Lookup1(attr);
  return (date_string != NULL) && ConvertStringToTime(date_string, date_ms);
}

void ResponseHeaders::ParseFirstLine(const StringPiece& first_line) {
  if (first_line.starts_with("HTTP/")) {
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
    fprintf(stderr, "date_ms_ = %s\n",
            Integer64ToString(proto_->date_ms()).c_str());
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
