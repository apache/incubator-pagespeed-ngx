// Copyright 2010 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_CORE_RESOURCE_FILTER_H_
#define PAGESPEED_CORE_RESOURCE_FILTER_H_

#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"

namespace pagespeed {

class Resource;

/**
 * Abstract base class for objects providing an IsAccepted predicate.
 */
class ResourceFilter {
 public:
  ResourceFilter();
  virtual ~ResourceFilter();

  // Returns true iff the resource should be kept as part of the resource set.
  virtual bool IsAccepted(const Resource& resource) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceFilter);
};

/**
 * A ResourceFilter that filters nothing
 */
class AllowAllResourceFilter : public ResourceFilter {
 public:
  AllowAllResourceFilter() {};
  virtual ~AllowAllResourceFilter() {};

  virtual bool IsAccepted(const Resource& resource) const { return true; }
};

/**
 * A ResourceFilter that returns the opposite filtering decisions of another.
 */
class NotResourceFilter : public ResourceFilter {
 public:
  // NotResourceFilter takes ownership of the passed filter.
  NotResourceFilter(ResourceFilter *base_filter)
      : base_filter_(base_filter) {};
  virtual ~NotResourceFilter() {};

  virtual bool IsAccepted(const Resource& resource) const {
    return !base_filter_->IsAccepted(resource);
  }

 private:
  scoped_ptr<ResourceFilter> base_filter_;
};

/**
 * A ResourceFilter that ANDs the result of two or more filters.
 */
class AndResourceFilter : public ResourceFilter {
 public:
  // AndResourceFilter takes ownership of the passed filters.
  AndResourceFilter(ResourceFilter *filter1, ResourceFilter *filter2)
      : filter1_(filter1), filter2_(filter2) {};
  virtual ~AndResourceFilter() {};

  virtual bool IsAccepted(const Resource& resource) const;

 private:
  scoped_ptr<ResourceFilter> filter1_;
  scoped_ptr<ResourceFilter> filter2_;
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_RESOURCE_H_
