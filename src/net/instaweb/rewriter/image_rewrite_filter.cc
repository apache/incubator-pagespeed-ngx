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

#include "net/instaweb/rewriter/public/image_rewrite_filter.h"

#include <limits.h>

#include "base/logging.h"               // for CHECK, etc
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/rewriter/public/image_tag_scanner.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/rewriter/public/local_storage_cache_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/statistics_work_bound.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/work_bound.h"

namespace net_instaweb {

class RewriteContext;
class UrlSegmentEncoder;

// names for Statistics variables.
const char kImageRewrites[] = "image_rewrites";
const char kImageRewritesDroppedIntentionally[] =
    "image_rewrites_dropped_intentionally";
const char ImageRewriteFilter::kImageRewritesDroppedDueToLoad[] =
    "image_rewrites_dropped_due_to_load";
const char kImageRewriteTotalBytesSaved[] = "image_rewrite_total_bytes_saved";
const char kImageRewriteTotalOriginalBytes[] =
    "image_rewrite_total_original_bytes";
const char kImageRewriteUses[] = "image_rewrite_uses";
const char kImageInline[] = "image_inline";
const char ImageRewriteFilter::kImageOngoingRewrites[] =
    "image_ongoing_rewrites";
const char kImageWebpRewrites[] = "image_webp_rewrites";

const int kNotCriticalIndex = INT_MAX;

// This is the resized placeholder image width for mobile.
const int kDelayImageWidthForMobile = 320;

class ImageRewriteFilter::Context : public SingleRewriteContext {
 public:
  Context(int64 css_image_inline_max_bytes,
          ImageRewriteFilter* filter, RewriteDriver* driver,
          RewriteContext* parent, ResourceContext* resource_context,
          bool is_css, int html_index)
      : SingleRewriteContext(driver, parent, resource_context),
        css_image_inline_max_bytes_(css_image_inline_max_bytes),
        filter_(filter),
        driver_(driver),
        is_css_(is_css),
        html_index_(html_index) {}
  virtual ~Context() {}

  virtual void Render();
  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output);
  virtual const char* id() const { return filter_->id(); }
  virtual OutputResourceKind kind() const { return kRewrittenResource; }
  virtual const UrlSegmentEncoder* encoder() const;

 private:
  int64 css_image_inline_max_bytes_;
  ImageRewriteFilter* filter_;
  RewriteDriver* driver_;
  bool is_css_;
  const int html_index_;
  DISALLOW_COPY_AND_ASSIGN(Context);
};

void ImageRewriteFilter::Context::RewriteSingle(
    const ResourcePtr& input_resource,
    const OutputResourcePtr& output_resource) {
  RewriteDone(
      filter_->RewriteLoadedResourceImpl(this, input_resource, output_resource),
      0);
}

void ImageRewriteFilter::Context::Render() {
  if (num_output_partitions() != 1) {
    // Partition failed since one of the inputs was unavailable; nothing to do.
    return;
  }

  CHECK_EQ(1, num_slots());

  const CachedResult* result = output_partition(0);
  bool rewrote_url = false;
  ResourceSlot* resource_slot = slot(0).get();
  if (is_css_) {
    rewrote_url = filter_->FinishRewriteCssImageUrl(css_image_inline_max_bytes_,
                                                    result, resource_slot);
  } else {
    if (!has_parent()) {
      // We use manual rendering for HTML, as we have to consider whether to
      // inline, and may also pass in width and height attributes.
      HtmlResourceSlot* html_slot = static_cast<HtmlResourceSlot*>(
          resource_slot);
      rewrote_url = filter_->FinishRewriteImageUrl(
          result, resource_context(),
          html_slot->element(), html_slot->attribute(), html_index_);
    }
    // Use standard rendering in case the rewrite is nested and not inside CSS.
  }
  if (rewrote_url) {
    // We wrote out the URL ourselves; don't let the default handling mess it up
    // (in particular replacing data: with out-of-line version)
    resource_slot->set_disable_rendering(true);
  }
}

const UrlSegmentEncoder* ImageRewriteFilter::Context::encoder() const {
  return filter_->encoder();
}

