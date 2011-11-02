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

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class RewriteDriver;
class RewriteOptions;
class UrlNamer;

// A URL partnership should be established in order to combine resources,
// such as in CSS combination, JS combination, or image spriting.  This
// class centralizes the handling of such combinations, answering two
// questions:
//   1. Is it legal for a new URL to enter into the partnership
//   2. What is the greatest common prefix
//   3. What are the unique suffices for the elements.
class UrlPartnership {
 public:
  explicit UrlPartnership(const RewriteDriver* driver);
  virtual ~UrlPartnership();

  // Adds a URL to a combination.  If it can be legally added, consulting
  // the DomainLaywer, then true is returned.
  bool AddUrl(const StringPiece& resource_url, MessageHandler* handler);

  // Computes the resolved base common to all URLs.  This will always
  // have a trailing slash.
  GoogleString ResolvedBase() const;

  // Returns the number of URLs that have been successfully added.
  int num_urls() const { return url_vector_.size(); }

  // Returns the relative path of a particular URL that was added into
  // the partnership.  This requires that Resolve() be called first.
  GoogleString RelativePath(int index) const;

  // Returns the full resolved path
  const GoogleUrl* FullPath(int index) const { return url_vector_[index]; }

  // Removes the last URL that was added to the partnership.
  void RemoveLast();

  // Establish a new URL as the original request, which is used for
  // domain authorization and mapping of URLs as they are added to the
  // partnership.
  virtual void Reset(const GoogleUrl& original_request);

  // Returns the number of common path components for all resources
  // in this partnership.
  int NumCommonComponents() const { return common_components_.size(); }

 protected:
  const RewriteOptions* rewrite_options() const { return rewrite_options_; }

 private:
  // Every time we add a new URL to a partnership we incrementally update
  // the common_components which helps us track how long the combined URL
  // will be and avoid exceeding URL limits.
  void IncrementalResolve(int index);

  // Based on the UrlNamer and DomainLawyer, find the domain
  // associated with a request, removing any proxy prefix that may
  // have been added by the UrlNamer.  The meaning of "associated" is
  // defined by the UrlNamer implementation.
  bool FindResourceDomain(GoogleUrl* resource,
                          GoogleString* domain,
                          MessageHandler* handler) const;

  typedef std::vector<GoogleUrl*> GurlVector;
  GurlVector url_vector_;
  GoogleString domain_;
  const RewriteOptions* rewrite_options_;
  const UrlNamer* url_namer_;
  GoogleUrl original_origin_and_path_;

  // common_components_ is updated while adding Urls to support incremental
  // resolution.
  StringVector common_components_;

  DISALLOW_COPY_AND_ASSIGN(UrlPartnership);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_URL_PARTNERSHIP_H_
