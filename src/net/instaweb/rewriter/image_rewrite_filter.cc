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

#include "base/logging.h"               // for CHECK, etc
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/css_resource_slot.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/rewriter/public/image_tag_scanner.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/data_url.h"
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
struct ContentType;

namespace {

// Rewritten image must be < kMaxRewrittenRatio * origSize to be worth
// redirecting references to it.
// TODO(jmaessen): Make this ratio adjustable.
const double kMaxRewrittenRatio = 1.0;

// Re-scale image if area / originalArea < kMaxAreaRatio
// Should probably be much less than 1 due to jpeg quality loss.
// Might need to differ depending upon image format.
// TODO(jmaessen): Make adjustable.
const double kMaxAreaRatio = 1.0;

// names for Statistics variables.
const char kImageRewrites[] = "image_rewrites";
const char kImageRewriteSavedBytes[] = "image_rewrite_saved_bytes";
const char kImageInline[] = "image_inline";
const char kImageWebpRewrites[] = "image_webp_rewrites";

}  // namespace

// name for statistic used to bound rewriting work.
const char ImageRewriteFilter::kImageOngoingRewrites[] =
    "image_ongoing_rewrites";

// Number of image rewrites we dropped lately due to work bound.
const char ImageRewriteFilter::kImageRewritesDroppedDueToLoad[] =
    "image_rewrites_dropped_due_to_load";

class ImageRewriteFilter::Context : public SingleRewriteContext {
 public:
  Context(int64 css_image_inline_max_bytes,
          ImageRewriteFilter* filter, RewriteDriver* driver,
          RewriteContext* parent, ResourceContext* resource_context,
          bool is_css)
      : SingleRewriteContext(driver, parent, resource_context),
        css_image_inline_max_bytes_(css_image_inline_max_bytes),
        filter_(filter),
        driver_(driver),
        is_css_(is_css) {}
  virtual ~Context() {}

  virtual void Render();
  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output);
  virtual const char* id() const { return filter_->id().c_str(); }
  virtual OutputResourceKind kind() const { return kRewrittenResource; }
  virtual const UrlSegmentEncoder* encoder() const;

 private:
  int64 css_image_inline_max_bytes_;
  ImageRewriteFilter* filter_;
  RewriteDriver* driver_;
  bool is_css_;
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
    CssResourceSlot* css_slot = static_cast<CssResourceSlot*>(resource_slot);
    rewrote_url = filter_->FinishRewriteCssImageUrl(css_image_inline_max_bytes_,
                                                    result, css_slot);
  } else {
    if (!has_parent()) {
      // We use manual rendering for HTML, as we have to consider whether to
      // inline, and may also pass in width and height attributes.
      HtmlResourceSlot* html_slot = static_cast<HtmlResourceSlot*>(
          resource_slot);
      rewrote_url = filter_->FinishRewriteImageUrl(
          result, resource_context(),
          html_slot->element(), html_slot->attribute());
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

ImageRewriteFilter::ImageRewriteFilter(RewriteDriver* driver,
                                       StringPiece path_prefix)
    : RewriteSingleResourceFilter(driver, path_prefix),
      image_filter_(new ImageTagScanner(driver)),
      rewrite_count_(NULL),
      inline_count_(NULL),
      rewrite_saved_bytes_(NULL),
      webp_count_(NULL) {
  Statistics* stats = resource_manager_->statistics();
  Variable* ongoing_rewrites = NULL;
  if (stats != NULL) {
    rewrite_count_ = stats->GetVariable(kImageRewrites);
    rewrite_saved_bytes_ = stats->GetVariable(
        kImageRewriteSavedBytes);
    inline_count_ = stats->GetVariable(kImageInline);
    ongoing_rewrites = stats->GetVariable(kImageOngoingRewrites);
    webp_count_ = stats->GetVariable(kImageWebpRewrites);
    image_rewrites_dropped_ =
        stats->GetTimedVariable(kImageRewritesDroppedDueToLoad);
  }
  work_bound_.reset(
      new StatisticsWorkBound(ongoing_rewrites,
                              driver->options()->image_max_rewrites_at_once()));
}

ImageRewriteFilter::~ImageRewriteFilter() {}

void ImageRewriteFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kImageInline);
  statistics->AddVariable(kImageRewriteSavedBytes);
  statistics->AddVariable(kImageRewrites);
  statistics->AddVariable(kImageOngoingRewrites);
  statistics->AddVariable(kImageWebpRewrites);
  statistics->AddTimedVariable(kImageRewritesDroppedDueToLoad,
                               ResourceManager::kStatisticsGroup);
}

