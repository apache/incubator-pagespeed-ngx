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

#include <string>
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/stl_util.h"

namespace net_instaweb {

UrlPartnership::~UrlPartnership() {
  STLDeleteElements(&gurl_vector_);
}

// Adds a URL to a combination.  If it can be legally added, consulting
// the DomainLaywer, then true is returned.  AddUrl cannot be called
// after Resolve (CHECK failure).
bool UrlPartnership::AddUrl(const StringPiece& resource_url,
                            MessageHandler* handler) {
  CHECK(!resolved_);
  std::string mapped_domain_name;
  bool ret = false;
  if (!original_request_.is_valid()) {
    handler->Message(kWarning, "Cannot rewrite %s relative to invalid url %s",
                     resource_url.as_string().c_str(),
                     original_request_.possibly_invalid_spec().c_str());
  } else if (domain_lawyer_->MapRequestToDomain(
      original_request_, resource_url, &mapped_domain_name, handler)) {
    if (gurl_vector_.empty()) {
      domain_.swap(mapped_domain_name);
      ret = true;
    } else {
      ret = (domain_ == mapped_domain_name);
    }

    if (ret) {
      // TODO(jmarantz): Consider getting the GURL out of the
      // DomainLawyer instead of recomputing it.  This is deferred
      // for now because DomainLawyer is under review and I don't
      // want to change it.
      std::string url_str(resource_url.data(), resource_url.size());
      GURL gurl = original_request_.Resolve(url_str);
      CHECK(gurl.is_valid() && gurl.SchemeIs("http"));
      gurl_vector_.push_back(new GURL(gurl));
    }
  }
  return ret;
}

namespace {

std::string AllExceptLeaf(const GURL& gurl) {
  CHECK(gurl.is_valid());
  std::string spec_str = gurl.spec();

  // Find the last slash before the question-mark, if any.  See
  // http://en.wikipedia.org/wiki/URI_scheme -- the query-string
  // syntax is not well-defined.  But the query-separator is well-defined:
  // it's a ? so I believe this implies that the first ? has to delimit
  // the query string.
  std::string::size_type question = spec_str.find('?');
  std::string::size_type last_slash = spec_str.find_last_of('/', question);
  CHECK(last_slash != std::string::npos);
  return std::string(spec_str.data(), last_slash);

  // TODO(jmarantz): jmaessen suggests using GURL to do the above this
  // way:
  //  void ResourceNamer::SetNonResourceFields(const GURL& origin) {
  //    // This code looks absolutely horrible, but appears to be the most
  //    // graceful means available to pull the information we need, intact, out
  //    // of origin.
  //    const std::string& spec = origin.possibly_invalid_spec();
  //    const url_parse::Parsed& parsed =
  //  origin.parsed_for_possibly_invalid_spec();
  //    url_parse::Component filename;
  //    url_parse::ExtractFileName(spec.data(), parsed.path, &filename);
  //    // By default, slice off trailing / character from path.
  //    size_t path_len = filename.begin - 1;
  //    if (spec[path_len] != kPathSeparatorChar) {
  //      // We have an incomplete path of some sort without a trailing /
  //      // so include all the characters in the path.
  //      path_len++;
  //    }
  //    host_and_path_.assign(spec.data(), 0, path_len);
  //    size_t filename_end = filename.begin + filename.len;
  //    size_t ext_begin = filename_end;
  //    size_t ext_len = 0;
  //    size_t filename_len = filename.len;
  //    if (filename_len > 0) {
  //      ext_begin = spec.rfind(kSeparatorChar, filename_end);
  //      if (ext_begin < static_cast<size_t>(filename.begin) ||
  //          ext_begin == spec.npos) {
  //        ext_begin = filename_end;
  //      } else {
  //        filename_len = ext_begin - filename.begin;
  //        ext_begin++;
  //        ext_len = filename_end - ext_begin;
  //      }
  //    }
  //    name_.assign(spec.data(), filename.begin, filename_len);
  //    ext_.assign(spec.data(), ext_begin, ext_len);
  //    query_params_.assign(spec.data() + filename_end,
  //                         spec.size() - filename_end);
  //  }
  //
  //  You only need about half these lines, since the above code also fishes
  //  around for an extension and splits off the query params.
}

}  // namespace


// Call after finishing all URLs.
void UrlPartnership::Resolve() {
  if (!resolved_) {
    resolved_ = true;
    if (!gurl_vector_.empty()) {
      std::vector<StringPiece> common_components;
      std::string base = AllExceptLeaf(*gurl_vector_[0]);

      if (gurl_vector_.size() == 1) {
        resolved_base_ = base + "/";
      } else {
        bool omit_empty = false;  // don't corrupt "http://x" by losing '/'
        SplitStringPieceToVector(base, "/", &common_components, omit_empty);
        int num_components = common_components.size();
        CHECK_LE(3, num_components);  // expect at least {"http:", "", "x"}

        // Split each string on / boundaries, then compare these path elements
        // until one doesn't match, then shortening common_components.
        for (int i = 1, n = gurl_vector_.size(); i < n; ++i) {
          std::string all_but_leaf = AllExceptLeaf(*gurl_vector_[i]);
          std::vector<StringPiece> components;
          SplitStringPieceToVector(all_but_leaf, "/", &components, omit_empty);
          CHECK_LE(3U, components.size());  // expect {"http:", "", "x"...}

          if (static_cast<int>(components.size()) < num_components) {
            num_components = components.size();
          }
          for (int c = 0; c < num_components; ++c) {
            if (common_components[c] != components[c]) {
              num_components = c;
              break;
            }
          }
        }

        // Now resurrect the resolved base using the common components.
        CHECK(resolved_base_.empty());
        CHECK_LE(3, num_components);
        for (int c = 0; c < num_components; ++c) {
          common_components[c].AppendToString(&resolved_base_);
          resolved_base_ += "/";  // initial segment is "http" with no leading /
        }
      }

      // TODO(jmarantz): resolve the domain shard if needed.
    }
  }
}

StringPiece UrlPartnership::ResolvedBase() const {
  CHECK(resolved_);
  return resolved_base_;
}

// Returns the relative path of a particular URL that was added into
// the partnership.  This requires that Resolve() be called first.
std::string UrlPartnership::RelativePath(int index) const {
  CHECK(resolved_);
  std::string spec = gurl_vector_[index]->spec();
  CHECK_GT(spec.size(), resolved_base_.size());
  CHECK_EQ(StringPiece(spec.data(), resolved_base_.size()),
           StringPiece(resolved_base_));
  return std::string(spec.data() + resolved_base_.size(),
                      spec.size() - resolved_base_.size());
}

}  // namespace net_instaweb
