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

#ifndef PAGESPEED_CORE_RESOURCE_CACHE_COMPUTER_H_
#define PAGESPEED_CORE_RESOURCE_CACHE_COMPUTER_H_

#include "base/basictypes.h"
#include "base/logging.h"

namespace pagespeed {

class Resource;

// Class to embody computing caching info for Resources.
// This class has two advantages over static functions in resource_util:
//  1) It allows computation to be run once, lazily and saved rather than
//     multiple times and thrown away.
//  2) It supplies virtual methods for details of caching policy so that users
//     (including Page Speed Automatic) can tweak parts of the policy by
//     subclassing and overriding these methods.
class ResourceCacheComputer {
 public:
  // resource must outlive ResourceCacheComputer.
  // Does not take ownership of resource.
  explicit ResourceCacheComputer(const Resource* resource)
      : resource_(resource) {}
  // Note: Do not remove virtual for this destructor, as code in other
  // projects does subclass this class.
  virtual ~ResourceCacheComputer();

  // Is the resource cachable, either by explicit caching headers or
  // using common caching heuristics? If you want to know if the
  // resource is explicitly marked as cacheable, use
  // GetFreshnessLifetimeMillis() and test to see that the output
  // parameter it positive.
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

  // Tweakable methods
  //
  // These methods are virtual so that derived calsses can change the policy
  // decisions regarding what constitutes a static resource type and/or what
  // constitutes a cacheable resource status code.

  // Is the given resource type usually associated wiht static resources?
  virtual bool IsLikelyStaticResourceType();

  // Is the given status code known to be associated with
  // static/cacheable resources?
  virtual bool IsCacheableResourceStatusCode();

 private:
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
    ~Optional();

    bool has_value() const { return has_value_; }

    T value() const {
      // Not meaningful unless has_value() == true.
      DCHECK(has_value());
      return value_;
    }

    void set_value(T value) {
      // Do not set a value twice.
      DCHECK(!has_value());
      value_ = value;
      has_value_ = true;
    }

   private:
    T value_;
    bool has_value_;

    DISALLOW_COPY_AND_ASSIGN(Optional);
  };

  const Resource* resource_;

  Optional<int64> freshness_lifetime_millis_;
  Optional<bool> is_cacheable_;
  Optional<bool> is_proxy_cacheable_;
  Optional<bool> is_explicitly_cacheable_;
  Optional<bool> is_heuristically_cacheable_;
  Optional<bool> has_explicit_no_cache_directive_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCacheComputer);
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_RESOURCE_CACHE_COMPUTER_H_
