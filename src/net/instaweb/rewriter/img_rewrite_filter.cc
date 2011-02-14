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

#include "net/instaweb/rewriter/public/img_rewrite_filter.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/rewriter/public/img_tag_scanner.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/atom.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/file_system.h"
#include <string>
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/statistics_work_bound.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace net_instaweb {

namespace {

// Rewritten image must be < kMaxRewrittenRatio * origSize to be worth
// redirecting references to it.
// TODO(jmaessen): Make this ratio adjustable.
const double kMaxRewrittenRatio = 1.0;

// Re-scale image if area / originalArea < kMaxAreaRatio
// Should probably be much less than 1 due to jpeg quality loss.
// Might need to differ depending upon img format.
// TODO(jmaessen): Make adjustable.
const double kMaxAreaRatio = 1.0;

// names for Statistics variables.
const char kImageRewrites[] = "image_rewrites";
const char kImageRewriteSavedBytes[] = "image_rewrite_saved_bytes";
const char kImageInline[] = "image_inline";

// name for statistic used to bound rewriting work.
const char kImageOngoingRewrites[] = "image_ongoing_rewrites";

const char kWidthKey[]  = "ImgRewriteFilter_W";
const char kHeightKey[] = "ImgRewriteFilter_H";
const char kDataUrlKey[] = "ImgRewriteFilter_DataUrl";

}  // namespace


ImageUrlEncoder::ImageUrlEncoder(UrlEscaper* url_escaper)
    : url_escaper_(url_escaper) { }

ImageUrlEncoder::~ImageUrlEncoder() { }

void ImageUrlEncoder::EncodeToUrlSegment(
    const StringPiece& origin_url, std::string* rewritten_url) {
  stored_dim_.EncodeTo(rewritten_url);
  url_escaper_->EncodeToUrlSegment(origin_url, rewritten_url);
}

bool ImageUrlEncoder::DecodeFromUrlSegment(
    const StringPiece& rewritten_url, std::string* origin_url) {
  // Note that "remaining" is shortened from the left as we parse.
  StringPiece remaining(rewritten_url.data(), rewritten_url.size());
  return (stored_dim_.DecodeFrom(&remaining) &&
          url_escaper_->DecodeFromUrlSegment(remaining, origin_url));
}

ImgRewriteFilter::ImgRewriteFilter(RewriteDriver* driver,
                                   bool log_image_elements,
                                   bool insert_image_dimensions,
                                   StringPiece path_prefix,
                                   size_t img_inline_max_bytes,
                                   size_t img_max_rewrites_at_once)
    : RewriteSingleResourceFilter(driver, path_prefix),
      img_filter_(new ImgTagScanner(html_parse_)),
      img_inline_max_bytes_(img_inline_max_bytes),
      log_image_elements_(log_image_elements),
      insert_image_dimensions_(insert_image_dimensions),
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
      new StatisticsWorkBound(ongoing_rewrites, img_max_rewrites_at_once));
}

void ImgRewriteFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kImageInline);
  statistics->AddVariable(kImageRewriteSavedBytes);
  statistics->AddVariable(kImageRewrites);

  statistics->AddVariable(kImageOngoingRewrites);
}

UrlSegmentEncoder* ImgRewriteFilter::CreateCustomUrlEncoder() const {
  return new ImageUrlEncoder(resource_manager_->url_escaper());
}

