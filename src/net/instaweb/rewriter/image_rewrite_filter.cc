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

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/rewriter/public/image_tag_scanner.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
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
class UrlSegmentEncoder;

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

// name for statistic used to bound rewriting work.
const char kImageOngoingRewrites[] = "image_ongoing_rewrites";

const char kWidthKey[]  = "ImageRewriteFilter_W";
const char kHeightKey[] = "ImageRewriteFilter_H";
const char kDataUrlKey[] = "ImageRewriteFilter_DataUrl";

}  // namespace

ImageRewriteFilter::ImageRewriteFilter(RewriteDriver* driver,
                                       StringPiece path_prefix)
    : RewriteSingleResourceFilter(driver, path_prefix),
      image_filter_(new ImageTagScanner(driver)),
      rewrite_count_(NULL),
      inline_count_(NULL),
      rewrite_saved_bytes_(NULL) {
  Statistics* stats = resource_manager_->statistics();
  Variable* ongoing_rewrites = NULL;
  if (stats != NULL) {
    rewrite_count_ = stats->GetVariable(kImageRewrites);
    rewrite_saved_bytes_ = stats->GetVariable(
        kImageRewriteSavedBytes);
    inline_count_ = stats->GetVariable(kImageInline);
    ongoing_rewrites = stats->GetVariable(kImageOngoingRewrites);
  }
  work_bound_.reset(
      new StatisticsWorkBound(ongoing_rewrites,
                              driver->options()->image_max_rewrites_at_once()));
}

void ImageRewriteFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kImageInline);
  statistics->AddVariable(kImageRewriteSavedBytes);
  statistics->AddVariable(kImageRewrites);
  statistics->AddVariable(kImageOngoingRewrites);
}

