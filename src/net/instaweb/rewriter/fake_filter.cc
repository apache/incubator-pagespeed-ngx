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

#include "net/instaweb/rewriter/public/fake_filter.h"
#include <memory>

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/scheduler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "pagespeed/kernel/html/html_element.h"

namespace net_instaweb {

FakeFilter::Context::~Context() {}

void FakeFilter::Context::RewriteSingle(const ResourcePtr& input,
                                        const OutputResourcePtr& output) {
  if (filter_->exceed_deadline()) {
    // Wake up 1us past the deadline.
    int64 wakeup_us = Driver()->scheduler()->timer()->NowUs() +
                      (Timer::kMsUs * GetRewriteDeadlineAlarmMs() + 1);
    Function* closure =
        MakeFunction(this, &Context::DoRewriteSingle, input, output);
    Driver()->scheduler()->AddAlarmAtUs(wakeup_us, closure);
  } else {
    DoRewriteSingle(input, output);
  }
}

void FakeFilter::Context::DoRewriteSingle(const ResourcePtr input,
                                          OutputResourcePtr output) {
  RewriteResult result = kRewriteFailed;
  GoogleString rewritten;

  if (filter_->enabled()) {
    // TODO(jkarlin): Writing to the filter from a context is not thread
    // safe.
    filter_->IncRewrites();
    StrAppend(&rewritten, input->contents(), ":", filter_->id());

    // Set the output type here to make sure that the CachedResult url
    // field has the correct extension for the type.
    const ContentType* output_type = &kContentTypeText;
    if (filter_->output_content_type() != NULL) {
      output_type = filter_->output_content_type();
    } else if (input->type() != NULL) {
      output_type = input->type();
    }
    ResourceVector rv = ResourceVector(1, input);
    if (Driver()->Write(rv, rewritten, output_type, input->charset(),
                        output.get())) {
      result = kRewriteOk;
    }
  }

  RewriteDone(result, 0);
}

GoogleString FakeFilter::Context::UserAgentCacheKey(
    const ResourceContext* resource_context) const {
  if (resource_context != NULL) {
    return ImageUrlEncoder::CacheKeyFromResourceContext(*resource_context);
  }
  return "";
}

FakeFilter::~FakeFilter() {}

void FakeFilter::StartElementImpl(HtmlElement* element) {
  resource_tag_scanner::UrlCategoryVector attributes;
  resource_tag_scanner::ScanElement(element, rewrite_options(), &attributes);
  for (int i = 0, n = attributes.size(); i < n; ++i) {
    if (attributes[i].category == category_) {
      ResourcePtr input_resource(CreateInputResourceOrInsertDebugComment(
          attributes[i].url->DecodedValueOrNull(), element));
      if (input_resource.get() == NULL) {
        return;
      }
      ResourceSlotPtr slot(
          driver()->GetSlot(input_resource, element, attributes[i].url));
      RewriteContext* context = MakeRewriteContext();
      context->AddSlot(slot);
      driver()->InitiateRewrite(context);
    }
  }
}

RewriteContext* FakeFilter::MakeNestedRewriteContext(
    RewriteContext* parent, const ResourceSlotPtr& slot) {
  ResourceContext* resource_context = new ResourceContext;
  if (parent != NULL && parent->resource_context() != NULL) {
    resource_context->CopyFrom(*parent->resource_context());
  }
  RewriteContext* context =
      MakeFakeContext(NULL, parent, resource_context);
  context->AddSlot(slot);
  return context;
}

void FakeFilter::ClearStats() {
  num_rewrites_ = 0;
  num_calls_to_encode_user_agent_ = 0;
}

void FakeFilter::EncodeUserAgentIntoResourceContext(
    ResourceContext* context) const {
  ImageUrlEncoder::SetWebpAndMobileUserAgent(*driver(), context);
  ++num_calls_to_encode_user_agent_;
}

}  // namespace net_instaweb
