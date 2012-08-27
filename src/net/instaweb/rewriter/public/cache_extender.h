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

#include "net/instaweb/rewriter/public/resource.h"  // for ResourcePtr
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class HtmlElement;
class ResponseHeaders;
class RewriteContext;
class RewriteDriver;
class Statistics;
class Variable;

// Rewrites resources without changing their content -- just their
// URLs and headers.  The original intent of this filter was limited
// to cache extension.  However, its scope has been expanded to include
// domain sharding and moving static resources to cookieless domains or
// CDNs.
//
// TODO(jmarantz): rename this class to something more generic, like
// RenameUrlFilter or ProxyUrlFilter.
class CacheExtender : public RewriteFilter {
 public:
  static const char kCacheExtensions[];
  static const char kNotCacheable[];

  explicit CacheExtender(RewriteDriver* driver);
  virtual ~CacheExtender();

  static void Initialize(Statistics* statistics);

  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element) {}

  virtual const char* Name() const { return "CacheExtender"; }
  virtual const char* id() const { return RewriteOptions::kCacheExtenderId; }

  // Creates a nested rewrite for given parent and slot, and returns it.
  // The result is not registered with the parent.
  RewriteContext* MakeNestedContext(RewriteContext* parent,
                                    const ResourceSlotPtr& slot);

 protected:
  virtual bool ComputeOnTheFly() const;
  virtual RewriteContext* MakeRewriteContext();

 private:
  class Context;
  friend class Context;

  RewriteResult RewriteLoadedResource(const ResourcePtr& input_resource,
                                      const OutputResourcePtr& output_resource);

  bool ShouldRewriteResource(
      const ResponseHeaders* headers, int64 now_ms,
      const ResourcePtr& input_resource, const StringPiece& url) const;

  Variable* extension_count_;
  Variable* not_cacheable_count_;

  DISALLOW_COPY_AND_ASSIGN(CacheExtender);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CACHE_EXTENDER_H_
