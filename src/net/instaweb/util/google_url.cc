/*
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

#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {
// Returns the offset at which the leaf starts in the fully
// qualified spec.
size_t LeafStartPosition(const GURL& gurl) {
  url_parse::Parsed parsed = gurl.parsed_for_possibly_invalid_spec();
  // query doesn't include '?'
  size_t question = parsed.query.begin - 1;
  if (question < 0) {
    question = std::string::npos;
  }
  size_t last_slash = gurl.spec().rfind('/', question);
  return last_slash;
}

// Find the start of the path, includes '/'
size_t PathStartPosition(const GURL& gurl) {
  std::string spec = gurl.spec();
  url_parse::Parsed parsed = gurl.parsed_for_possibly_invalid_spec();
  size_t origin_size = parsed.path.begin;
  if (!parsed.path.is_valid()) {
    origin_size = spec.size();
  }
  CHECK_LT(0, static_cast<int>(origin_size));
  CHECK_LE(origin_size, spec.size());
  return origin_size;
}

}  // namespace


// Find the last slash before the question-mark, if any.  See
// http://en.wikipedia.org/wiki/URI_scheme -- the query-string
// syntax is not well-defined.  But the query-separator is well-defined:
// it's a ? so I believe this implies that the first ? has to delimit
// the query string.
std::string GoogleUrl::AllExceptLeaf(const GURL& gurl) {
  CHECK(gurl.is_valid());
  std::string spec_str = gurl.spec();
  size_t last_slash = LeafStartPosition(gurl);
  CHECK(last_slash != std::string::npos);
  return std::string(spec_str.data(), last_slash + 1);

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

std::string GoogleUrl::LeafWithQuery(const GURL& gurl) {
  std::string spec_str = gurl.spec();
  size_t last_slash = LeafStartPosition(gurl);
  CHECK(last_slash != spec_str.npos);
  return std::string(spec_str.data() + last_slash + 1,
                      spec_str.size() - (last_slash + 1));
}

std::string GoogleUrl::LeafSansQuery(const GURL& gurl) {
  std::string spec_str = gurl.spec();
  size_t after_last_slash = LeafStartPosition(gurl) + 1;
  size_t leaf_length = spec_str.size() - after_last_slash;
  if (!gurl.has_query()) {
    return std::string(spec_str.data() + after_last_slash,
                        leaf_length);
  }
  url_parse::Parsed parsed = gurl.parsed_for_possibly_invalid_spec();
  // parsed.query.len doesn't include the '?'
  return std::string(spec_str.data() + after_last_slash,
                      leaf_length - (parsed.query.len + 1));
}

// For "http://a.com/b/c/d?e=f/g returns "http://a.com" without trailing slash
std::string GoogleUrl::Origin(const GURL& gurl) {
  std::string spec = gurl.spec();
  size_t origin_size = PathStartPosition(gurl);
  return std::string(spec.data(), origin_size);
}

// For "http://a.com/b/c/d?e=f/g returns "/b/c/d?e=f/g" including leading slash
std::string GoogleUrl::PathAndLeaf(const GURL& gurl) {
  std::string spec = gurl.spec();
  size_t origin_size = PathStartPosition(gurl);
  return std::string(spec.data() + origin_size, spec.size() - origin_size);
}

// For "http://a.com/b/c/d/g.html returns "/b/c/d/" including leading and
// trailing slashes.
// For queries, "http://a.com/b/c/d?E=f/g" returns "/b/c/".
std::string GoogleUrl::PathSansLeaf(const GURL& gurl) {
  std::string spec = gurl.spec();
  size_t path_start = PathStartPosition(gurl);
  size_t leaf_start = LeafStartPosition(gurl);
  return std::string(spec.data() + path_start,
                      leaf_start - path_start + 1);
}

}  // namespace net_instaweb
