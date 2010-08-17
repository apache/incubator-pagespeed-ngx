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
#include "net/instaweb/rewriter/public/img_tag_scanner.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/rewrite.pb.h"  // for ImgRewriteUrl
#include "net/instaweb/util/public/atom.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/file_system.h"
#include <string>
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/meta_data.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_writer.h"

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

// Threshold size (in bytes) below which we should just inline images
// encountered.
// TODO(jmaessen): Heuristic must be more sophisticated.  Does this image touch
// a fresh domain?  Require opening a new connection?  If so we can afford to
// inline quite large images (basically anything we could transmit in the
// resulting RTTs)---but of course we don't know about RTT here.  In the absence
// of such information, we ought to inline if header length + url size can be
// saved by inlining image, without increasing the size in packets of the html.
// Otherwise we end up loading the image in favor of the html, which might be a
// lose.  More work is needed here to figure out the exact tradeoffs involved,
// especially as we also undermine image cacheability.
const int kImageInlineThreshold = 2048;

// Should we log each image element as we encounter it?  Handy for debug.
// TODO(jmaessen): Hook into event logging infrastructure.
const bool kLogImageElements = false;

// We overload some http status codes for our own purposes

// This is used to retain the knowledge that a particular image is not
// profitable to optimize.  According to pagespeed, 200, 203, 206, and
// 304 are cacheable.  So we must select from those.
const HttpStatus::Code kNotOptimizable = HttpStatus::NOT_MODIFIED;  // 304

// This is used to indicate that an image has been determined to be so
// small that we should inline it in the HTML, rather than serving it as
// an external resource.  This must be a cacheable code.
const HttpStatus::Code kInlineImage = HttpStatus::NO_CONTENT;  // 204

}  // namespace

ImgRewriteFilter::ImgRewriteFilter(StringPiece path_prefix,
                                   HtmlParse* html_parse,
                                   ResourceManager* resource_manager,
                                   FileSystem* file_system)
    : RewriteFilter(path_prefix),
      file_system_(file_system),
      html_parse_(html_parse),
      img_filter_(new ImgTagScanner(html_parse)),
      resource_manager_(resource_manager),
      s_width_(html_parse->Intern("width")),
      s_height_(html_parse->Intern("height")),
      rewrite_count_(NULL),
      rewrite_saved_bytes_(NULL) {
  Statistics* stats = resource_manager_->statistics();
  if (stats != NULL) {
    rewrite_count_ = stats->AddVariable("image_rewrites");
    rewrite_saved_bytes_ = stats->AddVariable("image_rewrite_saved_bytes");
  }
}

void ImgRewriteFilter::OptimizeImage(
    const Resource* input_resource,
    const ImgRewriteUrl& url_proto, Image* image, OutputResource* result) {
  // TODO(jmarantz): OptimizeImage should embed input_resource.

  if (result != NULL && !result->IsWritten() && image != NULL) {
    int img_width, img_height, width, height;
    if (url_proto.has_width() && url_proto.has_height() &&
        image->Dimensions(&img_width, &img_height)) {
      width = url_proto.width();
      height = url_proto.height();
      int64 area = static_cast<int64>(width) * height;
      int64 img_area = static_cast<int64>(img_width) * img_height;
      if (area < img_area * kMaxAreaRatio) {
        if (image->ResizeTo(width, height)) {
          html_parse_->InfoHere("Resized image `%s' from %dx%d to %dx%d",
                                url_proto.origin_url().c_str(),
                                img_width, img_height, width, height);
        } else {
          html_parse_->InfoHere(
              "Couldn't resize image `%s' from %dx%d to %dx%d",
              url_proto.origin_url().c_str(),
              img_width, img_height, width, height);
        }
      } else {
        html_parse_->InfoHere(
            "Not worth resizing image `%s' from %dx%d to %dx%d",
            url_proto.origin_url().c_str(),
            img_width, img_height, width, height);
      }
    }
    // Unconditionally write resource back so we don't re-attempt optimization.
    MessageHandler* message_handler = html_parse_->message_handler();

    std::string inlined_url;
    int64 origin_expire_time_ms = input_resource->CacheExpirationTimeMs();
    if (image->output_size() < kImageInlineThreshold &&
        (img_width > 1 || img_height > 1) &&  // Rule out marker images <= 1x1
        image->AsInlineData(&inlined_url)) {
      resource_manager_->Write(kInlineImage, inlined_url, result,
                               origin_expire_time_ms, message_handler);
    } else if (image->output_size() <
               image->input_size() * kMaxRewrittenRatio) {
      resource_manager_->Write(
          HttpStatus::OK, image->Contents(), result, origin_expire_time_ms,
          message_handler);
      // TODO(jmarantz): what happens if Write returns false?

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
        rewrite_saved_bytes_->Add(image->input_size() - image->output_size());
      }
    } else {
      // Write nothing and set status code to indicate not to rewrite
      // in future.
      resource_manager_->Write(kNotOptimizable, "", result,
                               origin_expire_time_ms, message_handler);
    }
  }
}

