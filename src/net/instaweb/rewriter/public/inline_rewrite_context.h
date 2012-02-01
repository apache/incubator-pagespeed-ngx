/*
 * Copyright 2011 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_INLINE_REWRITE_CONTEXT_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_INLINE_REWRITE_CONTEXT_H_

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CachedResult;
class CommonFilter;
class OutputPartitions;

// Class that unifies tasks common to building rewriters for filters
// that inline resources.
class InlineRewriteContext : public RewriteContext {
 public:
  // Note that you should also call StartInlining() to do the work.
  InlineRewriteContext(CommonFilter* filter, HtmlElement* element,
                       HtmlElement::Attribute* src);
  virtual ~InlineRewriteContext();

  // Starts the actual inlining process, and takes over memory management
  // of this object.
  // Returns true if the process is started, false if it cannot be started
  // because the input resource cannot be created, in which case 'this' is
  // deleted and accordingly no rewriting callbacks are invoked.
  bool StartInlining();

 protected:
  // Subclasses of InlineRewriteContext must override these:
  virtual bool ShouldInline(const StringPiece& input) const = 0;
  virtual void RenderInline(const ResourcePtr& resource,
                            const StringPiece& text,
                            HtmlElement* element) = 0;


  // InlineRewriteContext takes care of these methods from RewriteContext;
  virtual bool Partition(OutputPartitions* partitions,
                         OutputResourceVector* outputs);
  virtual void Rewrite(int partition_index,
                       CachedResult* partition,
                       const OutputResourcePtr& output);
  virtual void Render();
  virtual OutputResourceKind kind() const;

 private:
  CommonFilter* filter_;
  HtmlElement* element_;
  HtmlElement::Attribute* src_;

  DISALLOW_COPY_AND_ASSIGN(InlineRewriteContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_INLINE_REWRITE_CONTEXT_H_
