// Copyright 2011 Google Inc. All Rights Reserved.
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
//
// Author: bmcquade@google.com (Bryan McQuade)
// Author: sligocki@google.com (Shawn Ligocki)

#include "pagespeed/kernel/http/caching_headers.h"

#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/time_util.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

CachingHeaders::CachingHeaders(int status_code)
    : status_code_(status_code),
      parsed_cache_control_(false),
      public_(false),
      private_(false),
      no_transform_(false),
      must_revalidate_(false),
      no_cache_(false),
      no_store_(false),
      cache_control_parse_error_(false),
      expires_invalid_(false) {
}

CachingHeaders::~CachingHeaders() {
}

// Lazy getters

bool CachingHeaders::IsCacheable() {
  if (!is_cacheable_.has_value()) {
    is_cacheable_.set_value(ComputeIsCacheable());
  }
  return is_cacheable_.value();
}

bool CachingHeaders::IsProxyCacheable() {
  if (!is_proxy_cacheable_.has_value()) {
    is_proxy_cacheable_.set_value(ComputeIsProxyCacheable());
  }
  return is_proxy_cacheable_.value();
}

bool CachingHeaders::IsHeuristicallyCacheable() {
  if (!is_heuristically_cacheable_.has_value()) {
    is_heuristically_cacheable_.set_value(ComputeIsHeuristicallyCacheable());
  }
  return is_heuristically_cacheable_.value();
}

bool CachingHeaders::GetFreshnessLifetimeMillis(
    int64* out_freshness_lifetime_millis) {
  ParseCacheControlIfNecessary();
  DCHECK_EQ(is_explicitly_cacheable_.has_value(),
            freshness_lifetime_millis_.has_value());
  if (!is_explicitly_cacheable_.has_value() ||
      !freshness_lifetime_millis_.has_value()) {
    int64 freshness_lifetime_millis = 0;
    is_explicitly_cacheable_.set_value(
        ComputeFreshnessLifetimeMillis(&freshness_lifetime_millis));
    freshness_lifetime_millis_.set_value(freshness_lifetime_millis);
  }
  *out_freshness_lifetime_millis = freshness_lifetime_millis_.value();
  return is_explicitly_cacheable_.value();
}

bool CachingHeaders::HasExplicitNoCacheDirective() {
  if (!has_explicit_no_cache_directive_.has_value()) {
    has_explicit_no_cache_directive_.set_value(
        ComputeHasExplicitNoCacheDirective());
  }
  return has_explicit_no_cache_directive_.value();
}

// Simple wrapper functions

bool CachingHeaders::IsExplicitlyCacheable() {
  int64 freshness_lifetime = 0;
  return (GetFreshnessLifetimeMillis(&freshness_lifetime) &&
          (freshness_lifetime > 0));
}

bool CachingHeaders::HasExplicitFreshnessLifetime() {
  int64 freshness_lifetime = 0;
  return GetFreshnessLifetimeMillis(&freshness_lifetime);
}

bool CachingHeaders::IsRedirectStatusCode() const {
  return status_code_ == 301 ||
      status_code_ == 302 ||
      status_code_ == 303 ||
      status_code_ == 307;
}

// Actual compute logic

bool CachingHeaders::ComputeIsCacheable() {
  int64 freshness_lifetime = 0;
  if (GetFreshnessLifetimeMillis(&freshness_lifetime)) {
    if (freshness_lifetime <= 0) {
      // The resource is explicitly not fresh, so we don't consider it
      // to be a static resource.
      return false;
    }

    // If there's an explicit freshness lifetime and it's greater than
    // zero, then the resource is cacheable.
    return true;
  }

  // If we've made it this far, we've got a resource that doesn't have
  // explicit caching headers. At this point we use heuristics
  // specified in the HTTP RFC and implemented in many
  // browsers/proxies to determine if this resource is typically
  // cached.
  return IsHeuristicallyCacheable();
}

bool CachingHeaders::ComputeIsProxyCacheable() {
  return IsCacheable() && !private_;
}

bool CachingHeaders::ComputeIsHeuristicallyCacheable() {
  if (HasExplicitFreshnessLifetime()) {
    // If the response has an explicit freshness lifetime then it's
    // not heuristically cacheable. This method only expects to be
    // called if the resource does *not* have an explicit freshness
    // lifetime.
    LOG(DFATAL) << "IsHeuristicallyCacheable received a resource with "
                << "explicit freshness lifetime.";
    return false;
  }

  if (must_revalidate_) {
    // must-revalidate indicates that a non-fresh response should not
    // be used in response to requests without validating at the
    // origin. Such a resource is not heuristically cacheable.
    return false;
  }

  if (url_.find_first_of('?') != StringPiece::npos) {
    // The HTTP RFC says:
    //
    // ...since some applications have traditionally used GETs and
    // HEADs with query URLs (those containing a "?" in the rel_path
    // part) to perform operations with significant side effects,
    // caches MUST NOT treat responses to such URIs as fresh unless
    // the server provides an explicit expiration time.
    //
    // So if we find a '?' in the URL, the resource is not
    // heuristically cacheable.
    //
    // In practice most browsers do not implement this policy. For
    // instance, Chrome and IE8 do not look for the query string,
    // while Firefox (as of version 3.6) does. For the time being we
    // implement the RFC but it might make sense to revisit this
    // decision in the future, given that major browser
    // implementations do not match.
    return false;
  }

  if (!IsCacheableResourceStatusCode()) {
    return false;
  }

  if (!IsLikelyStaticResourceType()) {
    return false;
  }

  return true;
}

