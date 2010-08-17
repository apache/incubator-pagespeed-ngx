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

#include "net/instaweb/rewriter/public/image.h"

#include "base/basictypes.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/message_handler.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"
#undef PAGESPEED_PNG_OPTIMIZER_GIF_READER
#define PAGESPEED_PNG_OPTIMIZER_GIF_READER 0
#include "third_party/opencv/src/opencv/include/opencv/cv.h"
#include "third_party/opencv/src/opencv/include/opencv/highgui.h"
#include "pagespeed/image_compression/gif_reader.h"
#include "pagespeed/image_compression/jpeg_optimizer.h"
#include "pagespeed/image_compression/png_optimizer.h"

namespace net_instaweb {

const char Image::kPngHeader[] = "\x89PNG\r\n\x1a\n";
const size_t Image::kPngHeaderLength = sizeof(kPngHeader) - 1;
const char Image::kPngIHDR[] = "\0\0\0\x0dIHDR";
const size_t Image::kPngIHDRLength = sizeof(kPngIHDR) - 1;
const size_t Image::kIHDRDataStart = kPngHeaderLength + kPngIHDRLength;
const size_t Image::kPngIntSize = 4;

const char Image::kGifHeader[] = "GIF8";
const size_t Image::kGifHeaderLength = sizeof(kGifHeader) - 1;
const size_t Image::kGifDimStart = kGifHeaderLength + 2;
const size_t Image::kGifIntSize = 2;

const size_t Image::kJpegIntSize = 2;

namespace {

bool WriteTempFileWithContentType(
    const StringPiece& prefix_name, const ContentType& content_type,
    const StringPiece& buffer, std::string* filename,
    FileSystem* file_system, MessageHandler* handler) {
  std::string tmp_filename;
  bool ok = file_system->WriteTempFile(
      prefix_name.as_string().c_str(), buffer, &tmp_filename, handler);
  if (ok) {
    *filename = StrCat(tmp_filename, content_type.file_extension());
    ok = file_system->RenameFile(
        tmp_filename.c_str(), filename->c_str(), handler);
  }
  return ok;
}

}  // namespace

Image::Image(const StringPiece& original_contents,
             const std::string& url,
             const StringPiece& file_prefix,
             FileSystem* file_system,
             MessageHandler* handler)
    : file_prefix_(),
      file_system_(file_system),
      handler_(handler),
      image_type_(IMAGE_UNKNOWN),
      original_contents_(original_contents),
      output_contents_(),
      output_valid_(false),
      opencv_filename_(),
      opencv_image_(NULL),
      opencv_load_possible_(true),
      resized_(false),
      url_(url),
      width_(-1),                   // Lazily initialized.  Catch errors.
      height_(-1) {
  file_prefix.CopyToString(&file_prefix_);
}

// Look through blocks of jpeg stream to find SOFn block
// indicating encoding and dimensions of image.
// Loosely based on code and FAQs found here:
//    http://www.faqs.org/faqs/jpeg-faq/part1/
void Image::FindJpegSize() {
  const StringPiece& buf = original_contents_;
  size_t pos = 2;  // Position of first data block after header.
  while (pos < buf.size()) {
    // Read block identifier
    int id = charToInt(buf[pos++]);
    if (id == 0xff) {  // Padding byte
      continue;
    }
    // At this point pos points to first data byte in block.  In any block,
    // first two data bytes are size (including these 2 bytes).  But first,
    // make sure block wasn't truncated on download.
    if (pos + kJpegIntSize > buf.size()) {
      break;
    }
    int length = JpegIntAtPosition(buf, pos);
    // Now check for a SOFn header, which describes image dimensions.
    if (0xc0 <= id && id <= 0xcf &&  // SOFn header
        length >= 8 &&               // Valid SOFn block size
        pos + 1 + 3 * kJpegIntSize <= buf.size() &&
        // Above avoids case where dimension data was truncated
        id != 0xc4 && id != 0xc8 && id != 0xcc) {
      // 0xc4, 0xc8, 0xcc aren't actually valid SOFn headers.
      // NOTE: we don't care if we have the whole SOFn block,
      // just that we can fetch both dimensions without trouble.
      // Our image download could be truncated at this point for
      // all we care.
      // We're a bit sloppy about SOFn block size, as it's
      // actually 8 + 3 * buf[pos+2], but for our purposes this
      // will suffice as we don't parse subsequent metadata (which
      // describes the formatting of chunks of image data).
      height_ = JpegIntAtPosition(buf, pos + 1 + kJpegIntSize);
      width_ = JpegIntAtPosition(buf, pos + 1 + 2 * kJpegIntSize);
      break;
    }
    pos += length;
  }
  if (width_ <= 0 || height_ <= 0) {
    handler_->Error(url_.c_str(), 0,
                    "Couldn't find jpeg dimensions (data truncated?).");
  }
}

// Look at first (IHDR) block of png stream to find image dimensions.
// See also: http://www.w3.org/TR/PNG/
void Image::FindPngSize() {
  const StringPiece& buf = original_contents_;
  if (buf.size() >= kIHDRDataStart + 2 * kPngIntSize &&  // Not truncated
      buf.substr(kPngHeaderLength, kPngIHDRLength).compare(
          StringPiece(kPngIHDR, kPngIHDRLength)) == 0) {
    width_ = PngIntAtPosition(buf, kIHDRDataStart);
    height_ = PngIntAtPosition(buf, kIHDRDataStart+kPngIntSize);
  } else {
    handler_->Error(url_.c_str(), 0,
                    "Couldn't find png dimensions "
                    "(data truncated or IHDR missing).");
  }
}

// Look at header of GIF file to extract image dimensions
// See also: http://en.wikipedia.org/wiki/Graphics_Interchange_Format
void Image::FindGifSize() {
  const StringPiece& buf = original_contents_;
  if (buf.size() >= kGifDimStart + 2 * kGifIntSize) {  // Not truncated
    width_ = GifIntAtPosition(buf, kGifDimStart);
    height_ = GifIntAtPosition(buf, kGifDimStart + kGifIntSize);
  } else {
    handler_->Error(url_.c_str(), 0,
                    "Couldn't find gif dimensions (data truncated)");
  }
}

void Image::ComputeImageType() {
  // Image classification based on buffer contents gakked from leptonica,
  // but based on well-documented headers (see Wikipedia etc.).
  // Note that we can be fooled if we're passed random binary data;
  // we make the call based on as few as two bytes (JPEG)
  const StringPiece& buf = original_contents_;
  if (buf.size() >= 8) {
    // Note that gcc rightly complains about constant ranges with the
    // negative char constants unless we cast.
    switch (charToInt(buf[0])) {
      case 0xff:
        // either jpeg or jpeg2
        // (the latter we don't handle yet, and don't bother looking for).
        if (charToInt(buf[1]) == 0xd8) {
          image_type_ = IMAGE_JPEG;
          FindJpegSize();
        }
        break;
      case 0x89:
        // possible png
        if (buf.substr(0, kPngHeaderLength).compare(
                StringPiece(kPngHeader, kPngHeaderLength)) == 0) {
          image_type_ = IMAGE_PNG;
          FindPngSize();
        }
        break;
      case 'G':
        // possible gif
        if (buf.substr(0, kGifHeaderLength).compare(
                StringPiece(kGifHeader, kGifHeaderLength)) == 0 &&
            (buf[4] == '7' || buf[4] == '9') && buf[5] == 'a') {
          image_type_ = IMAGE_GIF;
          FindGifSize();
        }
        break;
      default:
        break;
    }
  }
}

const ContentType* Image::content_type() {
  const ContentType* res = NULL;
  switch (image_type()) {
    case IMAGE_UNKNOWN:
      break;
    case IMAGE_JPEG:
      res = &kContentTypeJpeg;
      break;
    case IMAGE_PNG:
      res = &kContentTypePng;
      break;
    case IMAGE_GIF:
      res = &kContentTypeGif;
      break;
  }
  return res;
}


// Make sure OpenCV version of image is loaded if that is possible.
// Returns value of opencv_load_possible_ after load attempted.
// Note that if the load fails, opencv_load_possible_ will be false
// and future calls to LoadOpenCV will fail fast.
bool Image::LoadOpenCV() {
  if (opencv_image_ == NULL && opencv_load_possible_) {
    Image::Type image_type = this->image_type();
    const ContentType* content_type = this->content_type();
    opencv_load_possible_ = (content_type != NULL &&
                             image_type != IMAGE_GIF);
    if (opencv_load_possible_) {
      opencv_load_possible_ =
          WriteTempFileWithContentType(
              file_prefix_, *content_type,
              original_contents_, &opencv_filename_,
              file_system_, handler_);
    }
    if (opencv_load_possible_) {
      opencv_image_ = cvLoadImage(opencv_filename_.c_str());
      file_system_->RemoveFile(opencv_filename_.c_str(), handler_);
      opencv_load_possible_ = (opencv_image_ != NULL);
    }
  }
  return opencv_load_possible_;
}

void Image::CleanOpenCV() {
  if (opencv_image_ != NULL) {
    cvReleaseImage(&opencv_image_);
  }
}

bool Image::Dimensions(int* width, int* height) {
  bool ok = false;
  ok = opencv_image_ != NULL || LoadOpenCV();
  if (ok) {
    // Check our width and height against openCV.
    // Believe OpenCV.
    // TODO(jmaessen): rip out belt and suspenders when
    // we're confident it works.
    if (0 <= width_ && width_ != opencv_image_->width) {
      handler_->Error(url_.c_str(), 0,
                      "Computed width %d doesn't match OpenCV %d",
                      width_, opencv_image_->width);
    }
    if (0 <= height_ && height_ != opencv_image_->height) {
      handler_->Error(url_.c_str(), 0,
                      "Computed height %d doesn't match OpenCV %d",
                      height_, opencv_image_->height);
    }
    width_ = opencv_image_->width;
    height_ = opencv_image_->height;
  }
  if (0 <= width_ && 0 <= height_) {
    *width = width_;
    *height = height_;
    ok = true;
  }
  return ok;
}

bool Image::ResizeTo(int width, int height) {
  if (resized_) {
    // If we already resized, drop data and work with original image.
    UndoResize();
  }
  bool ok = opencv_image_ != NULL || LoadOpenCV();
  if (ok) {
    IplImage* rescaled_image =
        cvCreateImage(cvSize(width * 1.0, height * 1.0),
                      opencv_image_->depth,
                      opencv_image_->nChannels);
    ok = rescaled_image != NULL;
    if (ok) {
      cvResize(opencv_image_, rescaled_image);
      cvReleaseImage(&opencv_image_);
      opencv_image_ = rescaled_image;
    }
    resized_ = ok;
  }
  return resized_;
}

// Perform image optimization and output
bool Image::ComputeOutputContents() {
  if (!output_valid_) {
    bool ok = true;
    std::string opencv_contents;
    StringPiece contents = original_contents_;
    // Choose appropriate source for image contents.
    // Favor original contents if image unchanged.
    if (resized_ && opencv_image_ != NULL) {
      cvSaveImage(opencv_filename_.c_str(), opencv_image_);
      ok = file_system_->ReadFile(opencv_filename_.c_str(),
                                  &opencv_contents, handler_);
      file_system_->RemoveFile(opencv_filename_.c_str(), handler_);
      if (ok) {
        contents = opencv_contents;
      }
    }
    // Take image contents and re-compress them.
    if (ok) {
      // If we can't optimize the image, we'll fail.
      ok = false;
      switch (image_type()) {
        case IMAGE_UNKNOWN:
          break;
        case IMAGE_JPEG:
          // TODO(jmarantz): The PageSpeed library should, ideally, take
          // StringPiece args rather than const string&.  We would save
          // lots of string-copying if we made that change.
          ok = pagespeed::image_compression::OptimizeJpeg(
              std::string(contents.data(), contents.size()),
              &output_contents_);
          break;
        case IMAGE_PNG: {
          pagespeed::image_compression::PngReader png_reader;
          ok = pagespeed::image_compression::PngOptimizer::OptimizePng(
              png_reader,
              std::string(contents.data(), contents.size()),
              &output_contents_);
          break;
        }
        case IMAGE_GIF: {
#if PAGESPEED_PNG_OPTIMIZER_GIF_READER
          pagespeed::image_compression::GifReader gif_reader;
          ok = pagespeed::image_compression::PngOptimizer::OptimizePng(
              gif_reader, contents, &output_contents_);
          if (ok) {
            image_type_ = IMAGE_PNG;
          }
#endif
          break;
        }
      }
    }
    output_valid_ = ok;
  }
  return output_valid_;
}

StringPiece Image::Contents() {
  StringPiece contents;
  if (this->image_type() != IMAGE_UNKNOWN) {
    contents = original_contents_;
    if (output_valid_ || ComputeOutputContents()) {
      contents = output_contents_;
    }
  }
  return contents;
}

bool Image::AsInlineData(std::string* data_url) {
  StringPiece contents = Contents();
  const ContentType* content_type = this->content_type();
  bool ok = content_type != NULL && (contents.data() != NULL);
  if (ok) {
    DataUrl(*content_type, BASE64, contents, data_url);
  }
  return ok;
}

}  // namespace net_instaweb