RewriteSingleResourceFilter::RewriteResult
ImageRewriteFilter::RewriteLoadedResource(const ResourcePtr& input_resource,
                                          const OutputResourcePtr& result) {
  return RewriteLoadedResourceImpl(NULL /* no rewrite_context*/,
                                   input_resource, result);
}

RewriteSingleResourceFilter::RewriteResult
ImageRewriteFilter::RewriteLoadedResourceImpl(
      RewriteContext* rewrite_context, const ResourcePtr& input_resource,
      const OutputResourcePtr& result) {
  MessageHandler* message_handler = driver_->message_handler();
  StringVector urls;
  ResourceContext context;
  if (!encoder_.Decode(result->name(), &urls, &context, message_handler)) {
    return kRewriteFailed;
  }

  const RewriteOptions* options = driver_->options();
  scoped_ptr<Image> image(
      NewImage(input_resource->contents(), input_resource->url(),
               resource_manager_->filename_prefix(),
               context.attempt_webp(), options->image_jpeg_recompress_quality(),
               message_handler));
  Image::Type original_image_type = image->image_type();
  if (original_image_type == Image::IMAGE_UNKNOWN) {
    message_handler->Error(result->name().as_string().c_str(), 0,
                           "Unrecognized image content type.");
    return kRewriteFailed;
  }
  // We used to reject beacon images based on their size (1x1 or less) here, but
  // now rely on caching headers instead as this was missing a lot of padding
  // images that were ripe for inlining.

  RewriteResult rewrite_result = kTooBusy;
  if (work_bound_->TryToWork()) {
    rewrite_result = kRewriteFailed;
    bool resized = false;
    // Begin by resizing the image if necessary
    ImageDim image_dim;
    image->Dimensions(&image_dim);
    const ImageDim& page_dim = context.image_tag_dims();
    const ImageDim* post_resize_dim = &image_dim;
    if (options->Enabled(RewriteOptions::kResizeImages) &&
        ImageUrlEncoder::HasValidDimensions(page_dim) &&
        ImageUrlEncoder::HasValidDimensions(image_dim)) {
      int64 page_area =
          static_cast<int64>(page_dim.width()) * page_dim.height();
      int64 image_area =
          static_cast<int64>(image_dim.width()) * image_dim.height();
      if (page_area < image_area * kMaxAreaRatio) {
        const char* message;  // Informational message for logging only.
        if (image->ResizeTo(page_dim)) {
          post_resize_dim = &page_dim;
          message = "Resized";
          resized = true;
        } else {
          message = "Couldn't resize";
        }
        driver_->InfoAt(rewrite_context, "%s image `%s' from %dx%d to %dx%d",
                        message, input_resource->url().c_str(),
                        image_dim.width(), image_dim.height(),
                        page_dim.width(), page_dim.height());
      }
    }

    // Cache image dimensions, including any resizing we did.
    // This happens regardless of whether we rewrite the image contents.
    CachedResult* cached = result->EnsureCachedResultCreated();
    if (ImageUrlEncoder::HasValidDimensions(*post_resize_dim)) {
      ImageDim* dims = cached->mutable_image_file_dims();
      dims->set_width(post_resize_dim->width());
      dims->set_height(post_resize_dim->height());
    }

    // Now re-compress the (possibly resized) image, and decide if it's
    // saved us anything.
    if ((resized || options->Enabled(RewriteOptions::kRecompressImages)) &&
        (image->output_size() < image->input_size() * kMaxRewrittenRatio)) {
      // here output image type could potentially be different from input type.
      result->SetType(ImageToContentType(input_resource->url(), image.get()));

      // Consider inlining output image (no need to check input, it's bigger)
      // This needs to happen before Write to persist.
      SaveIfInlinable(image->Contents(), image->image_type(), cached);

      int64 origin_expire_time_ms = input_resource->CacheExpirationTimeMs();
      resource_manager_->MergeNonCachingResponseHeaders(input_resource, result);
      if (resource_manager_->Write(
              HttpStatus::kOK, image->Contents(), result.get(),
              origin_expire_time_ms, message_handler)) {
        driver_->InfoAt(
            rewrite_context,
            "Shrinking image `%s' (%u bytes) to `%s' (%u bytes)",
            input_resource->url().c_str(),
            static_cast<unsigned>(image->input_size()),
            result->url().c_str(),
            static_cast<unsigned>(image->output_size()));

        if (rewrite_saved_bytes_ != NULL) {
          // Note: if we are serving a request from a different server
          // than the server that rewrote the <img> tag, and they don't
          // share a file system, then we will be bumping the byte-count
          // here without bumping the rewrite count.  This seems ok,
          // though perhaps we may need to revisit.
          //
          // Currently this will be a problem even when serving on a
          // different file that *does* share a filesystem,
          // HashResourceManager does not yet load its internal map
          // by scanning the filesystem on startup.
          rewrite_saved_bytes_->Add(
              image->input_size() - image->output_size());
        }
        rewrite_result = kRewriteOk;
      }
    }

    // Try inlining input image if output hasn't been inlined already.
    if (!cached->has_inlined_data()) {
      SaveIfInlinable(input_resource->contents(), original_image_type, cached);
    }
    work_bound_->WorkComplete();
  } else {
    image_rewrites_dropped_->IncBy(1);
    message_handler->Message(kInfo, "%s: Too busy to rewrite image.",
                             input_resource->url().c_str());
  }
  return rewrite_result;
}

