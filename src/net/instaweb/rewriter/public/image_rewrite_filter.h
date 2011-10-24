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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_REWRITE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_REWRITE_FILTER_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
class CachedResult;
class ContentType;
class Image;
class ImageTagScanner;
class ResourceContext;
class RewriteContext;
class RewriteDriver;
class Statistics;
class TimedVariable;
class UrlSegmentEncoder;
class Variable;
class WorkBound;

// Identify img tags in html and optimize them.
// TODO(jmaessen): Big open question: how best to link pulled-in resources to
//     rewritten urls, when in general those urls will be in a different domain.
class ImageRewriteFilter : public RewriteSingleResourceFilter {
 public:
  ImageRewriteFilter(RewriteDriver* driver,
                     StringPiece path_prefix);
  virtual ~ImageRewriteFilter();
  static void Initialize(Statistics* statistics);
  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element);
  virtual const char* Name() const { return "ImageRewrite"; }

  // Can we inline resource?  If so, encode its contents into the data_url,
  // otherwise leave data_url alone.
  static bool CanInline(
      int image_inline_max_bytes, const StringPiece& contents,
      const ContentType* content_type, GoogleString* data_url);

  // Creates a nested rewrite for given parent and slot, and returns it.
  // The result is not registered with the parent.
  RewriteContext* MakeNestedContext(RewriteContext* parent,
                                    const ResourceSlotPtr& slot);

  // name for statistic used to bound rewriting work.
  static const char kImageOngoingRewrites[];

  // TimedVariable denoting image rewrites we dropped due to
  // load (too many concurrent rewrites)
  static const char kImageRewritesDroppedDueToLoad[];

 protected:
  // Interface to RewriteSingleResourceFilter
  virtual RewriteResult RewriteLoadedResource(const ResourcePtr& input_resource,
                                              const OutputResourcePtr& result);
  virtual int FilterCacheFormatVersion() const;
  virtual bool ReuseByContentHash() const;
  virtual const UrlSegmentEncoder* encoder() const;

  virtual bool HasAsyncFlow() const;
  virtual RewriteContext* MakeRewriteContext();

 private:
  class Context;
  friend class Context;

  // Helper methods.
  const ContentType* ImageToContentType(const GoogleString& origin_url,
                                        Image* image);
  void BeginRewriteImageUrl(HtmlElement* element, HtmlElement::Attribute* src);

  RewriteResult RewriteLoadedResourceImpl(RewriteContext* context,
                                          const ResourcePtr& input_resource,
                                          const OutputResourcePtr& result);


  // Returns true if it rewrote the URL.
  bool FinishRewriteImageUrl(
      const CachedResult* cached, const ResourceContext* resource_context,
      HtmlElement* element, HtmlElement::Attribute* src);

  // Populates width and height with the attributes specified in the
  // image tag (including in an inline style attribute).
  bool GetDimensions(HtmlElement* element, int* width, int* height);

  // Returns true if there is either a width or height attribute specified,
  // even if they're not parsable.
  bool HasAnyDimensions(HtmlElement* element);

  scoped_ptr<const ImageTagScanner> image_filter_;
  scoped_ptr<WorkBound> work_bound_;
  Variable* rewrite_count_;
  Variable* inline_count_;
  Variable* rewrite_saved_bytes_;
  Variable* webp_count_;
  TimedVariable* image_rewrites_dropped_;
  ImageUrlEncoder encoder_;

  DISALLOW_COPY_AND_ASSIGN(ImageRewriteFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_REWRITE_FILTER_H_
