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

#include "net/instaweb/rewriter/public/img_rewrite_filter.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/rewriter/public/image_dim.h"
#include "net/instaweb/rewriter/public/img_tag_scanner.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/atom.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/file_system.h"
#include <string>
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/meta_data.h"
#include "net/instaweb/util/public/statistics.h"
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

// We overload some http status codes for our own purposes

// This is used to retain the knowledge that a particular image is not
// profitable to optimize.  According to pagespeed, 200, 203, 206, and
// 304 are cacheable.  So we must select from those.
const HttpStatus::Code kNotOptimizable = HttpStatus::kNotModified;  // 304

// names for Statistics variables.
const char kImageRewrites[] = "image_rewrites";
const char kImageRewriteSavedBytes[] = "image_rewrite_saved_bytes";
const char kImageInline[] = "image_inline";

}  // namespace


ImageUrlEncoder::ImageUrlEncoder(UrlEscaper* url_escaper, ImageDim* stored_dim)
    : url_escaper_(url_escaper), stored_dim_(stored_dim) { }

ImageUrlEncoder::~ImageUrlEncoder() { }

void ImageUrlEncoder::EncodeToUrlSegment(
    const StringPiece& origin_url, std::string* rewritten_url) {
  stored_dim_->EncodeTo(rewritten_url);
  url_escaper_->EncodeToUrlSegment(origin_url, rewritten_url);
}

bool ImageUrlEncoder::DecodeFromUrlSegment(
    const StringPiece& rewritten_url, std::string* origin_url) {
  // Note that "remaining" is shortened from the left as we parse.
  StringPiece remaining(rewritten_url.data(), rewritten_url.size());
  return (stored_dim_->DecodeFrom(&remaining) &&
          url_escaper_->DecodeFromUrlSegment(remaining, origin_url));
}

ImgRewriteFilter::ImgRewriteFilter(RewriteDriver* driver,
                                   bool log_image_elements,
                                   bool insert_image_dimensions,
                                   StringPiece path_prefix,
                                   size_t img_inline_max_bytes)
    : RewriteFilter(driver, path_prefix),
      file_system_(driver->file_system()),
      html_parse_(driver->html_parse()),
      img_filter_(new ImgTagScanner(html_parse_)),
      resource_manager_(driver->resource_manager()),
      img_inline_max_bytes_(img_inline_max_bytes),
      log_image_elements_(log_image_elements),
      insert_image_dimensions_(insert_image_dimensions),
      s_width_(html_parse_->Intern("width")),
      s_height_(html_parse_->Intern("height")),
      rewrite_count_(NULL),
      inline_count_(NULL),
      rewrite_saved_bytes_(NULL) {
  Statistics* stats = resource_manager_->statistics();
  if (stats != NULL) {
    rewrite_count_ = stats->GetVariable(kImageRewrites);
    rewrite_saved_bytes_ = stats->GetVariable(
        kImageRewriteSavedBytes);
    inline_count_ = stats->GetVariable(kImageInline);
  }
}

void ImgRewriteFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kImageInline);
  statistics->AddVariable(kImageRewriteSavedBytes);
  statistics->AddVariable(kImageRewrites);
}

