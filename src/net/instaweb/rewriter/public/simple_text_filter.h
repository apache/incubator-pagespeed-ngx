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

#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"

namespace net_instaweb {

class RewriteDriver;

// Generic hyper-simple rewriter class, which retains zero state
// across different rewrites; just transforming text to other text,
// returning whether anything changed.  This text may come from
// resource files or inline in HTML, though the latter is NYI.
//
// Implementors of this mechanism do not have to worry about
// resource-loading, cache reading/writing, expiration times, etc.
// Subclass SimpleTextFilter::Rewriter to define how to rewrite text.
class SimpleTextFilter : public CommonFilter {
 public:
  class Rewriter : public RefCounted<Rewriter> {
   public:
    Rewriter() {}
    virtual bool RewriteText(const StringPiece& url,
                             const StringPiece& in,
                             GoogleString* out,
                             ResourceManager* resource_manager) = 0;
    virtual const char* id() const = 0;
    virtual const char* Name() const = 0;
    virtual HtmlElement::Attribute* FindResourceAttribute(
        HtmlElement* element) = 0;

   protected:
    REFCOUNT_FRIEND_DECLARATION(Rewriter);

    virtual ~Rewriter();

   private:
    DISALLOW_COPY_AND_ASSIGN(Rewriter);
  };

  class Context : public SingleRewriteContext {
   public:
    Context(const ResourceSlotPtr& slot,
            const RefCountedPtr<Rewriter>& rewriter,
            RewriteDriver* driver)
        : SingleRewriteContext(driver, slot, NULL),
          rewriter_(rewriter) {
    }
    virtual ~Context();
    virtual RewriteSingleResourceFilter::RewriteResult Rewrite(
        const Resource* input_resource, OutputResource* output_resource);

   protected:
    virtual const char* id() const { return rewriter_->id(); }

   private:
    RefCountedPtr<Rewriter> rewriter_;

    DISALLOW_COPY_AND_ASSIGN(Context);
  };

  SimpleTextFilter(Rewriter* rewriter, RewriteDriver* driver);
  virtual ~SimpleTextFilter();

  virtual void StartDocumentImpl() {}
  virtual void EndElementImpl(HtmlElement* element) {}
  virtual void StartElementImpl(HtmlElement* element);

 protected:
  virtual GoogleString id() const { return rewriter_->id(); }
  virtual const char* Name() const { return rewriter_->Name(); }

 private:
  RefCountedPtr<Rewriter> rewriter_;

  DISALLOW_COPY_AND_ASSIGN(SimpleTextFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SIMPLE_TEXT_FILTER_H_
