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

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CachedResult;
class ContentType;
class ImageDim;
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
class ImageRewriteFilter : public RewriteFilter {
 public:
  explicit ImageRewriteFilter(RewriteDriver* driver);
  virtual ~ImageRewriteFilter();
  static void InitStats(Statistics* statistics);
  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element);
  virtual const char* Name() const { return "ImageRewrite"; }
  virtual const char* id() const { return RewriteOptions::kImageCompressionId; }

  // Can we inline resource?  If so, encode its contents into the data_url,
  // otherwise leave data_url alone.
  bool TryInline(
      int64 image_inline_max_bytes, const CachedResult* cached_result,
      ResourceSlot* slot, GoogleString* data_url);

  // The valid contents of a dimension attribute on an image element have one of
  // the following forms: "45%" "45%px" "+45.0%" [45% of browser width; we can't
  // handle this] "45", "+45", "45px", "45arbitraryjunk" "45px%" [45 pixels
  // regardless of junk] Technically 0 is an invalid dimension, so we'll reject
  // those as well; note that 0 dimensions occur in the wild and Safari and
  // Chrome at least don't display anything.
  //
  // We actually reject the arbitraryjunk cases, as older browsers (eg FF9,
  // which isn't *that* old) don't deal with them at all.  So the only trailing
  // stuff we allow is px possibly with some white space.  Note that some older
  // browsers (like FF9) accept other units such as "in" or "pt" as synonyms for
  // px!
  //
  // We round fractions, as fractional pixels appear to be rounded in practice
  // (and our image resize algorithms require integer pixel sizes).
  //
  // Far more detail in the spec at:
  //   http://www.whatwg.org/specs/web-apps/current-work/multipage/
  //                  common-microsyntaxes.html#percentages-and-dimensions
  static bool ParseDimensionAttribute(const char* position, int* value);

  // Creates a nested rewrite for an image inside a CSS file with the given
  // parent and slot, and returns it. The result is not registered with the
  // parent.
  RewriteContext* MakeNestedRewriteContextForCss(
      int64 css_image_inline_max_bytes,
      RewriteContext* parent,
      const ResourceSlotPtr& slot);

  // Creates a nested rewrite for the given parent and slot and returns it. The
  // result is not registered with the parent.
  virtual RewriteContext* MakeNestedRewriteContext(RewriteContext* parent,
                                                   const ResourceSlotPtr& slot);

  // Update desired image dimensions if necessary. Returns true if it is
  // updated.
  static bool UpdateDesiredImageDimsIfNecessary(
      const ImageDim& image_dim, RewriteDriver* driver, ImageDim* desired_dim);

  // name for statistic used to bound rewriting work.
  static const char kImageOngoingRewrites[];

  // # of images that we decided not to rewrite because of size constraint.
  static const char kImageNoRewritesHighResolution[];

  // TimedVariable denoting image rewrites we dropped due to
  // load (too many concurrent rewrites)
  static const char kImageRewritesDroppedDueToLoad[];

  // # of images not rewritten because the image MIME type is unknown.
  static const char kImageRewritesDroppedMIMETypeUnknown[];

  // # of images not rewritten because the server fails to write the merged
  // html files.
  static const char kImageRewritesDroppedServerWriteFail[];

  // # of images not rewritten because the rewriting does not reduce the
  // data size by a certain threshold. The image is resized in this case.
  static const char kImageRewritesDroppedNoSavingResize[];

  // # of images not rewritten because the rewriting does not reduce the
  // data size by a certain threshold. The image is not resized in this case.
  static const char kImageRewritesDroppedNoSavingNoResize[];

  // The property cache property name used to store URLs discovered when
  // image_inlining_identify_and_cache_without_rewriting() is set in the
  // RewriteOptions.
  static const char kInlinableImageUrlsPropertyName[];

 protected:
  virtual const UrlSegmentEncoder* encoder() const;

  virtual RewriteContext* MakeRewriteContext();

 private:
  class Context;
  friend class Context;

  // Helper methods.
  const ContentType* ImageToContentType(const GoogleString& origin_url,
                                        Image* image);
  void BeginRewriteImageUrl(HtmlElement* element, HtmlElement::Attribute* src);

  RewriteResult RewriteLoadedResourceImpl(Context* context,
                                          const ResourcePtr& input_resource,
                                          const OutputResourcePtr& result);

  // Returns true if it rewrote (ie inlined) the URL.
  bool FinishRewriteCssImageUrl(
      int64 css_image_inline_max_bytes,
      const CachedResult* cached, ResourceSlot* slot);

  // Returns true if it rewrote the URL.
  bool FinishRewriteImageUrl(
      const CachedResult* cached, const ResourceContext* resource_context,
      HtmlElement* element, HtmlElement::Attribute* src, int image_index,
      ResourceSlot* slot);

  // Save image contents in cached if the image is inlinable.
  void SaveIfInlinable(const StringPiece& contents,
                       const Image::Type image_type,
                       CachedResult* cached);

  // Populates width and height with the attributes specified in the
  // image tag (including in an inline style attribute).
  void GetDimensions(HtmlElement* element, ImageDim* page_dim);

  // Returns true if there is either a width or height attribute specified,
  // even if they're not parsable.
  bool HasAnyDimensions(HtmlElement* element);

  // Resizes low quality image. It further reduces the size of inlined low
  // quality image for mobile.
  void ResizeLowQualityImage(
      Image* low_image, const ResourcePtr& input_resource,
      CachedResult* cached);

  // Checks if image is critical to generate low res image for the given image.
  // An image is considered critical if it is in the critical list as determined
  // by CriticalImageFinder. Images are considered critical if the platform
  // lacks a CriticalImageFinder implementation.
  bool IsCriticalImage(const StringPiece& image_url) const;

  // Persist a URL that would have be inlined to the property cache, if
  // options()->image_inlining_identify_and_cache_without_rewriting(). Returns
  // true if a PropertyValue was written.
  bool StoreUrlInPropertyCache(const StringPiece& url);

  // Set attempt_webp in resource_context.
  void SetAttemptWebp(StringPiece url, ResourceContext* resource_context);

  scoped_ptr<WorkBound> work_bound_;

  // Statistics

  // # of images rewritten successfully.
  Variable* image_rewrites_;
  // # of images that we decided not to rewrite because of size constraint.
  Variable* image_norewrites_high_resolution_;
  // # of images that we decided not to serve rewritten. This could be because
  // the rewrite failed, recompression wasn't effective enough, the image
  // couldn't be resized because it had an alpha-channel, etc.
  Variable* image_rewrites_dropped_intentionally_;
  // # of images not rewritten because the image MIME type is unknown.
  Variable* image_rewrites_dropped_mime_type_unknown_;
  // # of images not rewritten because the server fails to write the merged
  // html files.
  Variable* image_rewrites_dropped_server_write_fail_;
  // # of images not rewritten because the rewriting does not reduce the
  // data size by a certain threshold. The image is resized in this case.
  Variable* image_rewrites_dropped_nosaving_resize_;
  // # of images not rewritten because the rewriting does not reduce the
  // data size by a certain threshold. The image is not resized in this case.
  Variable* image_rewrites_dropped_nosaving_noresize_;
  // # of images not rewritten because of load.
  TimedVariable* image_rewrites_dropped_due_to_load_;
  // # of bytes saved from image rewriting (Note: This is computed at
  // rewrite time not at serve time, so the number of bytes saved in
  // transmission should be larger than this).
  Variable* image_rewrite_total_bytes_saved_;
  // Sum of original sizes of all successfully rewritten images.
  // image_rewrite_total_bytes_saved_ / image_rewrite_total_original_bytes_
  // is the average percentage reduction in image size.
  Variable* image_rewrite_total_original_bytes_;
  // # of uses of rewritten images (updating <img> src= attributes in HTML
  // or url()s in CSS).
  Variable* image_rewrite_uses_;
  // # of inlines of images (into HTML or CSS).
  Variable* image_inline_count_;
  // # of images rewritten into WebP format.
  Variable* image_webp_rewrites_;

  ImageUrlEncoder encoder_;

  // Counter to help associate each <img> tag in the HTML with a unique index,
  // for use in determining whether the image should be previewed.
  int image_counter_;

  // The set of inlinable URLs, populated as the page is parsed, if
  // image_inlining_identify_and_cache_without_rewriting() is set in the
  // RewriteOptions.
  StringSet inlinable_urls_;

  DISALLOW_COPY_AND_ASSIGN(ImageRewriteFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_REWRITE_FILTER_H_