ImageRewriteFilter::ImageRewriteFilter(RewriteDriver* driver)
    : RewriteFilter(driver),
      image_filter_(new ImageTagScanner(driver)),
      image_counter_(0) {
  Statistics* stats = resource_manager_->statistics();
  image_rewrites_ = stats->GetVariable(kImageRewrites);
  image_rewrites_dropped_intentionally_ =
      stats->GetVariable(kImageRewritesDroppedIntentionally);
  image_rewrites_dropped_due_to_load_ =
      stats->GetTimedVariable(kImageRewritesDroppedDueToLoad);
  image_rewrite_total_bytes_saved_ =
      stats->GetVariable(kImageRewriteTotalBytesSaved);
  image_rewrite_total_original_bytes_ =
      stats->GetVariable(kImageRewriteTotalOriginalBytes);
  image_rewrite_uses_ = stats->GetVariable(kImageRewriteUses);
  image_inline_count_ = stats->GetVariable(kImageInline);
  Variable* image_ongoing_rewrites = stats->GetVariable(kImageOngoingRewrites);
  image_webp_rewrites_ = stats->GetVariable(kImageWebpRewrites);
  work_bound_.reset(
      new StatisticsWorkBound(image_ongoing_rewrites,
                              driver->options()->image_max_rewrites_at_once()));
}

ImageRewriteFilter::~ImageRewriteFilter() {}

void ImageRewriteFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kImageRewrites);
  statistics->AddVariable(kImageRewritesDroppedIntentionally);
  statistics->AddTimedVariable(kImageRewritesDroppedDueToLoad,
                               ResourceManager::kStatisticsGroup);
  statistics->AddVariable(kImageRewriteTotalBytesSaved);
  statistics->AddVariable(kImageRewriteTotalOriginalBytes);
  statistics->AddVariable(kImageRewriteUses);
  statistics->AddVariable(kImageInline);
  statistics->AddVariable(kImageOngoingRewrites);
  statistics->AddVariable(kImageWebpRewrites);
}

void ImageRewriteFilter::StartDocumentImpl() {
  CriticalImagesFinder* finder =
      driver_->resource_manager()->critical_images_finder();
  if (finder != NULL &&
      driver_->options()->Enabled(RewriteOptions::kDelayImages)) {
    finder->UpdateCriticalImagesSetInDriver(driver_);
    // Compute critical images if critical images information is not present.
    finder->ComputeCriticalImages(driver_->url(), driver_,
                                  (driver_->critical_images() == NULL));
  }
  image_counter_ = 0;
}

namespace {

// Allocate and initialize CompressionOptions object based on RewriteOptions and
// ResourceContext.
Image::CompressionOptions* ImageOptionsForLoadedResource(
    const ResourceContext& context, const RewriteOptions* options,
    const ResourcePtr& input_resource) {
  Image::CompressionOptions* image_options = new Image::CompressionOptions();
  image_options->webp_preferred = context.attempt_webp();
  image_options->jpeg_quality = options->image_jpeg_recompress_quality();
  image_options->progressive_jpeg =
      options->Enabled(RewriteOptions::kConvertJpegToProgressive) &&
      static_cast<int64>(input_resource->contents().size()) >=
          options->progressive_jpeg_min_bytes();
  image_options->convert_png_to_jpeg =
      options->Enabled(RewriteOptions::kConvertPngToJpeg);
  image_options->convert_gif_to_png =
      options->Enabled(RewriteOptions::kConvertGifToPng);
  image_options->recompress_jpeg =
      options->Enabled(RewriteOptions::kRecompressJpeg);
  image_options->recompress_png =
      options->Enabled(RewriteOptions::kRecompressPng);
  image_options->recompress_webp =
      options->Enabled(RewriteOptions::kRecompressWebp);
  image_options->retain_color_profile =
      !options->Enabled(RewriteOptions::kStripImageColorProfile);
  image_options->retain_exif_data =
      !options->Enabled(RewriteOptions::kStripImageMetaData);
  image_options->jpeg_num_progressive_scans =
      options->image_jpeg_num_progressive_scans();
  image_options->retain_color_sampling =
      !options->Enabled(RewriteOptions::kJpegSubsampling);
  return image_options;
}

// Resize image if necessary, returning true if this resizing succeeds and false
// if it's unnecessary or fails.
bool ResizeImageIfNecessary(
    const RewriteContext* rewrite_context, const GoogleString& url,
    RewriteDriver* driver, ResourceContext* context,
    Image* image, CachedResult* cached) {
  const RewriteOptions* options = driver->options();
  bool resized = false;
  // Begin by resizing the image if necessary
  ImageDim image_dim;
  image->Dimensions(&image_dim);
  // Here we are computing the size of the image as described by the html on the
  // page. If we succeed in doing so, that will be the desired image size.
  // Otherwise we may fill in desired_image_dims later based on actual image
  // size.
  ImageDim* desired_dim = context->mutable_desired_image_dims();
  const ImageDim* post_resize_dim = &image_dim;
  if (options->Enabled(RewriteOptions::kResizeImages) &&
      ImageUrlEncoder::HasValidDimension(*desired_dim) &&
      ImageUrlEncoder::HasValidDimensions(image_dim) &&
      (image->content_type()->type() != ContentType::kGif ||
       options->Enabled(RewriteOptions::kConvertGifToPng) ||
       options->NeedLowResImages())) {
    if (!desired_dim->has_width()) {
      // Fill in a missing page height
      // multiply page_height * (image_width / image_height),
      // but to avoid fractions we instead group as
      // (page_height * image_width) / image_height and do the
      // math in int64 to avoid overflow in the numerator.
      // Note that we can fiddle with rounding by adding factors
      // of image_height to the numerator before we divide.
      int64 page_height = desired_dim->height();
      int64 page_width =
          (page_height * image_dim.width()) / image_dim.height();
      desired_dim->set_width(static_cast<int32>(page_width));
    } else if (!desired_dim->has_height()) {
      // Fill in a missing page width
      // Math as above, swapping width and height.
      int64 page_width = desired_dim->width();
      int64 page_height =
          (page_width * image_dim.height()) / image_dim.width();
      desired_dim->set_height(static_cast<int32>(page_height));
    }
    int64 page_area =
        static_cast<int64>(desired_dim->width()) * desired_dim->height();
    int64 image_area =
        static_cast<int64>(image_dim.width()) * image_dim.height();
    if (page_area * 100 <
        image_area * options->image_limit_resize_area_percent()) {
      const char* message;  // Informational message for logging only.
      if (image->ResizeTo(*desired_dim)) {
        post_resize_dim = desired_dim;
        message = "Resized";
        resized = true;
      } else {
        message = "Couldn't resize";
      }
      driver->InfoAt(rewrite_context, "%s image `%s' from %dx%d to %dx%d",
                     message, url.c_str(),
                     image_dim.width(), image_dim.height(),
                     desired_dim->width(), desired_dim->height());
    }
  }

  // Cache image dimensions, including any resizing we did.
  // This happens regardless of whether we rewrite the image contents.
  if (ImageUrlEncoder::HasValidDimensions(*post_resize_dim)) {
    ImageDim* dims = cached->mutable_image_file_dims();
    dims->set_width(post_resize_dim->width());
    dims->set_height(post_resize_dim->height());
  }
  return resized;
}

}  // namespace

