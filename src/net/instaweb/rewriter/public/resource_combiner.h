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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_COMBINER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_COMBINER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/url_partnership.h"
#include "net/instaweb/util/public/stl_util.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_multipart_encoder.h"
#include "net/instaweb/http/public/url_async_fetcher.h"

namespace net_instaweb {

class ContentType;
class HtmlElement;
class MessageHandler;
class OutputResource;
class RequestHeaders;
class Resource;
class ResourceManager;
class ResponseHeaders;
class RewriteDriver;
class RewriteDriver;
class Variable;
class Writer;

// This class is a utility for filters that combine multiple resource
// files into one. It provides two major pieces of functionality to help out:
// 1) It keeps a ResourceVector and provides methods to keep track
//    of resources and URLs that can be safely combined together while
//    encoding the information on the pieces in the combined URL.
// 2) It implements Fetch, reconstructing combinations as needed.
class ResourceCombiner {
 public:
  // Slack to leave in URL size, so that other filters running afterwards
  // can expand the URLs without going over maximum allowed sizes.
  //
  // Why 100? First example I saw, CssFilter expanded a CssCombined URL
  // by 36 chars. So 100 seemed like a nice round number to allow two
  // filters to run after this and then for there still be a little slack.
  //
  // TODO(sligocki): Set this more intelligently.
  static const int kUrlSlack = 100;

  // Note: extension should not include the leading dot here.  Before calling
  // AddResource, and on each new document, you must call Reset to provide a
  // base url.
  ResourceCombiner(RewriteDriver* rewrite_driver,
                   const StringPiece& path_prefix,
                   const StringPiece& extension);

  virtual ~ResourceCombiner();

  bool Fetch(OutputResource* resource,
             Writer* writer,
             const RequestHeaders& request_header,
             ResponseHeaders* response_headers,
             MessageHandler* message_handler,
             UrlAsyncFetcher::Callback* callback);

  // Resets the current combiner to an empty state.  We will keep a pointer
  // to this GURL, so it should live as long as you're using this combiner
  // without calling Reset again.
  // If a subclass needs to do some of its own reseting, see Clear().
  void Reset(const GURL& base_gurl);

  // Computes a name for the URL that meets all known character-set and
  // size restrictions.
  std::string UrlSafeId() const;

  // Returns the number of URLs that have been successfully added.
  int num_urls() const { return partnership_.num_urls(); }

  typedef std::vector<Resource*> ResourceVector;
  const ResourceVector& resources() const { return resources_; }

  // Base common to all URLs. Always has a trailing slash.
  std::string ResolvedBase() const { return partnership_.ResolvedBase(); }

 protected:
  // Tries to add a resource with given source URL to the current partnership.
  // Returns whether successful or not (in which case the partnership will be
  // unchanged). This will succeed only if we both have the data ready and can
  // fit in the names into the combined URL.
  bool AddResource(const StringPiece& url, MessageHandler* handler);

  // Returns one resource containing the combination of all added resources,
  // creating it if necessary.  Caller takes ownership.  Returns NULL if the
  // combination does not exist and cannot be created. Will not combine fewer
  // than 2 resources.
  OutputResource* Combine(const ContentType& content_type,
                          MessageHandler* handler);

  // Override this if your combination is not a matter of combining
  // text pieces (perhaps adjusted by WritePiece)
  virtual bool WriteCombination(const ResourceVector& combine_resources,
                                OutputResource* combination,
                                MessageHandler* handler);

  // Override this to alter how pieces are processed when included inside
  // a combination. Returns whether successful. The default implementation
  // writes input->contents() to the writer without any alteration.
  virtual bool WritePiece(Resource* input, OutputResource* combination,
                          Writer* writer, MessageHandler* handler);

  // Override this if you need to remove some state whenever Reset() is called.
  // Your implementation must call the superclass.
  virtual void Clear();

  const GURL* base_gurl() const { return base_gurl_; }

  ResourceManager* const resource_manager_;
  RewriteDriver* const rewrite_driver_;

 private:
  friend class CombinerCallback;

  // Recomputes the leaf size if our base has changed
  void UpdateResolvedBase();

  // Computes the total size
  void ComputeLeafSize();

  // Incrementally updates the accumulated leaf size without re-examining
  // every element in the combined file.
  void AccumulateLeafSize(const StringPiece& url);

  // Determines whether our accumulated leaf size is too big, taking into
  // account both per-segment and total-url limitations.
  bool UrlTooBig();

  // Override this if you need to forbid some combinations based on the
  // content of the resource (e.g. with resource->ContentsValid())
  // This is called before the URL is added to UrlPartnership's
  // data structures.
  virtual bool ResourceCombinable(Resource* resource,
                                  MessageHandler* handler);

  UrlPartnership partnership_;
  std::vector<Resource*> resources_;
  UrlMultipartEncoder multipart_encoder_;
  int prev_num_components_;
  int accumulated_leaf_size_;
  std::string resolved_base_;
  const int url_overhead_;
  const GURL* base_gurl_;
  std::string filter_prefix_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCombiner);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_COMBINER_H_
