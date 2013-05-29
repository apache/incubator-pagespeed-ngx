/*
 * Copyright 2013 Google Inc.
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

// Author: jkarlin@google.com (Josh Karlin)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_FAKE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_FAKE_FILTER_H_


#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class HtmlElement;
class ResourceContext;
class RewriteContext;
class RewriteDriver;
struct ContentType;

// A test filter that that appends ':id' to the input contents and counts the
// number of rewrites it has performed. It also has the ability to simulate a
// long rewrite to test exceeding the rewrite deadline.
class FakeFilter : public RewriteFilter {
 public:
  class Context : public SingleRewriteContext {
   public:
    Context(FakeFilter* filter, RewriteDriver* driver, RewriteContext* parent,
            ResourceContext* resource_context)
        : SingleRewriteContext(driver, parent, resource_context),
          filter_(filter) {}
    virtual ~Context();

    void RewriteSingle(const ResourcePtr& input,
                       const OutputResourcePtr& output);

    void DoRewriteSingle(const ResourcePtr input, OutputResourcePtr output);
    GoogleString UserAgentCacheKey(
        const ResourceContext* resource_context) const;

    virtual const char* id() const { return filter_->id(); }
    virtual OutputResourceKind kind() const { return filter_->kind(); }

   private:
    FakeFilter* filter_;
  };

  FakeFilter(const char* id, RewriteDriver* rewrite_driver)
      : RewriteFilter(rewrite_driver),
        id_(id),
        exceed_deadline_(false),
        enabled_(true),
        num_rewrites_(0),
        output_content_type_(NULL),
        num_calls_to_encode_user_agent_(0) {}

  virtual ~FakeFilter();

  virtual void StartDocumentImpl() {}
  virtual void EndElementImpl(HtmlElement* element) {}
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual RewriteContext* MakeRewriteContext() {
    return new FakeFilter::Context(this, driver_, NULL, NULL);
  }
  virtual RewriteContext* MakeNestedRewriteContext(RewriteContext* parent,
                                                   const ResourceSlotPtr& slot);
  int num_rewrites() const { return num_rewrites_; }
  int num_encode_user_agent() const { return num_calls_to_encode_user_agent_; }
  void ClearStats();
  void set_enabled(bool x) { enabled_ = x; }
  bool enabled() { return enabled_; }
  bool exceed_deadline() { return exceed_deadline_; }
  void set_exceed_deadline(bool x) { exceed_deadline_ = x; }
  void IncRewrites() { ++num_rewrites_; }
  void set_output_content_type(const ContentType* type) {
    output_content_type_ = type;
  }
  const ContentType* output_content_type() { return output_content_type_; }
  virtual void EncodeUserAgentIntoResourceContext(
      ResourceContext* context) const;

 protected:
  virtual const char* id() const { return id_; }
  virtual OutputResourceKind kind() const { return kRewrittenResource; }
  virtual const char* Name() const { return "MockFilter"; }
  virtual bool ComputeOnTheFly() const { return false; }

 private:
  const char* id_;
  bool exceed_deadline_;
  bool enabled_;
  int num_rewrites_;
  const ContentType* output_content_type_;
  mutable int num_calls_to_encode_user_agent_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FAKE_FILTER_H_