int ImageRewriteFilter::FilterCacheFormatVersion() const {
  return 1;
}

bool ImageRewriteFilter::ReuseByContentHash() const {
  return true;
}

void ImageRewriteFilter::SaveIfInlinable(const StringPiece& contents,
                                         const Image::Type image_type,
                                         CachedResult* cached) {
  // We retain inlining information if the image size is >= the largest possible
  // inlining threshold, as an image might be used in both html and css and we
  // may see it first from the one with a smaller threshold.  Note that this can
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
  int width, height;
  const RewriteOptions* options = driver_->options();

  if (options->Enabled(RewriteOptions::kResizeImages) &&
      GetDimensions(element, &width, &height)) {
    // Specific image size is called for.  Rewrite to that size.
    ImageDim* page_dim = resource_context->mutable_image_tag_dims();
    page_dim->set_width(width);
    page_dim->set_height(height);
  }
  StringPiece url(src->value());
  if (options->Enabled(RewriteOptions::kConvertJpegToWebp) &&
      driver_->UserAgentSupportsWebp() &&
      !url.ends_with(".png") && !url.ends_with(".gif")) {
    // Note that we guess content type based on extension above.  This avoids
    // the common case where we rewrite a .png twice, once for webp capable
    // browsers and once for non-webp browsers, even though neither rewrite uses
    // webp code paths at all.  We only consider webp as a candidate image
    // format if we might have a jpg.
    // TODO(jmaessen): if we instead set up the ResourceContext mapping
    // explicitly from within the filter, we can imagine doing so after we know
    // the content type of the image.  But that involves throwing away quite a
    // bit of the plumbing that is otherwise provided for us by
    // SingleRewriteContext.
    resource_context->set_attempt_webp(true);
  }

  if (HasAsyncFlow()) {
    ResourcePtr input_resource = CreateInputResource(src->value());
    if (input_resource.get() != NULL) {
      Context* context = new Context(0 /* No CSS inlining, it's html */,
                                     this, driver_, NULL /*not nested */,
                                     resource_context.release(),
                                     false /*not css */);
      ResourceSlotPtr slot(driver_->GetSlot(input_resource, element, src));
      context->AddSlot(slot);
      driver_->InitiateRewrite(context);
    }
  } else {
    scoped_ptr<CachedResult> cached(RewriteWithCaching(src->value(),
                                                       resource_context.get()));
    if (cached.get() != NULL) {
      FinishRewriteImageUrl(cached.get(), resource_context.get(), element, src);
    }
  }
}

bool ImageRewriteFilter::FinishRewriteCssImageUrl(
    int64 css_image_inline_max_bytes,
    const CachedResult* cached, CssResourceSlot* slot) {
  GoogleString data_url;
  if (driver_->UserAgentSupportsImageInlining() &&
      TryInline(css_image_inline_max_bytes, cached, &data_url)) {
    // TODO(jmaessen): UNSAFE.  We don't differentiate whether the user-agent
    // supports image inlining when producing the CSS file, so this cached CSS
    // file will get served for all subsequent requests, even for
    // non-inline-capable browsers.
    slot->UpdateUrlInCss(data_url);
    inline_count_->Add(1);
    return true;
  } else if (cached->optimizable()) {
    rewrite_count_->Add(1);
  }
  // Fall back to nested rewriting, which will also left trim the url if that
  // is required.
  return false;
}

