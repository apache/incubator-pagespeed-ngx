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

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/simple_text_filter.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/htmlparse/public/html_name.h"

namespace net_instaweb {

SimpleTextFilter::Rewriter::~Rewriter() {
}

SimpleTextFilter::SimpleTextFilter(Rewriter* rewriter, RewriteDriver* driver)
    : CommonFilter(driver),
      rewriter_(rewriter) {
}

SimpleTextFilter::~SimpleTextFilter() {
}

SimpleTextFilter::Context::~Context() {
}

RewriteSingleResourceFilter::RewriteResult SimpleTextFilter::Context::Rewrite(
    const Resource* input_resource, OutputResource* output_resource) {
  RewriteSingleResourceFilter::RewriteResult result =
      RewriteSingleResourceFilter::kRewriteFailed;
  GoogleString rewritten;
  ResourceManager* resource_manager = this->resource_manager();
  if (rewriter_->RewriteText(input_resource->url(),
                             input_resource->contents(), &rewritten,
                             resource_manager))  {
    MessageHandler* message_handler = resource_manager->message_handler();
    int64 origin_expire_time_ms = input_resource->CacheExpirationTimeMs();
    if (resource_manager->Write(HttpStatus::kOK, rewritten, output_resource,
                                origin_expire_time_ms, message_handler)) {
      result = RewriteSingleResourceFilter::kRewriteOk;
    }
  }
  return result;
}

void SimpleTextFilter::StartElementImpl(HtmlElement* element) {
  HtmlElement::Attribute* attr = rewriter_->FindResourceAttribute(element);
  if (attr != NULL) {
    ResourcePtr resource = CreateInputResource(attr->value());
    if (resource.get() != NULL) {
      ResourceSlotPtr slot(driver_->GetSlot(resource, element, attr));
      driver_->InitiateRewrite(new Context(slot, rewriter_, driver_));
    }
  }
}

}  // namespace net_instaweb