RewriteResult ImageRewriteFilter::RewriteLoadedResourceImpl(
      RewriteContext* rewrite_context, const ResourcePtr& input_resource,
      const OutputResourcePtr& result) {
  MessageHandler* message_handler = driver_->message_handler();
  StringVector urls;
  ResourceContext context;
  if (!encoder_.Decode(result->name(), &urls, &context, message_handler)) {
    return kRewriteFailed;
  }

  const RewriteOptions* options = driver_->options();

  Image::CompressionOptions* image_options =
      ImageOptionsForLoadedResource(context, options, input_resource);

  scoped_ptr<Image> image(
      NewImage(input_resource->contents(), input_resource->url(),
               resource_manager_->filename_prefix(), image_options,
               message_handler));

  Image::Type original_image_type = image->image_type();
  if (original_image_type == Image::IMAGE_UNKNOWN) {
    message_handler->Error(result->name().as_string().c_str(), 0,
                           "Unrecognized image content type.");
    image_rewrites_dropped_intentionally_->Add(1);
    return kRewriteFailed;
  }
  // We used to reject beacon images based on their size (1x1 or less) here, but
  // now rely on caching headers instead as this was missing a lot of padding
  // images that were ripe for inlining.
  RewriteResult rewrite_result = kTooBusy;
  if (work_bound_->TryToWork()) {
    rewrite_result = kRewriteFailed;
    CachedResult* cached = result->EnsureCachedResultCreated();
    bool resized =
        ResizeImageIfNecessary(rewrite_context, input_resource->url(),
                               driver_, &context, image.get(), cached);

    // Now re-compress the (possibly resized) image, and decide if it's
    // saved us anything.
    if ((resized || options->ImageOptimizationEnabled()) &&
        (image->output_size() * 100 <
         image->input_size() * options->image_limit_optimized_percent())) {
      // Here output image type could potentially be different from input type.
      const ContentType* output_type =
          ImageToContentType(input_resource->url(), image.get());

      // Consider inlining output image (no need to check input, it's bigger)
      // This needs to happen before Write to persist.
      SaveIfInlinable(image->Contents(), image->image_type(), cached);

      resource_manager_->MergeNonCachingResponseHeaders(input_resource, result);
      if (resource_manager_->Write(
              ResourceVector(1, input_resource), image->Contents(), output_type,
              StringPiece() /* no charset for images */, result.get(),
              message_handler)) {
        driver_->InfoAt(
            rewrite_context,
            "Shrinking image `%s' (%u bytes) to `%s' (%u bytes)",
            input_resource->url().c_str(),
            static_cast<unsigned>(image->input_size()),
            result->url().c_str(),
            static_cast<unsigned>(image->output_size()));

        // Update stats.
        image_rewrites_->Add(1);
        image_rewrite_total_bytes_saved_->Add(
            image->input_size() - image->output_size());
        image_rewrite_total_original_bytes_->Add(image->input_size());
        if (result->type()->type() == ContentType::kWebp) {
          image_webp_rewrites_->Add(1);
        }

        rewrite_result = kRewriteOk;
      }
    } else if (resized) {
      // Eliminate any image dimensions from a resize operation that succeeded,
      // but yielded overly-large output.
      driver_->InfoAt(
          rewrite_context,
          "Shrink of image `%s' doesn't save space; dropped.",
          input_resource->url().c_str());
      ImageDim* dims = cached->mutable_image_file_dims();
      dims->clear_width();
      dims->clear_height();
    }

    // Try inlining input image if output hasn't been inlined already.
    if (!cached->has_inlined_data()) {
      SaveIfInlinable(input_resource->contents(), original_image_type, cached);
    }

    int64 image_size = static_cast<int64>(image->output_size());
    if (options->NeedLowResImages() &&
        !cached->has_low_resolution_inlined_data() &&
        image_size >= options->min_image_size_low_resolution_bytes() &&
        image_size <= options->max_image_size_low_resolution_bytes()) {
      Image::CompressionOptions* image_options =
          new Image::CompressionOptions();
      image_options->webp_preferred = false;
      image_options->jpeg_quality = options->image_jpeg_recompress_quality();
      image_options->progressive_jpeg = false;
      image_options->convert_png_to_jpeg =
          options->Enabled(RewriteOptions::kConvertPngToJpeg);

      // Set to true since we optimize a gif to png before resize.
      image_options->convert_gif_to_png = true;
      image_options->recompress_jpeg = true;
      image_options->recompress_png = true;
      image_options->recompress_webp = true;

      // Since these are replaced with their high res versions, stripping
      // them off for low res images will further reduce bytes.
      image_options->retain_color_profile = false;
      image_options->retain_exif_data = false;
      image_options->retain_color_sampling = false;
      image_options->jpeg_num_progressive_scans =
          options->image_jpeg_num_progressive_scans();
      scoped_ptr<Image> low_image(
          NewImage(image->Contents(), input_resource->url(),
                   resource_manager_->filename_prefix(), image_options,
                   message_handler));
      low_image->SetTransformToLowRes();
      if (image->Contents().size() > low_image->Contents().size()) {
        // TODO(pulkitg): Add a some sort of guarantee on how small inline
        // images will be.
        if (context.mobile_user_agent()) {
          ResizeLowQualityImage(low_image.get(), input_resource, cached);
        } else {
          cached->set_low_resolution_inlined_data(low_image->Contents().data(),
                                                  low_image->Contents().size());
        }
        cached->set_low_resolution_inlined_image_type(
            static_cast<int>(low_image->image_type()));
      }
    }
    work_bound_->WorkComplete();
  } else {
    image_rewrites_dropped_due_to_load_->IncBy(1);
    message_handler->Message(kInfo, "%s: Too busy to rewrite image.",
                             input_resource->url().c_str());
  }

  // All other conditions were updated in other code paths above.
  if (rewrite_result == kRewriteFailed) {
    image_rewrites_dropped_intentionally_->Add(1);
  }

  return rewrite_result;
}