RewriteSingleResourceFilter::RewriteResult
ImageRewriteFilter::RewriteLoadedResource(const ResourcePtr& input_resource,
                                          const OutputResourcePtr& result) {
  MessageHandler* message_handler = driver_->message_handler();
  GoogleString url;
  ImageDim page_dim;
  if (!encoder_.DecodeUrlAndDimensions(result->name(), &page_dim, &url,
                                       message_handler)) {
    return kRewriteFailed;
  }
  scoped_ptr<Image> image(
      NewImage(input_resource->contents(), input_resource->url(),
               resource_manager_->filename_prefix(), message_handler));
  if (image->image_type() == Image::IMAGE_UNKNOWN) {
    message_handler->Error(result->name().as_string().c_str(), 0,
                           "Unrecognized image content type.");
    return kRewriteFailed;
  }
  ImageDim image_dim, post_resize_dim;
  image->Dimensions(&image_dim);
  post_resize_dim = image_dim;

  // Don't rewrite beacons
  if (!ImageUrlEncoder::HasValidDimensions(image_dim) ||
      (image_dim.width() <= 1 && image_dim.height() <= 1)) {
    return kRewriteFailed;
  }

  RewriteResult rewrite_result = kTooBusy;
  if (work_bound_->TryToWork()) {
    rewrite_result = kRewriteFailed;
    bool resized = false;
    const RewriteOptions* options = driver_->options();
    // Begin by resizing the image if necessary
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
          post_resize_dim = page_dim;
          message = "Resized";
          resized = true;
        } else {
          message = "Couldn't resize";
        }
        driver_->InfoHere("%s image `%s' from %dx%d to %dx%d", message,
                          input_resource->url().c_str(),
                          image_dim.width(), image_dim.height(),
                          page_dim.width(), page_dim.height());
      }
    }

    // Cache image dimensions, including any resizing we did.
    // This happens regardless of whether we rewrite the image contents.
    CachedResult* cached = result->EnsureCachedResultCreated();
    if (ImageUrlEncoder::HasValidDimensions(post_resize_dim)) {
      ImageDim* dims = cached->mutable_image_file_dims();
      dims->set_width(post_resize_dim.width());
      dims->set_height(post_resize_dim.height());
    }

    // We will consider whether to inline the image regardless of whether we
    // optimize it or not, so set up the necessary state for that.
    GoogleString inlined_url;
    int64 image_inline_max_bytes = options->image_inline_max_bytes();

    // Now re-compress the (possibly resized) image, and decide if it's
    // saved us anything.
    if ((resized || options->Enabled(RewriteOptions::kRecompressImages)) &&
        (image->output_size() < image->input_size() * kMaxRewrittenRatio)) {
      // here output image type could potentially be different from input type.
      result->SetType(ImageToContentType(input_resource->url(), image.get()));

      // Consider inlining output image (no need to check input, it's bigger)
      // This needs to happen before Write to persist.
      if (options->Enabled(RewriteOptions::kInlineImages) &&
          CanInline(image_inline_max_bytes, image->Contents(),
                    result->type(), &inlined_url)) {
        cached->set_image_inlined_uri(inlined_url);
      }

      int64 origin_expire_time_ms = input_resource->CacheExpirationTimeMs();
      if (resource_manager_->Write(
              HttpStatus::kOK, image->Contents(), result.get(),
              origin_expire_time_ms, message_handler)) {
        driver_->InfoHere(
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
    if (inlined_url.empty() &&
        options->Enabled(RewriteOptions::kInlineImages) &&
        CanInline(image_inline_max_bytes, input_resource->contents(),
                  input_resource->type(), &inlined_url)) {
      cached->set_image_inlined_uri(inlined_url);
    }
    work_bound_->WorkComplete();
  } else {
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

// Convert (possibly NULL) Image* to corresponding (possibly NULL) ContentType*
const ContentType* ImageRewriteFilter::ImageToContentType(
    const GoogleString& origin_url, Image* image) {
  const ContentType* content_type = NULL;
  if (image != NULL) {
    // Even if we know the content type from the extension coming
    // in, the content-type can change as a result of compression,
    // e.g. gif to png, or anything to vp8.
    switch (image->image_type()) {
      case Image::IMAGE_JPEG:
        content_type = &kContentTypeJpeg;
        break;
      case Image::IMAGE_PNG:
        content_type = &kContentTypePng;
        break;
      case Image::IMAGE_GIF:
        content_type = &kContentTypeGif;
        break;
      default:
        driver_->InfoHere(
            "Cannot detect content type of image url `%s`",
            origin_url.c_str());
        break;
    }
  }
  return content_type;
}

void ImageRewriteFilter::RewriteImageUrl(HtmlElement* element,
                                         HtmlElement::Attribute* src) {
  ResourceContext resource_context;
  ImageDim* page_dim = resource_context.mutable_image_tag_dims();
  int width, height;
  const RewriteOptions* options = driver_->options();

  if (options->Enabled(RewriteOptions::kResizeImages) &&
      element->IntAttributeValue(HtmlName::kWidth, &width) &&
      element->IntAttributeValue(HtmlName::kHeight, &height)) {
    // Specific image size is called for.  Rewrite to that size.
    page_dim->set_width(width);
    page_dim->set_height(height);
  }

  scoped_ptr<CachedResult> cached(RewriteWithCaching(src->value(),
                                                     &resource_context));
  if (cached.get() == NULL) {
    return;
  }

  // See if we have a data URL, and if so use it if the browser can handle it
  if (driver_->UserAgentSupportsImageInlining() &&
      cached->has_image_inlined_uri()) {
    src->SetValue(cached->image_inlined_uri());
    // Delete dimensions, as they ought to be redundant given inline image data.
    element->DeleteAttribute(HtmlName::kWidth);
    element->DeleteAttribute(HtmlName::kHeight);
    inline_count_->Add(1);
  } else {
    if (cached->optimizable()) {
      // Rewritten HTTP url
      src->SetValue(cached->url());
      rewrite_count_->Add(1);
    }

    if (options->Enabled(RewriteOptions::kInsertImageDimensions) &&
        !element->FindAttribute(HtmlName::kWidth) &&
        !element->FindAttribute(HtmlName::kHeight) &&
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
}

bool ImageRewriteFilter::CanInline(
    int image_inline_max_bytes, const StringPiece& contents,
    const ContentType* content_type, GoogleString* data_url) {
  bool ok = false;
  if (content_type != NULL &&
      static_cast<int>(contents.size()) <= image_inline_max_bytes) {
    DataUrl(*content_type, BASE64, contents, data_url);
    ok = true;
  }
  return ok;
}

void ImageRewriteFilter::EndElementImpl(HtmlElement* element) {
  // Don't rewrite if ModPagespeedDisableForBots is on
  // and the user-agent is a bot.
  if (driver_->RewriteImages()) {
    return;
  }
  if (!driver_->HasChildrenInFlushWindow(element)) {
    HtmlElement::Attribute *src = image_filter_->ParseImageElement(element);
    if (src != NULL) {
      RewriteImageUrl(element, src);
    }
  }
}

const UrlSegmentEncoder* ImageRewriteFilter::encoder() const {
  return &encoder_;
}

}  // namespace net_instaweb
