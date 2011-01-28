/*
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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CACHE_EXTENDER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CACHE_EXTENDER_H_

#include <vector>

#include "base/basictypes.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/util/public/atom.h"
#include <string>

namespace net_instaweb {

class Hasher;
class ResourceManager;
class Timer;
class Variable;

// Rewrites resources without changing their content -- just their
// URLs and headers.  The original intent of this filter was limited
// to cache extension.  However, its scope has been expanded to include
// domain sharding and moving static resources to cookieless domains or
// CDNs.
//
// TODO(jmarantz): rename this class to something more generic, like
// RenameUrlFilter or ProxyUrlFilter.
class CacheExtender : public RewriteSingleResourceFilter {
 public:
  CacheExtender(RewriteDriver* driver, const char* path_prefix);

  static void Initialize(Statistics* statistics);

  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element) {}

  virtual const char* Name() const { return "CacheExtender"; }

 protected:
  virtual bool RewriteLoadedResource(const Resource* input_resource,
                                     OutputResource* output_resource);

 private:
  bool IsRewrittenResource(const StringPiece& url) const;
  bool ShouldRewriteResource(
      const ResponseHeaders* headers, int64 now_ms,
      const Resource* input_resource, const StringPiece& url) const;

  ResourceTagScanner tag_scanner_;
  Variable* extension_count_;
  Variable* not_cacheable_count_;

  DISALLOW_COPY_AND_ASSIGN(CacheExtender);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CACHE_EXTENDER_H_
