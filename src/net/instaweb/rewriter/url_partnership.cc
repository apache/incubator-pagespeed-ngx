/**
 * Copyright 2010 Google Inc.
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

#include "net/instaweb/rewriter/public/url_partnership.h"

#include <algorithm>  // for std::min
#include <string>
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/stl_util.h"

namespace net_instaweb {

UrlPartnership::UrlPartnership(const DomainLawyer* domain_lawyer,
                               const GURL& original_request)
    : domain_lawyer_(domain_lawyer) {
  if (original_request.is_valid()) {
    original_origin_and_path_ = GoogleUrl::Create(
        GoogleUrl::AllExceptLeaf(original_request) + "/");
  }
}

UrlPartnership::~UrlPartnership() {
  STLDeleteElements(&gurl_vector_);
}

// Adds a URL to a combination.  If it can be legally added, consulting
// the DomainLaywer, then true is returned.  AddUrl cannot be called
// after Resolve (CHECK failure).
bool UrlPartnership::AddUrl(const StringPiece& untrimmed_resource_url,
                            MessageHandler* handler) {
  std::string resource_url, mapped_domain_name;
  bool ret = false;
  TrimWhitespace(untrimmed_resource_url, &resource_url);

  if (resource_url.empty()) {
    handler->Message(kInfo, "Cannot rewrite empty URL relative to %s",
                     original_origin_and_path_.possibly_invalid_spec().c_str());
  }
  else if (!original_origin_and_path_.is_valid()) {
    handler->Message(kInfo, "Cannot rewrite %s relative to invalid url %s",
                     resource_url.c_str(),
                     original_origin_and_path_.possibly_invalid_spec().c_str());
  } else if (domain_lawyer_->MapRequestToDomain(
      original_origin_and_path_, resource_url, &mapped_domain_name,
      handler)) {
    if (gurl_vector_.empty()) {
      domain_.swap(mapped_domain_name);
      ret = true;
    } else {
      ret = (domain_ == mapped_domain_name);
    }

    if (ret) {
      // TODO(jmarantz): Consider getting the GURL out of the
      // DomainLawyer instead of recomputing it.
      GURL gurl = GoogleUrl::Resolve(original_origin_and_path_, resource_url);
      CHECK(gurl.is_valid() && gurl.SchemeIs("http"));
      gurl_vector_.push_back(new GURL(gurl));
      int index = gurl_vector_.size() - 1;
      IncrementalResolve(index);
    }
  }
  return ret;
}

void UrlPartnership::RemoveLast() {
  CHECK(!gurl_vector_.empty());
  int last = gurl_vector_.size() - 1;
  delete gurl_vector_[last];
  gurl_vector_.resize(last);

  // Re-resolve the entire partnership in the absense of the influence of the
  // ex-partner, by re-adding the GURLs one at a time.
  common_components_.clear();
  for (int i = 0, n = gurl_vector_.size(); i < n; ++i) {
    IncrementalResolve(i);
  }
}

void UrlPartnership::IncrementalResolve(int index) {
  CHECK_LE(0, index);
  CHECK_LT(index, static_cast<int>(gurl_vector_.size()));

  // When tokenizing a URL, we don't want to omit empty segments
  // because we need to avoid aliasing "http://x" with "http://x".
  bool omit_empty = false;
  std::vector<StringPiece> components;

  if (index == 0) {
    std::string base = GoogleUrl::AllExceptLeaf(*gurl_vector_[0]);
    SplitStringPieceToVector(base, "/", &components, omit_empty);
    CHECK_LE(3U, components.size());  // expect {"http:", "", "x"...}
    for (size_t i = 0; i < components.size(); ++i) {
      const StringPiece& sp = components[i];
      common_components_.push_back(std::string(sp.data(), sp.size()));
    }
  } else {
    // Split each string on / boundaries, then compare these path elements
    // until one doesn't match, then shortening common_components.
    std::string all_but_leaf = GoogleUrl::AllExceptLeaf(*gurl_vector_[index]);
    SplitStringPieceToVector(all_but_leaf, "/", &components, omit_empty);
    CHECK_LE(3U, components.size());  // expect {"http:", "", "x"...}

    if (components.size() < common_components_.size()) {
      common_components_.resize(components.size());
    }
    for (size_t c = 0; c < common_components_.size(); ++c) {
      if (common_components_[c] != components[c]) {
        common_components_.resize(c);
        break;
      }
    }
  }
}

std::string UrlPartnership::ResolvedBase() const {
  std::string ret;
  if (!common_components_.empty()) {
    for (size_t c = 0; c < common_components_.size(); ++c) {
      const std::string& component = common_components_[c];
      ret += component;
      ret += "/";  // initial segment is "http" with no leading /
    }
  }
  return ret;
}

// Returns the relative path of a particular URL that was added into
// the partnership.  This requires that Resolve() be called first.
std::string UrlPartnership::RelativePath(int index) const {
  std::string resolved_base = ResolvedBase();
  std::string spec = gurl_vector_[index]->spec();
  CHECK_GE(spec.size(), resolved_base.size());
  CHECK_EQ(StringPiece(spec.data(), resolved_base.size()),
           StringPiece(resolved_base));
  return std::string(spec.data() + resolved_base.size(),
                      spec.size() - resolved_base.size());
}

}  // namespace net_instaweb