bool CachingHeaders::ComputeFreshnessLifetimeMillis(
    int64* out_freshness_lifetime_ms) {
  ParseCacheControlIfNecessary();

  // Initialize the output param to the default value. We do this in
  // case clients use the out value without checking the return value
  // of the function.
  *out_freshness_lifetime_ms = 0;

  if (HasExplicitNoCacheDirective()) {
    // If there's an explicit no cache directive then the resource is
    // never fresh.
    return true;
  }

  // First, look for Cache-Control: max-age. The HTTP/1.1 RFC
  // indicates that CC: max-age takes precedence to Expires.
  if (max_age_seconds_.has_value()) {
    *out_freshness_lifetime_ms = 1000LL * max_age_seconds_.value();
    return true;
  }

  // Next look for Expires.
  if (!expires_ms_.has_value()) {
    // If there's no expires header and we previously determined there
    // was no Cache-Control: max-age, then the resource doesn't have
    // an explicit freshness lifetime.
    return false;
  }

  // We've determined that there is an Expires header. Thus, the
  // resource has a freshness lifetime. Even if the Expires header
  // doesn't contain a valid date, it should be considered stale. From
  // HTTP/1.1 RFC 14.21: "HTTP/1.1 clients and caches MUST treat other
  // invalid date formats, especially including the value "0", as in
  // the past (i.e., "already expired")."

  int64 date_value_ms = 0;
  StringPieceVector date;
  if (!Lookup(HttpAttributes::kDate, &date) ||
      (date.size() != 1) ||
      !ConvertStringToTime(date[0], &date_value_ms)) {
    // We have an Expires header, but no Date header to reference
    // from. Thus we assume that the resource is heuristically
    // cacheable, but not explicitly cacheable.
    return false;
  }

  if (expires_invalid_) {
    // If we can't parse the Expires header, then treat the resource as
    // stale.
    return true;
  }

  int64 freshness_lifetime_ms = expires_ms_.value() - date_value_ms;
  if (freshness_lifetime_ms < 0) {
    freshness_lifetime_ms = 0;
  }
  *out_freshness_lifetime_ms = freshness_lifetime_ms;
  return true;
}

void CachingHeaders::ParseCacheControlIfNecessary() {
  if (!parsed_cache_control_) {
    parsed_cache_control_ = true;
    StringPieceVector values;
    if (Lookup(HttpAttributes::kCacheControl, &values)) {
      for (int i = 0, n = values.size(); i < n; ++i) {
        StringPiece value = values[i];
        if (value == "public") {
          public_ = true;
        } else if (value.starts_with("private")) {
          // See http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html :
          // "private" [ "=" <"> 1#field-name <"> ] ; Section 14.9.1
          // So we must use 'starts_width' rather than test for equality.
          private_ = true;
        } else if (value.starts_with("no-cache")) {
          // "no-cache" [ "=" <"> 1#field-name <"> ]; Section 14.9.1
          no_cache_ = true;
        } else if (value == "no-store") {
          no_store_ = true;
        } else if (value.starts_with("max-age=")) {
          int max_age_value = 0;
          StringPiece max_age_piece = value.substr(STATIC_STRLEN("max-age="));
          if (StringToInt(max_age_piece, &max_age_value)) {
            max_age_seconds_.set_value(max_age_value);
          } else {
            cache_control_parse_error_ = true;
          }
        }
      }
    }

    StringPieceVector expires;
    int64 expires_value_ms;
    if (Lookup(HttpAttributes::kExpires, &expires)) {
      if (!expires.empty() &&
          ConvertStringToTime(expires[0], &expires_value_ms)) {
        expires_ms_.set_value(expires_value_ms);
      } else {
        expires_invalid_ = true;
      }
    }
  }
}

bool CachingHeaders::ComputeHasExplicitNoCacheDirective() {
  ParseCacheControlIfNecessary();

  if (no_cache_ || no_store_ ||
      (max_age_seconds_.has_value() && max_age_seconds_.value() <= 0)) {
    return true;
  }

  if (expires_invalid_) {
    // An invalid Expires header (e.g. Expires: 0) means do not cache.
    return true;
  }

  StringPieceVector pragma, vary;
  if (Lookup(HttpAttributes::kPragma, &pragma) && STLFind(pragma, "no-cache")) {
    return true;
  }

  if (Lookup(HttpAttributes::kVary, &vary) &&
      STLFind(vary, "*")) {
    return true;
  }

  return false;
}

GoogleString CachingHeaders::GenerateDisabledCacheControl() {
  GoogleString new_cache_control(HttpAttributes::kNoCacheMaxAge0);
  StringPieceVector pieces, name_value;
  if (Lookup(HttpAttributes::kCacheControl, &pieces)) {
    for (int i = 0, n = pieces.size(); i < n; ++i) {
      name_value.clear();
      SplitStringPieceToVector(pieces[i], "=", &name_value, true);
      if (!name_value.empty()) {
        StringPiece name = name_value[0];
        TrimWhitespace(&name);
        // See http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.9.1
        if (!StringCaseEqual(name, HttpAttributes::kNoCache) &&
            !StringCaseEqual(name, HttpAttributes::kMaxAge) &&
            !StringCaseEqual(name, HttpAttributes::kPrivate) &&
            !StringCaseEqual(name, HttpAttributes::kPublic)) {
          StrAppend(&new_cache_control, ", ", pieces[i]);
        }
      }
    }
  }
  return new_cache_control;
}

}  // namespace net_instaweb