void ImgRewriteFilter::OptimizeImage(
    const Resource& input_resource, const ImageDim& page_dim,
    Image* image, OutputResource* result) {
  if (result != NULL && !result->IsWritten() && image != NULL) {
    ImageDim img_dim;
    image->Dimensions(&img_dim);
    const char* message;  // Informational message for logging only.
    if (page_dim.valid() && img_dim.valid()) {
      int64 page_area =
          static_cast<int64>(page_dim.width()) * page_dim.height();
      int64 img_area = static_cast<int64>(img_dim.width()) * img_dim.height();
      if (page_area < img_area * kMaxAreaRatio) {
        if (image->ResizeTo(page_dim)) {
          message = "Resized image";
        } else {
          message = "Couldn't resize image";
        }
      } else {
        message = "Not worth resizing image";
      }
      html_parse_->InfoHere("%s `%s' from %dx%d to %dx%d", message,
                            input_resource.url().c_str(),
                            img_dim.width(), img_dim.height(),
                            page_dim.width(), page_dim.height());
    }
    // Unconditionally write resource back so we don't re-attempt optimization.
    MessageHandler* message_handler = html_parse_->message_handler();

    int64 origin_expire_time_ms = input_resource.CacheExpirationTimeMs();
    if (image->output_size() <
        image->input_size() * kMaxRewrittenRatio) {
      if (resource_manager_->Write(
              HttpStatus::kOK, image->Contents(), result,
              origin_expire_time_ms, message_handler)) {
        html_parse_->InfoHere(
            "Shrinking image `%s' (%u bytes) to `%s' (%u bytes)",
            input_resource.url().c_str(),
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
      }
    } else {
      // Write nothing and set status code to indicate not to rewrite
      // in future.
      resource_manager_->Write(kNotOptimizable, "", result,
                               origin_expire_time_ms, message_handler);
    }
  }
}

Image* ImgRewriteFilter::GetImage(const StringPiece& origin_url,
                                  Resource* img_resource) {
  Image* image = NULL;
  MessageHandler* message_handler = html_parse_->message_handler();
  if (img_resource == NULL) {
    html_parse_->WarningHere("no input resource for %s",
                             origin_url.as_string().c_str());
  } else if (!resource_manager_->ReadIfCached(img_resource, message_handler)) {
    html_parse_->WarningHere("%s wasn't loaded",
                             img_resource->url().c_str());
  } else if (!img_resource->ContentsValid()) {
    html_parse_->WarningHere("Img contents from %s are invalid.",
                             img_resource->url().c_str());
  } else {
    image = new Image(img_resource->contents(), img_resource->url(),
                      resource_manager_->filename_prefix(), file_system_,
                      message_handler);
  }
  return image;
}

bool ImgRewriteFilter::OptimizedImageFor(
    const StringPiece& origin_url, const ImageDim& page_dim,
    Resource* img_resource, OutputResource* output) {
  scoped_ptr<Image> image(GetImage(origin_url, img_resource));
  bool ok = image.get() != NULL && image->image_type() != Image::IMAGE_UNKNOWN;
  if (ok) {
    OptimizeImage(*img_resource, page_dim, image.get(), output);
  }
  return ok;
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
        html_parse_->ErrorHere(
            "Cannot detect content type of image url `%s`",
            origin_url.c_str());
        break;
    }
  }
  return content_type;
}

