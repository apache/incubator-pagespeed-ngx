/*
 * Copyright 2016 Google Inc.
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
//
// Contains special slots that help rewrite images inside srcset attributes.

#include "net/instaweb/rewriter/public/srcset_slot.h"

#include <vector>

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

SrcSetSlotCollection::SrcSetSlotCollection(
    RewriteDriver* driver, HtmlElement* element,
    HtmlElement::Attribute* attribute)
    : driver_(driver), element_(element), attribute_(attribute),
      filter_(nullptr),
      // Note: these need to be deep-copied in case we run as a detached
      // rewrite, in which case element_ may be dead.
      begin_line_number_(element->begin_line_number()),
      end_line_number_(element->end_line_number()) {
}

void SrcSetSlotCollection::Initialize(CommonFilter* filter) {
  filter_ = filter;

  StringPiece input(attribute_->DecodedValueOrNull());
  ParseSrcSet(input, &candidates_);

  for (int i = 0, n = candidates_.size(); i < n; ++i) {
    GoogleString url = candidates_[i].url;
    if (!url.empty()) {
      // TODO(morlovich): Different filters may different policy wrt to
      // inlining unknown; make it explicit somewhere that this relies on
      // them being consistent about it if shared between filters.
      ResourcePtr resource(filter->CreateInputResourceOrInsertDebugComment(
                               candidates_[i].url, element_));
      if (resource.get() != nullptr) {
        candidates_[i].slot = new SrcSetSlot(resource, this, i);
      }
    }
  }
}

void SrcSetSlotCollection::ParseSrcSet(
    StringPiece input, std::vector<ImageCandidate>* out) {
  out->clear();

  // ref: https://html.spec.whatwg.org/multipage/embedded-content.html#parse-a-srcset-attribute
  while (true) {
    // Strip leading whitespace, commas.
    while (!input.empty() &&
           (IsHtmlSpace(input[0]) || input[0] == ',')) {
      input.remove_prefix(1);
    }

    if (input.empty()) {
      break;
    }

    // Find where the URL ends --- it's WS terminated.
    stringpiece_ssize_type url_end = input.find_first_of(" \f\n\r\t");
    StringPiece url;

    if (url_end == StringPiece::npos) {
      url = input;
      input.clear();
    } else {
      url = input.substr(0, url_end);
      input = input.substr(url_end);
    }

    // URL may have trailing commas, which also means there is no
    // descriptor.
    bool expect_descriptor = true;
    while (url.ends_with(",")) {
      url.remove_suffix(1);
      expect_descriptor = false;
    }

    StringPiece descriptor;
    if (expect_descriptor) {
      bool inside_paren = false;
      int pos, n;
      for (pos = 0, n = input.size(); pos < n; ++pos) {
        if (input[pos] == '(') {
          inside_paren = true;
        } else if (input[pos] == ')' && inside_paren) {
          inside_paren = false;
        } else if (input[pos] == ',' && !inside_paren) {
          break;
        }
      }
      descriptor = input.substr(0, pos);
      input = input.substr(pos);
      TrimWhitespace(&descriptor);
    }

    ImageCandidate cand;
    url.CopyToString(&cand.url);
    descriptor.CopyToString(&cand.descriptor);
    out->push_back(cand);
  }
}

GoogleString SrcSetSlotCollection::Serialize(
    const std::vector<ImageCandidate>& in) {
  GoogleString result;
  for (int i = 0, n = in.size(); i < n; ++i) {
    if (i != 0) {
      StrAppend(&result, ", ");
    }
    StrAppend(&result, in[i].url);
    if (!in[i].descriptor.empty()) {
      StrAppend(&result, " ", in[i].descriptor);
    }
  }
  return result;
}

void SrcSetSlotCollection::Commit() {
  // Note that if slots don't want to render, they simply end up not changing
  // things in candidates_
  attribute_->SetValue(Serialize(candidates_));
}

SrcSetSlot::SrcSetSlot(const ResourcePtr& resource,
                       SrcSetSlotCollection* parent,
                       int index)
    : ResourceSlot(resource), parent_(parent), index_(index),
      url_relativity_(
          GoogleUrl::FindRelativity(parent_->url(index))) {
}

SrcSetSlot::~SrcSetSlot() {}

void SrcSetSlot::Render() {
  if (disable_rendering() || preserve_urls()) {
    return;
  }

  parent_->set_url(
      index_,
      RelativizeOrPassthrough(
          parent_->driver()->options(), resource()->url(),
          url_relativity_, parent_->driver()->base_url()));
  parent_->Commit();
}

GoogleString SrcSetSlot::LocationString() const {
  GoogleString loc = StrCat(parent_->driver()->id(), ":",
                            IntegerToString(parent_->begin_line_number()));
  if (parent_->end_line_number() != parent_->begin_line_number()) {
    StrAppend(&loc, "-", IntegerToString(parent_->end_line_number()));
  }

  StrAppend(&loc, " srcset entry for ", parent_->descriptor(index_));
  return loc;
}

bool SrcSetSlotCollectionComparator::operator()(
    const SrcSetSlotCollectionPtr& p, const SrcSetSlotCollectionPtr& q) const {
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