// Generate resized low quality image if the image width is not smaller than
// kDelayImageWidthForMobile. If image width is smaller than
// kDelayImageWidthForMobile, "delay_images" optimization is not very useful
// and no low quality image will be generated.
void ImageRewriteFilter::ResizeLowQualityImage(
    Image* low_image, const ResourcePtr& input_resource, CachedResult* cached) {
  ImageDim image_dim;
  low_image->Dimensions(&image_dim);
  if (image_dim.width() >= kDelayImageWidthForMobile) {
    const RewriteOptions* options = driver_->options();
    Image::CompressionOptions* image_options =
        new Image::CompressionOptions();
    // TODO(bolian): Use webp format for supported user agents.
    image_options->webp_preferred = false;
    image_options->jpeg_quality = options->image_jpeg_recompress_quality();
    image_options->progressive_jpeg = false;
    image_options->convert_png_to_jpeg =
        options->Enabled(RewriteOptions::kConvertPngToJpeg);
    image_options->convert_gif_to_png =
        options->Enabled(RewriteOptions::kConvertGifToPng);
    image_options->recompress_jpeg =
        options->Enabled(RewriteOptions::kRecompressJpeg);
    image_options->recompress_png =
        options->Enabled(RewriteOptions::kRecompressPng);
    image_options->recompress_webp =
        options->Enabled(RewriteOptions::kRecompressWebp);
    scoped_ptr<Image> image(
        NewImage(low_image->Contents(), input_resource->url(),
                 resource_manager_->filename_prefix(), image_options,
                 driver_->message_handler()));
    image->SetTransformToLowRes();
    ImageDim resized_dim;
    resized_dim.set_width(kDelayImageWidthForMobile);
    resized_dim.set_height((static_cast<int64>(resized_dim.width()) *
                            image_dim.height()) / image_dim.width());
    MessageHandler* message_handler = driver_->message_handler();
    bool resized = image->ResizeTo(resized_dim);
    StringPiece contents = image->Contents();
    StringPiece old_contents = low_image->Contents();
    if (resized && contents.size() < old_contents.size()) {
      cached->set_low_resolution_inlined_data(contents.data(), contents.size());
      message_handler->Message(
          kInfo,
          "Resized low quality image (%s) from "
          "%dx%d(%d bytes) to %dx%d(%d bytes)",
          input_resource->url().c_str(),
          image_dim.width(), image_dim.height(),
          static_cast<int>(old_contents.size()),
          resized_dim.width(), resized_dim.width(),
          static_cast<int>(contents.size()));
    } else {
      message_handler->Message(
          kError,
          "Couldn't resize low quality image (%s) or resized image file is "
          "not smaller: "
          "%dx%d(%d bytes) => %dx%d(%d bytes)",
          input_resource->url().c_str(),
          image_dim.width(), image_dim.height(),
          static_cast<int>(old_contents.size()),
          resized_dim.width(), resized_dim.height(),
          static_cast<int>(contents.size()));
    }
  }
}