bool ImageRewriteFilter::FinishRewriteImageUrl(
    const CachedResult* cached, const ResourceContext* resource_context,
    HtmlElement* element, HtmlElement::Attribute* src) {
  const RewriteOptions* options = driver_->options();
  bool rewrote_url = false;

  // See if we have a data URL, and if so use it if the browser can handle it
  // TODO(jmaessen): get rid of a string copy here.  Tricky because ->SetValue()
  // copies implicitly.
  GoogleString data_url;
  if (driver_->UserAgentSupportsImageInlining() &&
      TryInline(driver_->options()->ImageInlineMaxBytes(),
                cached, &data_url)) {
    src->SetValue(data_url);
    if (cached->has_image_file_dims() &&
        (!resource_context->has_image_tag_dims() ||
         ((cached->image_file_dims().width() ==
           resource_context->image_tag_dims().width()) &&
          (cached->image_file_dims().height() ==
           resource_context->image_tag_dims().height())))) {
      // Delete dimensions, as they they match the given inline image data.
      element->DeleteAttribute(HtmlName::kWidth);
      element->DeleteAttribute(HtmlName::kHeight);
    }
    inline_count_->Add(1);
    rewrote_url = true;
  } else {
    if (cached->optimizable()) {
      // Rewritten HTTP url
      src->SetValue(cached->url());
      rewrite_count_->Add(1);
      rewrote_url = true;
    }

    if (options->Enabled(RewriteOptions::kInsertImageDimensions) &&
        !HasAnyDimensions(element) &&
        cached->has_image_file_dims() &&
        ImageUrlEncoder::HasValidDimensions(cached->image_file_dims())) {
      // Add image dimensions.  We don't bother if even a single image
      // dimension is already specified---even though we don't resize in that
      // case, either, because we might be off by a pixel in the other
      // dimension from the size chosen by the browser.  We also don't bother
      // to resize if either dimension is specified with units (px, em, %)
      // rather than as absolute pixels.  But note that we DO attempt to
      // include image dimensions even if we otherwise choose not to optimize
      // an image.
      const ImageDim& file_dims = cached->image_file_dims();
      driver_->AddAttribute(element, HtmlName::kWidth, file_dims.width());
      driver_->AddAttribute(element, HtmlName::kHeight, file_dims.height());
    }
  }

  return rewrote_url;
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

bool ImageRewriteFilter::GetDimensions(HtmlElement* element,
                                       int* width, int* height) {
  css_util::StyleExtractor extractor(element);
  css_util::DimensionState state = extractor.state();
  *width = extractor.width();
  *height = extractor.height();
  // If we didn't get a height dimension above, but there is a height
  // value in the style attribute, that means there's a height value
  // we can't process.  This height will tromp the height attribute in the
  // image tag, so we need to avoid resizing.
  // The same is true of width.
  switch (state) {
    case css_util::kHasBothDimensions:
      return true;
    case css_util::kNotParsable:
      return false;
    case css_util::kHasHeightOnly:
      return element->IntAttributeValue(HtmlName::kWidth, width);
    case css_util::kHasWidthOnly:
      return element->IntAttributeValue(HtmlName::kHeight, height);
    case css_util::kNoDimensions:
      return (element->IntAttributeValue(HtmlName::kWidth, width) &&
              element->IntAttributeValue(HtmlName::kHeight, height));
    default:
      return false;
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
  if (!driver_->HasChildrenInFlushWindow(element)) {
    HtmlElement::Attribute *src = image_filter_->ParseImageElement(element);
    if (src != NULL) {
      BeginRewriteImageUrl(element, src);
    }
  }
}

const UrlSegmentEncoder* ImageRewriteFilter::encoder() const {
  return &encoder_;
}

bool ImageRewriteFilter::HasAsyncFlow() const {
  return driver_->asynchronous_rewrites();
}

RewriteContext* ImageRewriteFilter::MakeRewriteContext() {
  return new Context(0 /*No CSS inlining, it's html */,
                     this, driver_, NULL /*not nested */,
                     new ResourceContext(), false /*not css */);
}

RewriteContext* ImageRewriteFilter::MakeNestedRewriteContextForCss(
    int64 css_image_inline_max_bytes,
    RewriteContext* parent, const ResourceSlotPtr& slot) {
  Context* context = new Context(css_image_inline_max_bytes,
                                 this, NULL /* driver*/, parent,
                                 new ResourceContext, true /*is css */);
  context->AddSlot(slot);
  return context;
}

RewriteContext* ImageRewriteFilter::MakeNestedRewriteContext(
    RewriteContext* parent, const ResourceSlotPtr& slot) {
  Context* context = new Context(0 /*No Css inling */, this, NULL /* driver*/,
                                 parent, new ResourceContext,
                                 false /*not css */);
  context->AddSlot(slot);
  return context;
}

}  // namespace net_instaweb