void ImgRewriteFilter::RewriteImageUrl(HtmlElement* element,
                                       HtmlElement::Attribute* src) {
  // TODO(jmaessen): content type can change after re-compression.
  // How do we deal with that given only URL?
  // Separate input and output content type?
  MessageHandler* message_handler = html_parse_->message_handler();
  scoped_ptr<Resource> input_resource(
      resource_manager_->CreateInputResourceAndReadIfCached(
          base_gurl(), src->value(), message_handler));
  if (input_resource != NULL && input_resource->ContentsValid()) {
    ImageDim page_dim;
    // Always rewrite to absolute url used to obtain resource.
    // This lets us do context-free fetches of content.
    int width, height;
    if (element->IntAttributeValue(s_width_, &width) &&
        element->IntAttributeValue(s_height_, &height)) {
      // Specific image size is called for.  Rewrite to that size.
      page_dim.set_dims(width, height);
    }
    scoped_ptr<Image> image(GetImage(src->value(), input_resource.get()));
    const ContentType* content_type =
        ImageToContentType(src->value(), image.get());

    if (content_type != NULL) {
      ImageDim actual_dim;
      image->Dimensions(&actual_dim);
      // Create an output resource and fetch it, as that will tell
      // us if we have already optimized it, or determined that it was not
      // worth optimizing.
      std::string rewritten_name;
      ImageUrlEncoder encoder(resource_manager_->url_escaper(), &page_dim);
      scoped_ptr<OutputResource> output_resource(
          resource_manager_->CreateOutputResourceFromResource(
              filter_prefix_, content_type, &encoder, input_resource.get(),
              message_handler));
      if (output_resource.get() != NULL) {
        if (!resource_manager_->FetchOutputResource(
                output_resource.get(), NULL, NULL, message_handler)) {
          OptimizeImage(*input_resource, page_dim, image.get(),
                        output_resource.get());
        }
        if (output_resource->IsWritten()) {
          UpdateTargetElement(*input_resource, *output_resource,
                              page_dim, actual_dim, element, src);
        }
      }
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

// Given image processing reflected in the already-written output_resource,
// actually update the element (particularly the src attribute), and log
// statistics on what happened.
void ImgRewriteFilter::UpdateTargetElement(
    const Resource& input_resource, const OutputResource& output_resource,
    const ImageDim& page_dim, const ImageDim& actual_dim,
    HtmlElement* element, HtmlElement::Attribute* src) {

  if (actual_dim.valid() &&
      (actual_dim.width() > 1 || actual_dim.height() > 1)) {
    std::string inlined_url;
    bool output_ok =
        output_resource.metadata()->status_code() == HttpStatus::kOK;
    bool ie6or7 = driver_->user_agent().IsIe6or7();
    if (!ie6or7 &&
        ((output_ok &&
          CanInline(img_inline_max_bytes_, output_resource.contents(),
                    output_resource.type(), &inlined_url)) ||
         CanInline(img_inline_max_bytes_, input_resource.contents(),
                   input_resource.type(), &inlined_url))) {
      src->SetValue(inlined_url);
      if (inline_count_ != NULL) {
        inline_count_->Add(1);
      }
    } else {
      if (output_ok) {
        src->SetValue(output_resource.url());
        if (rewrite_count_ != NULL) {
          rewrite_count_->Add(1);
        }
      }
      if (insert_image_dimensions_ && actual_dim.valid() && !page_dim.valid() &&
          !element->FindAttribute(s_width_) &&
          !element->FindAttribute(s_height_)) {
        // Add image dimensions.  We don't bother if even a single image
        // dimension is already specified---even though we don't resize in that
        // case, either, because we might be off by a pixel in the other
        // dimension from the size chosen by the browser.  We also don't bother
        // to resize if either dimension is specified with units (px, em, %)
        // rather than as absolute pixes.  But note that we DO attempt to
        // include image dimensions even if we otherwise choose not to optimize
        // an image.  This may require examining the image contents if we didn't
        // just perform the image processing.
        element->AddAttribute(s_width_, actual_dim.width());
        element->AddAttribute(s_height_, actual_dim.height());
      }
    }
  }
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

void ImgRewriteFilter::Flush() {
  // TODO(jmaessen): wait here for resources to have been rewritten??
}

bool ImgRewriteFilter::Fetch(OutputResource* resource,
                             Writer* writer,
                             const MetaData& request_header,
                             MetaData* response_headers,
                             UrlAsyncFetcher* fetcher,
                             MessageHandler* message_handler,
                             UrlAsyncFetcher::Callback* callback) {
  bool ok = true;
  const char* failure_reason = "";
  const ContentType *content_type = resource->type();
  if (content_type != NULL) {
    ImageDim page_dim;  // Dimensions given in source page (invalid if absent).
    ImageUrlEncoder encoder(resource_manager_->url_escaper(), &page_dim);
    scoped_ptr<Resource> input_image(
        resource_manager_->CreateInputResourceFromOutputResource(
            &encoder, resource, message_handler));
    if (input_image.get() != NULL) {
      std::string origin_url = input_image->url();
      // TODO(jmarantz): this needs to be refactored slightly to
      // allow for asynchronous fetches of the input image, if
      // it's not obtained via cache or local filesystem read.

      if (OptimizedImageFor(
              origin_url, page_dim, input_image.get(), resource)) {
        if (resource_manager_->FetchOutputResource(
                resource, writer, response_headers, message_handler)) {
          if (resource->metadata()->status_code() != HttpStatus::kOK) {
            // Note that this should not happen, because the url
            // should not have escaped into the wild.  We're content
            // serving an empty response if it does.  We *could* serve
            // / redirect to the origin_url as a fail safe, but it's
            // probably not worth it.  Instead we log and hope that
            // this causes us to find and fix the problem.
            message_handler->Error(resource->url().c_str(), 0,
                                   "Rewriting %s rejected, "
                                   "but URL requested (mistaken rewriting?).",
                                   origin_url.c_str());
          }
          callback->Done(true);
        } else {
          ok = false;
          failure_reason = "Server could not read image resource.";
        }
      } else {
        ok = false;
        failure_reason = "Server could not find source image.";
      }
      // Image processing has failed, forward the original image data.
      if (!ok && input_image != NULL) {
        if (input_image->ContentsValid()) {
          ok = writer->Write(input_image->contents(), message_handler);
        }
        if (ok) {
          resource_manager_->SetDefaultHeaders(content_type, response_headers);
        } else {
          message_handler->Error(resource->name().as_string().c_str(), 0,
                                 "%s", failure_reason);
          ok = writer->Write("<img src=\"", message_handler);
          ok &= writer->Write(origin_url, message_handler);
          ok &= writer->Write("\" alt=\"Temporarily Moved\"/>",
                              message_handler);
          response_headers->set_major_version(1);
          response_headers->set_minor_version(1);
          response_headers->SetStatusAndReason(HttpStatus::kTemporaryRedirect);
          response_headers->Add(HttpAttributes::kLocation, origin_url.c_str());
          response_headers->Add(HttpAttributes::kContentType, "text/html");
        }
        if (ok) {
          callback->Done(true);
        }
      }
    }
  } else {
    ok = false;
    failure_reason = "Unrecognized image content type.";
  }
  if (!ok) {
    writer->Write(failure_reason, message_handler);
    response_headers->set_status_code(HttpStatus::kNotFound);
    response_headers->set_reason_phrase(failure_reason);
    message_handler->Error(resource->name().as_string().c_str(), 0,
                           "%s", failure_reason);
  }
  return ok;
}

}  // namespace net_instaweb