void ImageRewriteFilter::SaveIfInlinable(const StringPiece& contents,
                                         const Image::Type image_type,
                                         CachedResult* cached) {
  // We retain inlining information if the image size is < the largest possible
  // inlining threshold, as an image might be used in both html and css and we
  // may see it first from the one with a smaller threshold. Note that this can
  // cause us to save inline information for an image that won't ever actually
  // be inlined (because it's too big to inline in html, say, and doesn't occur
  // in css).
  int64 image_inline_max_bytes =
      driver_->options()->MaxImageInlineMaxBytes();
  if (static_cast<int64>(contents.size()) < image_inline_max_bytes) {
    cached->set_inlined_data(contents.data(), contents.size());
    cached->set_inlined_image_type(static_cast<int>(image_type));
  }
}

// Convert (possibly NULL) Image* to corresponding (possibly NULL) ContentType*
const ContentType* ImageRewriteFilter::ImageToContentType(
    const GoogleString& origin_url, Image* image) {
  const ContentType* content_type = NULL;
  if (image != NULL) {
    // Even if we know the content type from the extension coming
    // in, the content-type can change as a result of compression,
    // e.g. gif to png, or jpeg to webp.
    return image->content_type();
  }
  return content_type;
}

void ImageRewriteFilter::BeginRewriteImageUrl(HtmlElement* element,
                                              HtmlElement::Attribute* src) {
  scoped_ptr<ResourceContext> resource_context(new ResourceContext);
  const RewriteOptions* options = driver_->options();

  if (options->Enabled(RewriteOptions::kResizeImages)) {
    GetDimensions(element, resource_context->mutable_desired_image_dims());
  }
  StringPiece url(src->DecodedValueOrNull());
  if (options->Enabled(RewriteOptions::kConvertJpegToWebp) &&
      driver_->UserAgentSupportsWebp() &&
      (options->Enabled(RewriteOptions::kConvertPngToJpeg) ||
       !(url.ends_with(".png") || url.ends_with(".gif")))) {
    // Note that we guess content type based on extension above. This avoids
    // the common case where we rewrite a .png twice, once for webp capable
    // browsers and once for non-webp browsers, even though neither rewrite uses
    // webp code paths at all. We only consider webp as a candidate image
    // format if we might have a jpg.
    // TODO(jmaessen): if we instead set up the ResourceContext mapping
    // explicitly from within the filter, we can imagine doing so after we know
    // the content type of the image. But that involves throwing away quite a
    // bit of the plumbing that is otherwise provided for us by
    // SingleRewriteContext.
    resource_context->set_attempt_webp(true);
  }
  if (options->NeedLowResImages() &&
      options->Enabled(RewriteOptions::kResizeMobileImages) &&
      driver_->IsMobileUserAgent()) {
    resource_context->set_mobile_user_agent(true);
  }

  ResourcePtr input_resource = CreateInputResource(src->DecodedValueOrNull());
  if (input_resource.get() != NULL) {
    // If the image will be inlined and the local storage cache is enabled, add
    // the LSC marker attribute to this element so that the LSC filter knows to
    // insert the relevant javascript functions.
    if (driver_->UserAgentSupportsImageInlining()) {
      LocalStorageCacheFilter::InlineState state;
      LocalStorageCacheFilter::AddStorableResource(src->DecodedValueOrNull(),
                                                   driver_,
                                                   true /* ignore cookie */,
                                                   element, &state);
    }
    Context* context = new Context(0 /* No CSS inlining, it's html */,
                                   this, driver_, NULL /*not nested */,
                                   resource_context.release(),
                                   false /*not css */, image_counter_++);
    ResourceSlotPtr slot(driver_->GetSlot(input_resource, element, src));
    context->AddSlot(slot);
    driver_->InitiateRewrite(context);
  }
}

