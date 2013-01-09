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
//
// Provides the ResourceCombiner class which helps implement filters that
// combine multiple resources, as well as the TimedBool, which is useful
// when figuring out how long results are valid for.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_COMBINER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_COMBINER_H_

#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/url_partnership.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest_prod.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

struct ContentType;
class MessageHandler;
class OutputResource;
class RewriteDriver;
class RewriteFilter;
class Writer;

// A boolean with an expiration date.
struct TimedBool {
  // A date, in milliseconds since the epoch, after which value should
  // no longer be considered valid.
  int64 expiration_ms;
  bool value;
};

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

  // Note: extension should not include the leading dot here.
  ResourceCombiner(RewriteDriver* rewrite_driver,
                   const StringPiece& extension,
                   RewriteFilter* filter);

  virtual ~ResourceCombiner();

  // Resets the current combiner to an empty state, incorporating the base URL.
  // Make sure this gets called before documents --- on a ::Flush() is enough.
  // If a subclass needs to do some of its own reseting, see Clear().
  void Reset();

  // Computes a name for the URL that meets all known character-set and
  // size restrictions.
  GoogleString UrlSafeId() const;

  // Returns the number of URLs that have been successfully added.
  int num_urls() const { return partnership_.num_urls(); }

  const ResourceVector& resources() const { return resources_; }

  // Base common to all URLs. Always has a trailing slash.
  GoogleString ResolvedBase() const { return partnership_.ResolvedBase(); }

  // TODO(jmarantz): rename this to AddResource now that async flow is live.
  TimedBool AddResourceNoFetch(const ResourcePtr& resource,
                               MessageHandler* handler);

 protected:
  // Removes the last resource that was added here, assuming the last call to
  // AddResource was successful.  If the last call to AddResource returned
  // false, behavior is undefined.
  virtual void RemoveLastResource();

  // Returns one resource containing the combination of all added resources,
  // creating it if necessary.  Caller takes ownership.  Returns NULL if the
  // combination does not exist and cannot be created. Will not combine fewer
  // than 2 resources.
  OutputResourcePtr Combine(MessageHandler* handler);

  // Override this if your combination is not a matter of combining
  // text pieces (perhaps adjusted by WritePiece).
  virtual bool WriteCombination(const ResourceVector& combine_resources,
                                const OutputResourcePtr& combination,
                                MessageHandler* handler);

  // Override this to alter how pieces are processed when included inside
  // a combination. Returns whether successful. The default implementation
  // writes input->contents() to the writer without any alteration.
  // 'index' is the position of this piece in the combination.
  virtual bool WritePiece(int index, const Resource* input,
                          OutputResource* combination, Writer* writer,
                          MessageHandler* handler);

  // Override this if you need to remove some state whenever Reset() is called.
  // Your implementation must call the superclass.
  virtual void Clear();

  ServerContext* const server_context_;
  RewriteDriver* const rewrite_driver_;

 private:
  friend class AggregateCombiner;

  // Implement this to control the content-type given the combination.
  virtual const ContentType* CombinationContentType() = 0;

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

  // Computes the total combined resources size.
  // Override this if combined resource size to be restricted to some limit.
  virtual void AccumulateCombinedSize(const ResourcePtr& resource) {}

  // Determines whether our accumulated resources size is too big.
  // Override this if combined resource size to be restricted to some limit.
  virtual bool ContentSizeTooBig() const { return false; }

  // Override this if you need to forbid some combinations based on the
  // content of the resource (e.g. with resource->HttpStatusOk())
  // This is called before the URL is added to UrlPartnership's
  // data structures.
  virtual bool ResourceCombinable(Resource* resource,
                                  MessageHandler* handler);

  UrlPartnership partnership_;
  ResourceVector resources_;
  StringVector multipart_encoder_urls_;
  int prev_num_components_;
  int accumulated_leaf_size_;
  GoogleString resolved_base_;
  const int url_overhead_;
  RewriteFilter* filter_;

  FRIEND_TEST(ResourceCombinerTest, TestRemove);
  FRIEND_TEST(ResourceCombinerTest, TestRemoveFrom3);
  FRIEND_TEST(ResourceCombinerTest, TestRebaseRemove);
  FRIEND_TEST(ResourceCombinerTest, TestRebaseRemoveAdd);

  DISALLOW_COPY_AND_ASSIGN(ResourceCombiner);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_COMBINER_H_