Image* ImgRewriteFilter::GetImage(const ImgRewriteUrl& url_proto,
                                  Resource* img_resource) {
  Image* image = NULL;
  MessageHandler* message_handler = html_parse_->message_handler();
  if (img_resource == NULL) {
    html_parse_->WarningHere("no input resource for %s",
                             url_proto.origin_url().c_str());
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

OutputResource* ImgRewriteFilter::ImageOutputResource(
    const std::string& url_string, Image* image) {
  OutputResource* result = NULL;
  if (image != NULL) {
    const ContentType* content_type = image->content_type();
    if (content_type != NULL) {
      MessageHandler* message_handler = html_parse_->message_handler();
      result = resource_manager_->CreateNamedOutputResource(
          filter_prefix_, url_string, content_type, message_handler);
    }
  }
  return result;
}

OutputResource* ImgRewriteFilter::OptimizedImageFor(
    const ImgRewriteUrl& url_proto, const std::string& url_string,
    Resource* img_resource) {
  scoped_ptr<Image> image(GetImage(url_proto, img_resource));
  OutputResource* result = ImageOutputResource(url_string, image.get());
  OptimizeImage(img_resource, url_proto, image.get(), result);
  return result;
}

void ImgRewriteFilter::RewriteImageUrl(const HtmlElement& element,
                                       HtmlElement::Attribute* src) {
  // TODO(jmaessen): content type can change after re-compression.
  // How do we deal with that given only URL?
  // Separate input and output content type?
  int width, height;
  ImgRewriteUrl rewritten_url_proto;
  std::string rewritten_url;
  MessageHandler* message_handler = html_parse_->message_handler();
  scoped_ptr<Resource> input_resource(
      resource_manager_->CreateInputResource(src->value(), message_handler));

  if ((input_resource != NULL) &&
      resource_manager_->ReadIfCached(input_resource.get(), message_handler) &&
      input_resource->ContentsValid()) {
    // Always rewrite to absolute url used to obtain resource.
    // This lets us do context-free fetches of content.
    rewritten_url_proto.set_origin_url(input_resource->url());
    if (element.IntAttributeValue(s_width_, &width) &&
        element.IntAttributeValue(s_height_, &height)) {
      // Specific image size is called for.  Rewrite to that size.
      rewritten_url_proto.set_width(width);
      rewritten_url_proto.set_height(height);
    }
    Encode(rewritten_url_proto, &rewritten_url);

    // Create an output output resource and fetch it, as that will tell
    // us if we have already optimized it, or determined that it was not
    // worth optimizing.
    scoped_ptr<OutputResource> output_resource(
       resource_manager_->CreateNamedOutputResource(
           filter_prefix_, rewritten_url, NULL, message_handler));
    if (!resource_manager_->FetchOutputResource(
            output_resource.get(), NULL, NULL, message_handler)) {
      scoped_ptr<Image> image(GetImage(rewritten_url_proto,
                                       input_resource.get()));
      const ContentType* content_type = NULL;

      if (image != NULL) {
        // Even if we know the content type from the extension coming
        // in, the content-type can change as a result of compression,
        // e.g. gif to png, or anying to vp8.
        //
        // TODO(jmaessen): test this.
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
                "Cannot detect content type of image url `%s`", src->value());
            break;
        }
        if (content_type != NULL) {
          output_resource->SetType(content_type);
          OptimizeImage(input_resource.get(), rewritten_url_proto, image.get(),
                        output_resource.get());
        }
      }
    }

    if (output_resource->IsWritten()) {
      if (output_resource->metadata()->status_code() == HttpStatus::OK) {
        html_parse_->InfoHere("%s remapped to %s",
                              src->value(), output_resource->url().c_str());
        src->SetValue(output_resource->url());
        if (rewrite_count_ != NULL) {
          rewrite_count_->Add(1);
        }
      } else if (output_resource->metadata()->status_code() == kInlineImage) {
        html_parse_->InfoHere("%s inlined", src->value());
        src->SetValue(output_resource->contents());
      } else {
        html_parse_->InfoHere("%s not rewritten due to lack of benefit",
                              src->value());
      }
    }
  }
}

