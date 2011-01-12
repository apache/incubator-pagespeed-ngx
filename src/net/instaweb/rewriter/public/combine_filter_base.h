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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_COMBINE_FILTER_BASE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_COMBINE_FILTER_BASE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/url_partnership.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_multipart_encoder.h"

namespace net_instaweb {

class OutputResource;
class Resource;
class ResourceManager;
class Variable;

// This class is a base for filters that combine multiple resource
// files into one. It provides two major pieces of functionality to help out:
// 1) The CombineFilterBase::Partnership class to keep track
//    of elements and URLs that can be safely combined together while
//    encoding the information on the pieces in the combined URL
// 2) It implements Fetch, reconstructing combinations as needed.
class CombineFilterBase : public RewriteFilter {
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

  // Note: extension should not include the leading dot here
  CombineFilterBase(RewriteDriver* rewrite_driver, const char* path_prefix,
                    const char* extension);

  virtual ~CombineFilterBase();

  virtual bool Fetch(OutputResource* resource,
                     Writer* writer,
                     const RequestHeaders& request_header,
                     ResponseHeaders* response_headers,
                     MessageHandler* message_handler,
                     UrlAsyncFetcher::Callback* callback);

 protected:
  typedef std::vector<Resource*> ResourceVector;

  class Partnership : public UrlPartnership {
   public:
    Partnership(CombineFilterBase* filter, RewriteDriver* driver,
                int url_overhead);
    virtual ~Partnership();

    // Tries to add an element with given source URL to this partnership.
    // Returns whether successful or not (in which case the partnership
    // will be unchanged). This will succeed only if we both have the
    // data ready and can fit in the names into the combined URL.
    bool AddElement(HtmlElement* element, const StringPiece& url,
                    MessageHandler* handler);

    // Computes a name for the URL that meets all known character-set and
    // size restrictions.
    std::string UrlSafeId() const;

    HtmlElement* element(int i) { return elements_[i]; }
    const ResourceVector& resources() const { return resources_; }

   private:
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

    CombineFilterBase* filter_;
    ResourceManager* resource_manager_;
    std::vector<HtmlElement*> elements_;
    std::vector<Resource*> resources_;
    UrlMultipartEncoder multipart_encoder_;
    int prev_num_components_;
    int accumulated_leaf_size_;
    std::string resolved_base_;
    const int url_overhead_;
  };

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

  const int url_overhead_;

 private:
  friend class CombinerCallback;

  DISALLOW_COPY_AND_ASSIGN(CombineFilterBase);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_COMBINE_FILTER_BASE_H_