bool ImageRewriteFilter::FinishRewriteCssImageUrl(
    int64 css_image_inline_max_bytes,
    const CachedResult* cached, ResourceSlot* slot) {
  GoogleString data_url;
  if (driver_->UserAgentSupportsImageInlining() &&
      TryInline(css_image_inline_max_bytes, cached, &data_url)) {
    // TODO(jmaessen): Can we make output URL reflect actual *usage*
    // of image inlining and/or webp images?
    slot->DirectSetUrl(data_url);
    image_inline_count_->Add(1);
    return true;
  } else if (cached->optimizable()) {
    image_rewrite_uses_->Add(1);
  }
  // Fall back to nested rewriting, which will also left trim the url if that
  // is required.
  return false;
}

bool ImageRewriteFilter::FinishRewriteImageUrl(
    const CachedResult* cached, const ResourceContext* resource_context,
    HtmlElement* element, HtmlElement::Attribute* src, int image_index) {
  const RewriteOptions* options = driver_->options();
  bool rewrote_url = false;
  bool image_inlined = false;
  GoogleString src_value(src->DecodedValueOrNull());
  if (src_value.empty()) {
    return false;
  }

  // See if we have a data URL, and if so use it if the browser can handle it
  // TODO(jmaessen): get rid of a string copy here. Tricky because ->SetValue()
  // copies implicitly.
  GoogleString data_url;
  if (driver_->UserAgentSupportsImageInlining() &&
      TryInline(driver_->options()->ImageInlineMaxBytes(),
                cached, &data_url)) {
    src->SetValue(data_url);
    // TODO(jmaessen): We used to take the absence of desired_image_dims here as
    // license to delete dimensions.  That was incorrect, as sometimes there
    // were dimensions in the page but the image was being enlarged on page and
    // we can't strip the enlargement out safely.  So right now, if the size
    // information exactly matches the size of the image, we'll inline the
    // image, but resource_context->has_desired_image_dims() will be false (so
    // that the rewritten image url doesn't contain the image dimensions) and
    // we'll leave the dimensions in place unnecessarily.
    if (cached->has_image_file_dims() &&
        resource_context->has_desired_image_dims() &&
        (cached->image_file_dims().width() ==
         resource_context->desired_image_dims().width()) &&
        (cached->image_file_dims().height() ==
         resource_context->desired_image_dims().height())) {
      // Delete dimensions, as they they match the given inline image data.
      element->DeleteAttribute(HtmlName::kWidth);
      element->DeleteAttribute(HtmlName::kHeight);
    }
    // Note the use of the ORIGINAL url not the data url.
    LocalStorageCacheFilter::AddLscAttributes(src_value, *cached,
                                              true /* has_url */,
                                              driver_, element);
    image_inline_count_->Add(1);
    rewrote_url = true;
    image_inlined = true;
  } else {
    // Not inlined means we cannot store it in local storage.
    LocalStorageCacheFilter::RemoveLscAttributes(element);
    if (cached->optimizable()) {
      // Rewritten HTTP url
      src->SetValue(cached->url());
      image_rewrite_uses_->Add(1);
      rewrote_url = true;
    }

    if (options->Enabled(RewriteOptions::kInsertImageDimensions) &&
        !HasAnyDimensions(element) &&
        cached->has_image_file_dims() &&
        ImageUrlEncoder::HasValidDimensions(cached->image_file_dims())) {
      // Add image dimensions. We don't bother if even a single image
      // dimension is already specified---even though we don't resize in that
      // case, either, because we might be off by a pixel in the other
      // dimension from the size chosen by the browser. We also don't bother
      // to resize if either dimension is specified with units (px, em, %)
      // rather than as absolute pixels. But note that we DO attempt to
      // include image dimensions even if we otherwise choose not to optimize
      // an image.
      const ImageDim& file_dims = cached->image_file_dims();
      driver_->AddAttribute(element, HtmlName::kWidth, file_dims.width());
      driver_->AddAttribute(element, HtmlName::kHeight, file_dims.height());
    }
  }

  if (driver_->UserAgentSupportsImageInlining() && !image_inlined &&
      options->NeedLowResImages() &&
      cached->has_low_resolution_inlined_data() &&
      IsCriticalImage(src_value)) {
    int max_preview_image_index =
        driver_->options()->max_inlined_preview_images_index();
    if (max_preview_image_index < 0 || image_index < max_preview_image_index) {
      int image_type = cached->low_resolution_inlined_image_type();
      bool valid_image_type = Image::kImageTypeStart <= image_type &&
          Image::kImageTypeEnd >= image_type;
      DCHECK(valid_image_type) << "Invalid Image Type: " << image_type;
      if (valid_image_type) {
        GoogleString data_url;
        DataUrl(*Image::TypeToContentType(static_cast<Image::Type>(image_type)),
                BASE64, cached->low_resolution_inlined_data(), &data_url);
        driver_->AddAttribute(element, HtmlName::kPagespeedLowResSrc, data_url);
        driver_->increment_num_inline_preview_images();
      } else {
        driver_->message_handler()->Message(kError,
                                            "Invalid low res image type: %d",
                                            image_type);
      }
    }
  }
  return rewrote_url;
}

