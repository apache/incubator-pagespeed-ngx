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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_SIMPLE_TEXT_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_SIMPLE_TEXT_FILTER_H_

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"  // for ResourcePtr
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class RewriteContext;
class RewriteDriver;

// Generic hyper-simple rewriter class, which retains zero state
// across different rewrites; just transforming text to other text,
// returning whether anything changed.  This text may come from
// resource files or inline in HTML, though the latter is NYI.
//
// Implementors of this mechanism do not have to worry about
// resource-loading, cache reading/writing, expiration times, etc.
// Subclass SimpleTextFilter::Rewriter to define how to rewrite text.
class SimpleTextFilter : public RewriteFilter {
 public:
  class Rewriter : public RefCounted<Rewriter> {
   public:
    Rewriter() {}
    virtual bool RewriteText(const StringPiece& url,
                             const StringPiece& in,
                             GoogleString* out,
                             ServerContext* resource_manager) = 0;
    virtual HtmlElement::Attribute* FindResourceAttribute(
        HtmlElement* element) = 0;

    virtual OutputResourceKind kind() const = 0;
    virtual const char* id() const = 0;
    virtual const char* name() const = 0;

    // See RewriteContext::OptimizationOnly()
    virtual bool OptimizationOnly() const { return true; }

   protected:
    REFCOUNT_FRIEND_DECLARATION(Rewriter);
    virtual ~Rewriter();

   private:
    DISALLOW_COPY_AND_ASSIGN(Rewriter);
  };

  typedef RefCountedPtr<Rewriter> RewriterPtr;

  class Context : public SingleRewriteContext {
   public:
    Context(const RewriterPtr& rewriter, RewriteDriver* driver,
            RewriteContext* parent);
    virtual ~Context();
    virtual void RewriteSingle(
        const ResourcePtr& input, const OutputResourcePtr& output);

   protected:
    virtual const char* id() const { return rewriter_->id(); }
    virtual OutputResourceKind kind() const { return rewriter_->kind(); }
    virtual bool OptimizationOnly() const {
      return rewriter_->OptimizationOnly();
    }

   private:
    RewriterPtr rewriter_;

    DISALLOW_COPY_AND_ASSIGN(Context);
  };

  SimpleTextFilter(Rewriter* rewriter, RewriteDriver* driver);
  virtual ~SimpleTextFilter();

  virtual void StartDocumentImpl() {}
  virtual void EndElementImpl(HtmlElement* element) {}
  virtual void StartElementImpl(HtmlElement* element);

  virtual RewriteContext* MakeRewriteContext();
  virtual RewriteContext* MakeNestedRewriteContext(
      RewriteContext* parent, const ResourceSlotPtr& slot);

 protected:
  virtual const char* id() const { return rewriter_->id(); }
  virtual const char* Name() const { return rewriter_->name(); }
  virtual bool ComputeOnTheFly() const {
    return rewriter_->kind() == kOnTheFlyResource;
  }

 private:
  RewriterPtr rewriter_;

  DISALLOW_COPY_AND_ASSIGN(SimpleTextFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SIMPLE_TEXT_FILTER_H_