void ImgRewriteFilter::EndElement(HtmlElement* element) {
  HtmlElement::Attribute *src = img_filter_->ParseImgElement(element);
  if (src != NULL) {
    if (kLogImageElements) {
      // We now know that element is an img tag.
      // Log the element in its original form.
      std::string tagstring;
      element->ToString(&tagstring);
      html_parse_->Info(
          html_parse_->filename(), element->begin_line_number(),
          "Found image: %s", tagstring.c_str());
    }
    RewriteImageUrl(*element, src);
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
  StringPiece suffix = resource->suffix();
  const ContentType *content_type = NameExtensionToContentType(suffix);
  StringPiece stripped_url = resource->name();
  if (content_type != NULL) {
    ImgRewriteUrl url_proto;
    if (Decode(stripped_url, &url_proto)) {
      std::string stripped_url_string = stripped_url.as_string();
      scoped_ptr<Resource> input_image(
          resource_manager_->CreateInputResource(
              url_proto.origin_url(), message_handler));

      // TODO(jmarantz): this needs to be refactored slightly to
      // allow for asynchronous fetches of the input image, if
      // it's not obtained via cache or local filesystem read.

      scoped_ptr<OutputResource> image_resource(OptimizedImageFor(
          url_proto, stripped_url_string, input_image.get()));
      if (image_resource != NULL) {
        if (resource_manager_->FetchOutputResource(
                image_resource.get(), writer, response_headers,
                message_handler)) {
          callback->Done(true);
        } else {
          ok = false;
          failure_reason = "Server could not read image resource.";
        }
        if (image_resource->metadata()->status_code() != HttpStatus::OK) {
          // Note that this should not happen, because the url should not have
          // escaped into the wild.  We're content serving an empty response if
          // it does.  We *could* serve / redirect to the origin_url as a fail
          // safe, but it's probably not worth it.  Instead we log and hope that
          // this causes us to find and fix the problem.
          message_handler->Error(resource->name().as_string().c_str(), 0,
                                 "Rewriting of %s rejected, "
                                 "but URL requested (mistaken rewriting?).",
                                 url_proto.origin_url().c_str());
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
          ok &= writer->Write(url_proto.origin_url(), message_handler);
          ok &= writer->Write("\" alt=\"Temporarily Moved\"/>",
                              message_handler);
          response_headers->set_major_version(1);
          response_headers->set_minor_version(1);
          response_headers->SetStatusAndReason(HttpStatus::TEMPORARY_REDIRECT);
          response_headers->Add("Location", url_proto.origin_url().c_str());
          response_headers->Add("Content-Type", "text/html");
        }
        if (ok) {
          callback->Done(true);
        }
      }
    } else {
      ok = false;
      failure_reason = "Server could not decode image source.";
    }
  } else {
    ok = false;
    failure_reason = "Unrecognized image content type.";
  }

  if (!ok) {
    writer->Write(failure_reason, message_handler);
    response_headers->set_status_code(HttpStatus::NOT_FOUND);
    response_headers->set_reason_phrase(failure_reason);
    message_handler->Error(resource->name().as_string().c_str(), 0,
                           "%s", failure_reason);
  }
  return ok;
}

}  // namespace net_instaweb
