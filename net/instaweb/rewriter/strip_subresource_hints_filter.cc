/*
 * Copyright 2015 Google Inc.
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

// Author: kspoelstra@we-amp.com (Kees Spoelstra)

#include "net/instaweb/rewriter/public/strip_subresource_hints_filter.h"

#include "base/logging.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_parse.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"

namespace net_instaweb {

StripSubresourceHintsFilter::StripSubresourceHintsFilter(RewriteDriver* driver)
    : driver_(driver),
      delete_element_(NULL),
      remove_(false) {
}

StripSubresourceHintsFilter::~StripSubresourceHintsFilter() { }

void StripSubresourceHintsFilter::StartDocument() {
  const RewriteOptions *options = driver_->options();
  if (driver_->can_modify_urls()) {
    remove_ =
      (!(options->css_preserve_urls() &&
        options->image_preserve_urls() &&
        options->js_preserve_urls()));
  } else {
    remove_ = false;
  }
  delete_element_ = NULL;
}

void StripSubresourceHintsFilter::StartElement(HtmlElement* element) {
  if (!remove_) return;
  if (!delete_element_) {
    if (element->keyword() == HtmlName::kLink) {
      const char *value = element->AttributeValue(HtmlName::kRel);
      if (value && strcasecmp(value, "subresource") == 0) {
        const RewriteOptions *options = driver_->options();
        const char *resource_url = element->AttributeValue(HtmlName::kSrc);
        if (!resource_url) {  // can't check validity, delete
          delete_element_ = element;
          return;
        }
        const GoogleUrl &base_url = driver_->decoded_base_url();
        scoped_ptr<GoogleUrl> resolved_resource_url(
            new GoogleUrl(base_url, resource_url));
        if (options->IsAllowed(resolved_resource_url->Spec()) &&
            options->domain_lawyer()->IsDomainAuthorized(
              base_url, *resolved_resource_url)) {
          delete_element_ = element;
        }
      }
    }
  }
}

void StripSubresourceHintsFilter::EndElement(HtmlElement* element) {
  if (delete_element_ == element) {
    driver_->DeleteNode(delete_element_);
    delete_element_ = NULL;
  }
}

void StripSubresourceHintsFilter::Flush() {
  delete_element_ = NULL;
}

void StripSubresourceHintsFilter::EndDocument() {
}

}  // namespace net_instaweb