bool ImageRewriteFilter::IsCriticalImage(const StringPiece& image_url) const {
  GoogleUrl image_gurl(driver_->base_url(), image_url);
  CriticalImagesFinder* finder =
      driver_->resource_manager()->critical_images_finder();
  bool is_image_critical = (finder == NULL) ||
      (finder->IsCriticalImage(image_gurl.spec_c_str(), driver_));
  return is_image_critical;
}

bool ImageRewriteFilter::HasAnyDimensions(HtmlElement* element) {
  if (element->FindAttribute(HtmlName::kWidth)) {
    return true;
  }
  if (element->FindAttribute(HtmlName::kHeight)) {
    return true;
  }
  css_util::StyleExtractor extractor(element);
  return extractor.HasAnyDimensions();
}

namespace {

// Skip ascii whitespace, returning pointer to first non-whitespace character in
// accordance with:
//   http://www.whatwg.org/specs/web-apps/current-work/multipage/
//                  common-microsyntaxes.html#space-character
const char* SkipAsciiWhitespace(const char* position) {
  while (*position <= ' ' &&  // Quickly skip if no leading whitespace
         (*position == ' ' || *position == '\x09' || *position == '\x0A' ||
          *position == '\x0C' || *position == '\x0D')) {
    ++position;
  }
  return position;
}

bool GetDimensionAttribute(
    const HtmlElement* element, HtmlName::Keyword name, int* value) {
  const HtmlElement::Attribute* attribute = element->FindAttribute(name);
  if (attribute == NULL) {
    return false;
  }
  const char* position = attribute->DecodedValueOrNull();
  return ImageRewriteFilter::ParseDimensionAttribute(position, value);
}

// If the element has a width attribute, set it in page_dim.
void SetWidthFromAttribute(const HtmlElement* element, ImageDim* page_dim) {
  int32 width;
  if (GetDimensionAttribute(element, HtmlName::kWidth, &width)) {
    page_dim->set_width(width);
  }
}

// If the element has a height attribute, set it in page_dim.
void SetHeightFromAttribute(const HtmlElement* element, ImageDim* page_dim) {
  int32 height;
  if (GetDimensionAttribute(element, HtmlName::kHeight, &height)) {
    page_dim->set_height(height);
  }
}

}  // namespace

bool ImageRewriteFilter::ParseDimensionAttribute(
    const char* position, int* value) {
  if (position == NULL) {
    return false;
  }
  // Note that we rely heavily on null-termination of char* here to cause our
  // control flow to fall through when we reach end of string.  Numbered steps
  // correspond to the steps in the spec.
  //   http://www.whatwg.org/specs/web-apps/current-work/multipage/
  //                  common-microsyntaxes.html#percentages-and-dimensions
  // 3) Skip ascii whitespace
  position = SkipAsciiWhitespace(position);
  // 5) Skip leading plus
  if (*position == '+') {
    ++position;
  }
  unsigned int result = 0;  // unsigned for consistent overflow behavior.
  // 6,7,9) Process digits
  while ('0' <= *position && *position <= '9') {
    unsigned int new_result = result * 10 + *position - '0';
    if (new_result < result) {
      // Integer overflow.  Reject.
      return false;
    }
    result = new_result;
    ++position;
  }
  // 6,7,8) Reject if no digits or only zeroes, or conversion to signed will
  // fail.
  if (result < 1 || INT_MAX < result) {
    return false;
  }
  // 11) Process fraction (including 45. with nothing after the . )
  if (*position == '.') {
    ++position;
    if ('5' <= *position && *position <= '9' && result < INT_MAX) {
      // Round based on leading fraction digit, avoiding overflow.
      ++result;
      ++position;
    }
    // Discard all fraction digits.
    while ('0' <= *position && *position <= '9') {
      ++position;
    }
  }
  // Skip whitespace before a possible trailing px.  The spec allows other junk,
  // or a trailing percent, but we can't resize percentages and older browsers
  // don't resize when they encounter junk.
  position = SkipAsciiWhitespace(position);
  if (position[0] == 'p' && position[1] == 'x') {
    position = SkipAsciiWhitespace(position + 2);
  }
  // Reject if there's trailing junk.
  if (*position != '\0') {
    return false;
  }
  // 14) return result as length.
  *value = static_cast<int>(result);
  return true;
}