RewriteSingleResourceFilter::RewriteResult
ImgRewriteFilter::RewriteLoadedResource(const Resource* input_resource,
                                        OutputResource* result,
                                        UrlSegmentEncoder* raw_encoder) {
  ImageUrlEncoder* encoder = static_cast<ImageUrlEncoder*>(raw_encoder);
  MessageHandler* message_handler = html_parse_->message_handler();

  ImageDim page_dim = encoder->stored_dim();
  scoped_ptr<Image> image(
      new Image(input_resource->contents(), input_resource->url(),
                resource_manager_->filename_prefix(), message_handler));

  if (image->image_type() == Image::IMAGE_UNKNOWN) {
    message_handler->Error(result->name().as_string().c_str(), 0,
                           "Unrecognized image content type.");
    return kRewriteFailed;
  }

  ImageDim img_dim, post_resize_dim;
  image->Dimensions(&img_dim);
  post_resize_dim = img_dim;

  // Don't rewrite beacons
  if (img_dim.width() <= 1 && img_dim.height() <= 1) {
    return kRewriteFailed;
  }

  RewriteResult rewrite_result = kTooBusy;
  if (work_bound_->TryToWork()) {
    rewrite_result = kRewriteFailed;

    const char* message;  // Informational message for logging only.
    if (page_dim.valid() && img_dim.valid()) {
      int64 page_area =
          static_cast<int64>(page_dim.width()) * page_dim.height();
      int64 img_area = static_cast<int64>(img_dim.width()) * img_dim.height();
      if (page_area < img_area * kMaxAreaRatio) {
        if (image->ResizeTo(page_dim)) {
          post_resize_dim = page_dim;
          message = "Resized image";
        } else {
          message = "Couldn't resize image";
        }
      } else {
        message = "Not worth resizing image";
      }
      html_parse_->InfoHere("%s `%s' from %dx%d to %dx%d", message,
                            input_resource->url().c_str(),
                            img_dim.width(), img_dim.height(),
                            page_dim.width(), page_dim.height());
    }

    // Cache image dimensions, including any resizing we did
    OutputResource::CachedResult* cached = result->EnsureCachedResultCreated();
    if (post_resize_dim.valid()) {
      cached->SetRememberedInt(kWidthKey, post_resize_dim.width());
      cached->SetRememberedInt(kHeightKey, post_resize_dim.height());
    }

    std::string inlined_url;
    if (image->output_size() <
        image->input_size() * kMaxRewrittenRatio) {
      // here output image type could potentially be different from input type.
      result->SetType(ImageToContentType(input_resource->url(), image.get()));

      // Consider inlining output image (no need to check input, it's bigger)
      // This needs to happen before Write to persist.
      if (CanInline(img_inline_max_bytes_, image->Contents(),
                    result->type(), &inlined_url)) {
        cached->SetRemembered(kDataUrlKey, inlined_url);
      }

      int64 origin_expire_time_ms = input_resource->CacheExpirationTimeMs();
      if (resource_manager_->Write(
              HttpStatus::kOK, image->Contents(), result,
              origin_expire_time_ms, message_handler)) {
        html_parse_->InfoHere(
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
        CanInline(img_inline_max_bytes_, input_resource->contents(),
                  input_resource->type(), &inlined_url)) {
      cached->SetRemembered(kDataUrlKey, inlined_url);
    }
    work_bound_->WorkComplete();
  }
  return rewrite_result;
}

// Convert (possibly NULL) Image* to corresponding (possibly NULL) ContentType*
const ContentType* ImgRewriteFilter::ImageToContentType(
    const std::string& origin_url, Image* image) {
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
        html_parse_->InfoHere(
            "Cannot detect content type of image url `%s`",
            origin_url.c_str());
        break;
    }
  }
  return content_type;
}

void ImgRewriteFilter::RewriteImageUrl(HtmlElement* element,
                                       HtmlElement::Attribute* src) {
  ImageDim page_dim;
  int width, height;
  if (element->IntAttributeValue(HtmlName::kWidth, &width) &&
      element->IntAttributeValue(HtmlName::kHeight, &height)) {
    // Specific image size is called for.  Rewrite to that size.
    page_dim.set_dims(width, height);
  }

  ImageUrlEncoder encoder(resource_manager_->url_escaper());
  encoder.set_stored_dim(page_dim);
  scoped_ptr<OutputResource::CachedResult> cached(RewriteWithCaching(
                                                      src->value(), &encoder));
  if (cached.get() == NULL) {
    return;
  }

  // See if we have a data URL, and if so use it if the browser can handle it
  std::string inlined_url;
  bool ie6or7 = driver_->user_agent().IsIe6or7();
  if (!ie6or7 && cached->Remembered(kDataUrlKey, &inlined_url)) {
    src->SetValue(inlined_url);
    if (inline_count_ != NULL) {
      inline_count_->Add(1);
    }
  } else {
    if (cached->optimizable()) {
      // Rewritten HTTP url
      src->SetValue(cached->url());
      if (rewrite_count_ != NULL) {
        rewrite_count_->Add(1);
      }
    }

    int actual_width, actual_height;
    if (insert_image_dimensions_ &&
        !element->FindAttribute(HtmlName::kWidth) &&
        !element->FindAttribute(HtmlName::kHeight) &&
        cached->RememberedInt(kWidthKey, &actual_width) &&
        cached->RememberedInt(kHeightKey, &actual_height)) {
      // Add image dimensions.  We don't bother if even a single image
      // dimension is already specified---even though we don't resize in that
      // case, either, because we might be off by a pixel in the other
      // dimension from the size chosen by the browser.  We also don't bother
      // to resize if either dimension is specified with units (px, em, %)
      // rather than as absolute pixels.  But note that we DO attempt to
      // include image dimensions even if we otherwise choose not to optimize
      // an image.
      html_parse_->AddAttribute(element, HtmlName::kWidth, actual_width);
      html_parse_->AddAttribute(element, HtmlName::kHeight, actual_height);
    }
  }
}

bool ImgRewriteFilter::CanInline(
    int img_inline_max_bytes, const StringPiece& contents,
    const ContentType* content_type, std::string* data_url) {
  bool ok = false;
  if (content_type != NULL && contents.size() <= img_inline_max_bytes) {
    DataUrl(*content_type, BASE64, contents, data_url);
    ok = true;
  }
  return ok;
}

void ImgRewriteFilter::EndElementImpl(HtmlElement* element) {
  HtmlElement::Attribute *src = img_filter_->ParseImgElement(element);
  if (src != NULL) {
    if (log_image_elements_) {
      // We now know that element is an img tag.
      // Log the element in its original form.
      std::string tagstring;
      element->ToString(&tagstring);
      html_parse_->Info(
          html_parse_->id(), element->begin_line_number(),
          "Found image: %s", tagstring.c_str());
    }
    RewriteImageUrl(element, src);
  }
}

}  // namespace net_instaweb
