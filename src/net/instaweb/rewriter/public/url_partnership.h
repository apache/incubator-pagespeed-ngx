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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_URL_PARTNERSHIP_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_URL_PARTNERSHIP_H_

#include <vector>
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"
#include <string>

class GURL;

namespace net_instaweb {

class MessageHandler;
class RewriteOptions;

// A URL partnership should be established in order to combine resources,
// such as in CSS combination, JS combination, or image spriting.  This
// class centralizes the handling of such combinations, answering two
// questions:
//   1. Is it legal for a new URL to enter into the partnership
//   2. What is the greatest common prefix
//   3. What are the unique suffices for the elements.
class UrlPartnership {
 public:
  UrlPartnership(const RewriteOptions* options, const GURL& original_request);
  ~UrlPartnership();

  // Adds a URL to a combination.  If it can be legally added, consulting
  // the DomainLaywer, then true is returned.
  bool AddUrl(const StringPiece& resource_url, MessageHandler* handler);

  // Computes the resolved base common to all URLs.  This will always
  // have a trailing slash.
  std::string ResolvedBase() const;

  // Returns the number of URLs that have been successfully added.
  int num_urls() { return gurl_vector_.size(); }

  // Returns the relative path of a particular URL that was added into
  // the partnership.  This requires that Resolve() be called first.
  std::string RelativePath(int index) const;

  // Returns the full resolved path
  const GURL* FullPath(int index) const { return gurl_vector_[index]; }

  // Removes the last URL that was added to the partnership.
  void RemoveLast();

 protected:
  int num_components() const { return common_components_.size(); }
  const RewriteOptions* rewrite_options() const { return rewrite_options_; }

 private:
  void IncrementalResolve(int index);

  typedef std::vector<GURL*> GurlVector;
  GurlVector gurl_vector_;
  std::string domain_;
  GURL domain_gurl_;
  const RewriteOptions* rewrite_options_;
  GURL original_origin_and_path_;

  // common_components_ is updated while adding Urls to support incremental
  // resolution.
  StringVector common_components_;

  DISALLOW_COPY_AND_ASSIGN(UrlPartnership);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_URL_PARTNERSHIP_H_