void ImageRewriteFilter::GetDimensions(HtmlElement* element,
                                       ImageDim* page_dim) {
  css_util::StyleExtractor extractor(element);
  css_util::DimensionState state = extractor.state();
  int32 width = extractor.width();
  int32 height = extractor.height();
  // If we didn't get a height dimension above, but there is a height
  // value in the style attribute, that means there's a height value
  // we can't process. This height will trump the height attribute in the
  // image tag, so we need to avoid resizing.
  // The same is true of width.
  switch (state) {
    case css_util::kNotParsable:
      break;
    case css_util::kHasBothDimensions:
      page_dim->set_width(width);
      page_dim->set_height(height);
      break;
    case css_util::kHasHeightOnly:
      page_dim->set_height(height);
      SetWidthFromAttribute(element, page_dim);
      break;
    case css_util::kHasWidthOnly:
      page_dim->set_width(width);
      SetHeightFromAttribute(element, page_dim);
      break;
    case css_util::kNoDimensions:
      SetWidthFromAttribute(element, page_dim);
      SetHeightFromAttribute(element, page_dim);
      break;
    default:
      break;
  }
}

bool ImageRewriteFilter::TryInline(
    int64 image_inline_max_bytes, const CachedResult* cached_result,
    GoogleString* data_url) {
  if (!cached_result->has_inlined_data()) {
    return false;
  }
  StringPiece data = cached_result->inlined_data();
  if (static_cast<int64>(data.size()) >= image_inline_max_bytes) {
    return false;
  }
  DataUrl(
      *Image::TypeToContentType(
          static_cast<Image::Type>(cached_result->inlined_image_type())),
      BASE64, data, data_url);
  return true;
}

void ImageRewriteFilter::EndElementImpl(HtmlElement* element) {
  // Don't rewrite if ModPagespeedDisableForBots is on
  // and the user-agent is a bot.
  if (driver_->ShouldNotRewriteImages()) {
    return;
  }

  // Don't rewrite it the image is broken by a flush.
  if (driver_->HasChildrenInFlushWindow(element)) {
    return;
  }

  // Don't rewrite if we cannot find the src attribute.
  HtmlElement::Attribute *src = image_filter_->ParseImageElement(element);
  if (src == NULL || src->DecodedValueOrNull() == NULL) {
    return;
  }

  // Ask the LSC filter to work out how to handle this element. A return
  // value of true means we don't have to rewrite it so can skip that.
  // The state is carried forward to after we initiate rewriting since
  // we might still have to modify the element.
  LocalStorageCacheFilter::InlineState state;
  if (LocalStorageCacheFilter::AddStorableResource(src->DecodedValueOrNull(),
                                                   driver_,
                                                   false /* check cookie */,
                                                   element, &state)) {
    return;
  }

  BeginRewriteImageUrl(element, src);
}

const UrlSegmentEncoder* ImageRewriteFilter::encoder() const {
  return &encoder_;
}

RewriteContext* ImageRewriteFilter::MakeRewriteContext() {
  return new Context(0 /*No CSS inlining, it's html */,
                     this, driver_, NULL /*not nested */,
                     new ResourceContext(), false /*not css */,
                     kNotCriticalIndex);
}

RewriteContext* ImageRewriteFilter::MakeNestedRewriteContextForCss(
    int64 css_image_inline_max_bytes,
    RewriteContext* parent, const ResourceSlotPtr& slot) {
  Context* context = new Context(css_image_inline_max_bytes,
                                 this, NULL /* driver*/, parent,
                                 new ResourceContext, true /*is css */,
                                 kNotCriticalIndex);
  context->AddSlot(slot);
  return context;
}

RewriteContext* ImageRewriteFilter::MakeNestedRewriteContext(
    RewriteContext* parent, const ResourceSlotPtr& slot) {
  Context* context = new Context(0 /*No Css inling */, this, NULL /* driver*/,
                                 parent, new ResourceContext,
                                 false /*not css */, kNotCriticalIndex);
  context->AddSlot(slot);
  return context;
}

}  // namespace net_instaweb
