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
//         sligocki@google.com (Shawn Ligocki)
//         jmarantz@google.com (Joshua Marantz)

#ifndef PAGESPEED_KERNEL_HTTP_CACHING_HEADERS_H_
#define PAGESPEED_KERNEL_HTTP_CACHING_HEADERS_H_

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {
// TODO(jmarantz): This class is temporarily in net_instaweb so that
// it can conveniently reference GoogleString etc.  We will move
// all the files in pagespeed/... to be in namespace pagespeed in a
// future change.

// Class to embody computing caching info for Resources.
// This class has two advantages over static functions in resource_util:
//  1) It allows computation to be run once, lazily and saved rather than
//     multiple times and thrown away.
//  2) It supplies virtual methods for details of caching policy so that users
//     (including Page Speed Automatic) can tweak parts of the policy by
//     subclassing and overriding these methods.
class CachingHeaders {
 public:
  // All StringPieces must outlive the CachingHeaders class.
  explicit CachingHeaders(int status_code);
  virtual ~CachingHeaders();

  // Implementors supply this method to provide HTTP response header values.
  virtual bool Lookup(const StringPiece& key, StringPieceVector* values) = 0;

  // To obtain correct heuristics on URLs with query-parameters, supply the URL.
  void set_url(StringPiece x) { url_ = x; }

  // Is the resource privately cacheable, either by explicit caching
  // headers or using common caching heuristics? If you want to know
  // if the resource is explicitly marked as cacheable, use
  // GetFreshnessLifetimeMillis() and test to see that the output
  // parameter it positive.
  // TODO(sligocki): Rename to IsBrowserCacheable().
  bool IsCacheable();

  // Is the resource likely to be cached by proxies?
  bool IsProxyCacheable();

  // Is this resource explicitly marked cacheable?
  bool IsExplicitlyCacheable();

  // Get the freshness lifetime of hte given resource, using the
  // algorithm described in the HTTP/1.1 RFC. Returns true if the
  // resource has an explicit freshness lifetime, false otherwise.
  // The out parameter is only valid when this function returns true.
  bool GetFreshnessLifetimeMillis(int64* out_freshness_lifetime_millis);

  // Does the resource have an explicit freshness lifetime? This is just
  // a wrapper around GetFreshnessLifetimeMillis().
  bool HasExplicitFreshnessLifetime();

  // Does the resource have an explicit HTTP header directive that
  // indicates it's not cacheable? For instance, Cache-Control: no-cache or
  // Pragma: no-cache.
  bool HasExplicitNoCacheDirective();

  // Determines whether the caching headers have a must-revalidate directive.
  bool MustRevalidate();

  // Determines whether the caching headers have a proxy-revalidate directive.
  bool ProxyRevalidate();

  // Tweakable methods
  //
  // These methods are virtual so that derived calsses can change the policy
  // decisions regarding what constitutes a static resource type and/or what
  // constitutes a cacheable resource status code.

  // Is the given resource type usually associated wiht static resources?
  virtual bool IsLikelyStaticResourceType() const = 0;

  // Is the given status code known to be associated with
  // static/cacheable resources?
  virtual bool IsCacheableResourceStatusCode() const = 0;

  bool IsRedirectStatusCode() const;

  int status_code() const { return status_code_; }

  // Generates a cache-control string for disabling caching, that is strictly
  // more conservative than the existing cache-control string.
  GoogleString GenerateDisabledCacheControl();

 private:
  void ParseCacheControlIfNecessary();

  // Uses heuristics to test cacheability. Can only be called if no explicit
  // cache headers have been set for resource!
  bool IsHeuristicallyCacheable();

  // Compute values w/o memory. Helper functions to the ones above which lazily
  // compute the result and memoize the result.
  bool ComputeIsCacheable();
  bool ComputeIsProxyCacheable();
  bool ComputeIsHeuristicallyCacheable();
  bool ComputeFreshnessLifetimeMillis(int64* out_freshness_lifetime_millis);
  bool ComputeHasExplicitNoCacheDirective();

  // A variable with added bool for whether or not it's been set.
  template<class T> class Optional {
   public:
    Optional() : has_value_(false) {}
    ~Optional() {}

    bool has_value() const { return has_value_; }

    T value() const {
      // Not meaningful unless has_value() == true.
      DCHECK(has_value());
      return value_;
    }

    void set_value(T value) {
      value_ = value;
      has_value_ = true;
    }

   private:
    T value_;
    bool has_value_;

    DISALLOW_COPY_AND_ASSIGN(Optional);
  };

  int status_code_;
  StringPiece url_;
  bool parsed_cache_control_;

  // Cache-control settings, read directly from the HTTP header.  The bools
  // all default to false & can be set true.  The max_age is left in !was_set
  // state until successfully parsed.
  bool public_;
  bool private_;
  bool no_transform_;
  bool must_revalidate_;
  bool proxy_revalidate_;
  bool no_cache_;
  bool no_store_;
  bool cache_control_parse_error_;
  bool expires_invalid_;
  Optional<int> max_age_seconds_;
  Optional<int64> expires_ms_;

  // Computed caching properties, taking into account response-code, type,
  // vary-headers, pragma, etc.
  Optional<int64> freshness_lifetime_millis_;
  Optional<bool> is_cacheable_;
  Optional<bool> is_proxy_cacheable_;
  Optional<bool> is_explicitly_cacheable_;
  Optional<bool> is_heuristically_cacheable_;
  Optional<bool> has_explicit_no_cache_directive_;

  DISALLOW_COPY_AND_ASSIGN(CachingHeaders);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTTP_CACHING_HEADERS_H_
