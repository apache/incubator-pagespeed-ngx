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

#include "net/instaweb/rewriter/public/resource_slot.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"

namespace net_instaweb {

ResourceSlot::~ResourceSlot() {
}

void ResourceSlot::SetResource(const ResourcePtr& resource) {
  resource_ = ResourcePtr(resource);
}

bool ResourceSlot::DirectSetUrl(const StringPiece& url) {
  LOG(DFATAL) << "Trying to direct-set a URL on a slot that does not "
      "support it: " << LocationString();
  return false;
}

void ResourceSlot::ReportInput(const InputInfo& input) {
  if (inputs_ == nullptr) {
    inputs_.reset(new std::vector<InputInfo>);
  }
  inputs_->push_back(input);
}

RewriteContext* ResourceSlot::LastContext() const {
  if (contexts_.empty()) {
    return NULL;
  }
  return contexts_.back();
}

void ResourceSlot::DetachContext(RewriteContext* context) {
  if (contexts_.front() == context) {
    contexts_.pop_front();
  } else if (contexts_.back() == context) {
    contexts_.pop_back();
  } else {
    LOG(DFATAL) << "Can only detach first or last context";
  }
}

GoogleString ResourceSlot::RelativizeOrPassthrough(
    const RewriteOptions* options, StringPiece url,
    UrlRelativity url_relativity, const GoogleUrl& base_url) {
  if (options->preserve_url_relativity()) {
    // Set possibly relative URL.
    // TODO(sligocki): Get GoogleUrl in interface?
    GoogleUrl output_url(url);
    if (output_url.IsAnyValid()) {
      return output_url.Relativize(url_relativity, base_url).as_string();
    } else {
      LOG(DFATAL) << "Invalid URL passed to RelativizeOrPassthrough: " << url;
      return url.as_string();
    }
  } else {
    // Pass through absolute URL.
    return url.as_string();
  }
}

NullResourceSlot::NullResourceSlot(const ResourcePtr& resource,
                                   StringPiece location)
    : ResourceSlot(resource),
      location_(location.data(), location.size()) {
}

NullResourceSlot::~NullResourceSlot() {
}

FetchResourceSlot::~FetchResourceSlot() {
}

void FetchResourceSlot::Render() {
  LOG(DFATAL) << "FetchResourceSlot::Render should never be called";
}

GoogleString FetchResourceSlot::LocationString() const {
  return StrCat("Fetch of ", resource()->url());
}

HtmlResourceSlot::HtmlResourceSlot(const ResourcePtr& resource,
                                   HtmlElement* element,
                                   HtmlElement::Attribute* attribute,
                                   RewriteDriver* driver)
    : ResourceSlot(resource),
      element_(element),
      attribute_(attribute),
      driver_(driver),
      // TODO(sligocki): This is always the URL used to create resource, right?
      // Maybe we could construct the input resource here just to guarantee
      // that and simplify the code?
      url_relativity_(
          GoogleUrl::FindRelativity(attribute->DecodedValueOrNull())),
      // Note: these need to be deep-copied in case we run as a detached
      // rewrite, in which case element_ may be dead.
      begin_line_number_(element->begin_line_number()),
      end_line_number_(element->end_line_number()) {
}

HtmlResourceSlot::~HtmlResourceSlot() {
}

void HtmlResourceSlot::Render() {
  if (disable_rendering()) {
    return;  // nothing done here.
  } else if (should_delete_element()) {
    if (element_ != NULL) {
      driver_->DeleteNode(element_);
      element_ = NULL;
    }
  } else if (!preserve_urls()) {
    DirectSetUrl(RelativizeOrPassthrough(driver_->options(), resource()->url(),
                                         url_relativity_, driver_->base_url()));
    // Note that to insert image dimensions, we explicitly save
    // a reference to the element in the enclosing Context object.
  }
}

GoogleString HtmlResourceSlot::LocationString() const {
  if (begin_line_number_ == end_line_number_) {
    return StrCat(driver_->id(), ":", IntegerToString(begin_line_number_));
  } else {
    return StrCat(driver_->id(), ":",
                  IntegerToString(begin_line_number_),
                  "-", IntegerToString(end_line_number_));
  }
}

bool HtmlResourceSlot::DirectSetUrl(const StringPiece& url) {
  // We should never try to render unauthorized resource URLs as is.
  if (!resource()->is_authorized_domain()) {
    return false;
  }
  DCHECK(attribute_ != NULL);
  if (attribute_ != NULL) {
    attribute_->SetValue(url);
    return true;
  }
  return false;
}

// TODO(jmarantz): test sanity of set maintenance using this comparator.
bool HtmlResourceSlotComparator::operator()(
    const HtmlResourceSlotPtr& p, const HtmlResourceSlotPtr& q) const {
  // Note: The ordering depends on pointer comparison and so is arbitrary
  // and non-deterministic.
  if (p->element() < q->element()) {
    return true;
  } else if (p->element() > q->element()) {
    return false;
  }
  return (p->attribute() < q->attribute());
}

}  // namespace net_instaweb
