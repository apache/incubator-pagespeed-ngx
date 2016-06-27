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

#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/http/google_url.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
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
  remove_ = (driver_->can_modify_urls() &&
             (!(options->css_preserve_urls() &&
                options->image_preserve_urls() &&
                options->js_preserve_urls())));
  delete_element_ = NULL;
}

void StripSubresourceHintsFilter::StartElement(HtmlElement* element) {
  if (!remove_ || delete_element_) return;

  if (element->keyword() == HtmlName::kLink) {
    const char *value = element->AttributeValue(HtmlName::kRel);
    if (value && (
            StringCaseEqual(value, "subresource") ||
            StringCaseEqual(value, "preload"))) {
      const RewriteOptions *options = driver_->options();
      const char *resource_url = element->AttributeValue(HtmlName::kSrc);
      if (!resource_url) {
        // There's either no src attr, or one that we can't decode (utf8 etc).
        // One way this could happen is if we have a url-encoded utf8 url in an
        // img tag and a utf8 encoded url in the subresource tag.  Delete the
        // subresource link to be on the safe side.
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
