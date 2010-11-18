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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_IMG_REWRITE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_IMG_REWRITE_FILTER_H_

#include "net/instaweb/rewriter/public/rewrite_filter.h"

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/img_tag_scanner.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/util/public/atom.h"
#include <string>
#include "net/instaweb/util/public/url_segment_encoder.h"
#include "net/instaweb/util/public/work_bound.h"

namespace net_instaweb {

class ContentType;
class FileSystem;
class HtmlParse;
class Image;
class ImageDim;
class OutputResource;
class ResourceManager;
class UrlEscaper;
class Variable;

// This class supports the encoding of image urls with optional
// additional dimension metadata.  The passed-in stored_dim is used as
// the source and/or destination of this metadata during encode/decode
// respectively.
class ImageUrlEncoder : public UrlSegmentEncoder {
 public:
  ImageUrlEncoder(UrlEscaper* url_escaper, ImageDim* stored_dim);
  virtual ~ImageUrlEncoder();

  // Encode an origin_url and stored_dim from origin page to a rewritten_url.
  virtual void EncodeToUrlSegment(
      const StringPiece& origin_url, std::string* rewritten_url);

  // Decode an origin_url and stored_dim from a rewritten_url, returning false
  // on parse failure (invalidating output vars).
  virtual bool DecodeFromUrlSegment(const StringPiece& rewritten_url,
                                    std::string* origin_url);

 private:
  UrlEscaper* url_escaper_;
  ImageDim* stored_dim_;
};

// Identify img tags in html and optimize them.
// TODO(jmaessen): See which ones have immediately-obvious size info.
// TODO(jmaessen): Provide alternate resources at rewritten urls
//     asynchronously somehow.
// TODO(jmaessen): Big open question: how best to link pulled-in resources to
//     rewritten urls, when in general those urls will be in a different domain.
class ImgRewriteFilter : public RewriteFilter {
 public:
  ImgRewriteFilter(RewriteDriver* driver,
                   bool log_image_elements,
                   bool insert_image_dimensions,
                   StringPiece path_prefix,
                   size_t img_inline_max_bytes,
                   size_t img_max_rewrites_at_once);
  static void Initialize(Statistics* statistics);
  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element);
  virtual void Flush();
  virtual bool Fetch(OutputResource* resource,
                     Writer* writer,
                     const MetaData& request_header,
                     MetaData* response_headers,
                     MessageHandler* message_handler,
                     UrlAsyncFetcher::Callback* callback);
  virtual const char* Name() const { return "ImgRewrite"; }

  // Can we inline resource?  If so, encode its contents into the data_url,
  // otherwise leave data_url alone.
  static bool CanInline(
      int img_inline_max_bytes, const StringPiece& contents,
      const ContentType* content_type, std::string* data_url);

 private:
  // Helper methods.
  Image* GetImage(const StringPiece& origin_url, Resource* img_resource);
  OutputResource* ImageOutputResource(const std::string& url_string,
                                      Image* image);
  const ContentType* ImageToContentType(const std::string& origin_url,
                                        Image* image);
  void OptimizeImage(const Resource& input_resource, const ImageDim& page_dim,
                     Image* image, OutputResource* result);
  bool OptimizedImageFor(
      const StringPiece& origin_url, const ImageDim& page_dim,
      Resource* img_resource, OutputResource* output);
  void RewriteImageUrl(HtmlElement* element, HtmlElement::Attribute* src);
  void UpdateTargetElement(const Resource& input_resource,
                           const OutputResource& output_resource,
                           const ImageDim& page_dim, const ImageDim& actual_dim,
                           HtmlElement* element, HtmlElement::Attribute* src);

  FileSystem* file_system_;
  HtmlParse* html_parse_;
  scoped_ptr<ImgTagScanner> img_filter_;
  ResourceManager* resource_manager_;
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
  // Should we log each image element as we encounter it?  Handy for debug.
  bool log_image_elements_;
  // Should we insert image dimensions into html if they are absent?
  bool insert_image_dimensions_;
  const Atom s_width_;
  const Atom s_height_;
  Variable* rewrite_count_;
  Variable* inline_count_;
  Variable* rewrite_saved_bytes_;

  DISALLOW_COPY_AND_ASSIGN(ImgRewriteFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IMG_REWRITE_FILTER_H_
