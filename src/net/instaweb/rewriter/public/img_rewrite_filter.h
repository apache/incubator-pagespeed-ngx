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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_IMG_REWRITE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_IMG_REWRITE_FILTER_H_

#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/rewriter/public/img_tag_scanner.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/util/public/atom.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/url_segment_encoder.h"
#include "net/instaweb/util/public/work_bound.h"

namespace net_instaweb {

class ContentType;
class FileSystem;
class HtmlParse;
class Image;
class OutputResource;
class ResourceManager;
class Variable;

// Identify img tags in html and optimize them.
// TODO(jmaessen): Big open question: how best to link pulled-in resources to
//     rewritten urls, when in general those urls will be in a different domain.
class ImgRewriteFilter : public RewriteSingleResourceFilter {
 public:
  ImgRewriteFilter(RewriteDriver* driver,
                   StringPiece path_prefix,
                   size_t img_inline_max_bytes,
                   size_t img_max_rewrites_at_once);
  static void Initialize(Statistics* statistics);
  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element);
  virtual const char* Name() const { return "ImgRewrite"; }

  // Can we inline resource?  If so, encode its contents into the data_url,
  // otherwise leave data_url alone.
  static bool CanInline(
      int img_inline_max_bytes, const StringPiece& contents,
      const ContentType* content_type, GoogleString* data_url);

 protected:
  // Interface to RewriteSingleResourceFilter
  virtual RewriteResult RewriteLoadedResource(const Resource* input_resource,
                                              OutputResource* result);
  virtual int FilterCacheFormatVersion() const;
  virtual bool ReuseByContentHash() const;
  virtual const UrlSegmentEncoder* encoder() const;

 private:
  // Helper methods.
  const ContentType* ImageToContentType(const GoogleString& origin_url,
                                        Image* image);
  void RewriteImageUrl(HtmlElement* element, HtmlElement::Attribute* src);

  scoped_ptr<const ImgTagScanner> img_filter_;
  scoped_ptr<WorkBound> work_bound_;
  // Threshold size (in bytes) below which we should just inline images
  // encountered.
  // TODO(jmaessen): Heuristic must be more sophisticated.  Does this image
  // touch a fresh domain?  Require opening a new connection?  If so we can
  // afford to inline quite large images (basically anything we could transmit
  // in the resulting RTTs)---but of course we don't know about RTT here.  In
  // the absence of such information, we ought to inline if header length + url
  // size can be saved by inlining image, without increasing the size in packets
  // of the html.  Otherwise we end up loading the image in favor of the html,
  // which might be a lose.  More work is needed here to figure out the exact
  // tradeoffs involved, especially as we also undermine image cacheability.
  size_t img_inline_max_bytes_;
  Variable* rewrite_count_;
  Variable* inline_count_;
  Variable* rewrite_saved_bytes_;
  ImageUrlEncoder encoder_;

  DISALLOW_COPY_AND_ASSIGN(ImgRewriteFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IMG_REWRITE_FILTER_H_
