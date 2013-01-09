// Copyright 2012 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_CORE_RESOURCE_COLLECTION_H_
#define PAGESPEED_CORE_RESOURCE_COLLECTION_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

namespace pagespeed {

class Resource;
class ResourceCollection;
class ResourceFilter;

// sorts resources by their URLs.
struct ResourceUrlLessThan {
  bool operator() (const Resource* lhs, const Resource* rhs) const;
};

typedef std::set<const Resource*, ResourceUrlLessThan> ResourceSet;
typedef std::map<std::string, ResourceSet> HostResourceMap;
typedef std::vector<const Resource*> ResourceVector;

/**
 * Companion class to ResourceCollection that provides convenience
 * methods to look up resources that are part of redirect chains.
 */
class RedirectRegistry {
 public:
  typedef std::vector<const Resource*> RedirectChain;
  typedef std::vector<RedirectChain> RedirectChainVector;
  typedef std::map<const Resource*, const RedirectChain*>
      ResourceToRedirectChainMap;

  RedirectRegistry();
  void Init(const ResourceCollection& resource_collection);

  const RedirectChainVector& GetRedirectChains() const;
  const RedirectChain* GetRedirectChainOrNull(const Resource* resource) const;
  // Given a pointer to a resource, return a pointer to the final resource in
  // the redirect chain.  If the resource has no redirect chain, return the
  // resource itself.  If the given pointer is NULL, return NULL.
  const Resource* GetFinalRedirectTarget(const Resource* resource) const;

 private:
  void BuildRedirectChains(const ResourceCollection& resource_collection);

  RedirectChainVector redirect_chains_;
  ResourceToRedirectChainMap resource_to_redirect_chain_map_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(RedirectRegistry);
};

/**
 * Collection of resources with convenient accessor methods.
 */
class ResourceCollection {
 public:
  ResourceCollection();
  // ResourceCollection takes ownership of the passed resource_filter.
  explicit ResourceCollection(ResourceFilter* resource_filter);
  ~ResourceCollection();

  // Setters

  // Adds a resource to the list.
  // Returns true if resource was added to the list.
  //
  // Ownership of the resource is transfered over to the
  // ResourceCollection object.
  bool AddResource(Resource* resource);

  bool Freeze();

  // Resource access.
  int num_resources() const;
  bool has_resource_with_url(const std::string& url) const;
  const Resource& GetResource(int idx) const;
  const Resource* GetResourceWithUrlOrNull(const std::string& url) const;

  // Get a non-const pointer to a resource. It is an error to call
  // these methods after this object has been frozen.
  Resource* GetMutableResource(int idx);
  Resource* GetMutableResourceWithUrlOrNull(const std::string& url);

  // Get the map from hostname to all resources on that hostname.
  const HostResourceMap* GetHostResourceMap() const;

  // Get the set of all resources, sorted in request order. Will be
  // NULL if one or more resources does not have a request start
  // time.
  const ResourceVector* GetResourcesInRequestOrder() const;

  const RedirectRegistry* GetRedirectRegistry() const;

  bool SetPrimaryResourceUrl(const std::string& url);
  const std::string& primary_resource_url() const;
  const Resource* GetPrimaryResourceOrNull() const;
  bool is_frozen() const;

 private:
  bool IsValidResource(const Resource* resource) const;

  std::vector<Resource*> resources_;
  std::string primary_resource_url_;

  // Map from URL to Resource. The resources_ vector, above, owns the
  // Resource instances in this map.
  std::map<std::string, const Resource*> url_resource_map_;

  // Map from hostname to Resources on that hostname. The resources_
  // vector, above, owns the Resource instances in this map.
  HostResourceMap host_resource_map_;

  ResourceVector request_order_vector_;

  scoped_ptr<ResourceFilter> resource_filter_;
  RedirectRegistry redirect_registry_;
  bool frozen_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCollection);
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_RESOURCE_COLLECTION_H_
