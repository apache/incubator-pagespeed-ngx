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

#include "net/instaweb/rewriter/public/image.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"
#ifdef USE_SYSTEM_OPENCV
#include "cv.h"
#include "highgui.h"
#else
#include "third_party/opencv/src/opencv/include/opencv/cv.h"
#include "third_party/opencv/src/opencv/include/opencv/highgui.h"
#endif
#include "pagespeed/image_compression/gif_reader.h"
#include "pagespeed/image_compression/jpeg_optimizer.h"
#include "pagespeed/image_compression/png_optimizer.h"

using pagespeed::image_compression::PngOptimizer;

namespace net_instaweb {

namespace ImageHeaders {

const char kPngHeader[] = "\x89PNG\r\n\x1a\n";
const size_t kPngHeaderLength = STATIC_STRLEN(kPngHeader);
const char kPngIHDR[] = "\0\0\0\x0dIHDR";
const size_t kPngIntSize = 4;
const size_t kPngSectionHeaderLength = 2 * kPngIntSize;
const size_t kIHDRDataStart = kPngHeaderLength + kPngSectionHeaderLength;
const size_t kPngSectionMinSize = kPngSectionHeaderLength + kPngIntSize;
const size_t kPngColourTypeOffset = kIHDRDataStart + 2 * kPngIntSize + 1;
const char kPngAlphaChannel = 0x4;  // bit of ColourType set for alpha channel
const char kPngIDAT[] = "IDAT";
const char kPngtRNS[] = "tRNS";

const char kGifHeader[] = "GIF8";
const size_t kGifHeaderLength = STATIC_STRLEN(kGifHeader);
const size_t kGifDimStart = kGifHeaderLength + 2;
const size_t kGifIntSize = 2;

const size_t kJpegIntSize = 2;

}  // namespace ImageHeaders

Image::Image(const StringPiece& original_contents,
             const GoogleString& url,
             const StringPiece& file_prefix,
             MessageHandler* handler)
    : file_prefix_(file_prefix.data(), file_prefix.size()),
      handler_(handler),
      image_type_(IMAGE_UNKNOWN),
      original_contents_(original_contents),
      output_contents_(),
      output_valid_(false),
      opencv_image_(NULL),
      opencv_load_possible_(true),
      changed_(false),
      url_(url) { }

Image::Image(int width, int height, Type type,
      const StringPiece& tmp_dir, MessageHandler* handler)
    : file_prefix_(tmp_dir.data(), tmp_dir.size()),
      handler_(handler),
      image_type_(type),
      original_contents_(),
      output_contents_(),
      output_valid_(false),
      opencv_image_(NULL),
      opencv_load_possible_(true),
      changed_(false),
      url_() {
  dims_.set_width(width);
  dims_.set_height(height);
}

Image::~Image() {
  CleanOpenCv();
}

// Looks through blocks of jpeg stream to find SOFn block
// indicating encoding and dimensions of image.
// Loosely based on code and FAQs found here:
//    http://www.faqs.org/faqs/jpeg-faq/part1/
void Image::FindJpegSize() {
  const StringPiece& buf = original_contents_;
  size_t pos = 2;  // Position of first data block after header.
  while (pos < buf.size()) {
    // Read block identifier
    int id = CharToInt(buf[pos++]);
    if (id == 0xff) {  // Padding byte
      continue;
    }
    // At this point pos points to first data byte in block.  In any block,
    // first two data bytes are size (including these 2 bytes).  But first,
    // make sure block wasn't truncated on download.
    if (pos + ImageHeaders::kJpegIntSize > buf.size()) {
      break;
    }
    int length = JpegIntAtPosition(buf, pos);
    // Now check for a SOFn header, which describes image dimensions.
    if (0xc0 <= id && id <= 0xcf &&  // SOFn header
        length >= 8 &&               // Valid SOFn block size
        pos + 1 + 3 * ImageHeaders::kJpegIntSize <= buf.size() &&
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
      dims_.set_height(
          JpegIntAtPosition(buf, pos + 1 + ImageHeaders::kJpegIntSize));
      dims_.set_width(
          JpegIntAtPosition(buf, pos + 1 + 2 * ImageHeaders::kJpegIntSize));
      break;
    }
    pos += length;
  }
  if (!ImageUrlEncoder::HasValidDimensions(dims_) ||
      (dims_.height() <= 0) || (dims_.width() <= 0)) {
    dims_.Clear();
    handler_->Error(url_.c_str(), 0,
                    "Couldn't find jpeg dimensions (data truncated?).");
  }
}

// Looks at first (IHDR) block of png stream to find image dimensions.
// See also: http://www.w3.org/TR/PNG/
void Image::FindPngSize() {
  const StringPiece& buf = original_contents_;
  // Here we make sure that buf contains at least enough data that we'll be able
  // to decipher the image dimensions first, before we actually check for the
  // headers and attempt to decode the dimensions (which are the first two ints
  // after the IHDR section label).
  if ((buf.size() >=  // Not truncated
       ImageHeaders::kIHDRDataStart + 2 * ImageHeaders::kPngIntSize) &&
      (StringPiece(buf.data() + ImageHeaders::kPngHeaderLength,
                   ImageHeaders::kPngSectionHeaderLength) ==
       StringPiece(ImageHeaders::kPngIHDR,
                   ImageHeaders::kPngSectionHeaderLength))) {
    dims_.set_width(PngIntAtPosition(buf, ImageHeaders::kIHDRDataStart));
    dims_.set_height(PngIntAtPosition(
        buf, ImageHeaders::kIHDRDataStart + ImageHeaders::kPngIntSize));
  } else {
    handler_->Error(url_.c_str(), 0,
                    "Couldn't find png dimensions "
                    "(data truncated or IHDR missing).");
  }
}

// Looks at header of GIF file to extract image dimensions.
// See also: http://en.wikipedia.org/wiki/Graphics_Interchange_Format
void Image::FindGifSize() {
  const StringPiece& buf = original_contents_;
  // Make sure that buf contains enough data that we'll be able to
  // decipher the image dimensions before we attempt to do so.
  if (buf.size() >=
      ImageHeaders::kGifDimStart + 2 * ImageHeaders::kGifIntSize) {
    // Not truncated
    dims_.set_width(GifIntAtPosition(buf, ImageHeaders::kGifDimStart));
    dims_.set_height(GifIntAtPosition(
        buf, ImageHeaders::kGifDimStart + ImageHeaders::kGifIntSize));
  } else {
    handler_->Error(url_.c_str(), 0,
                    "Couldn't find gif dimensions (data truncated)");
  }
}

// Looks at image data in order to determine image type, and also fills in any
// dimension information it can (setting image_type_ and dims_).
void Image::ComputeImageType() {
  // Image classification based on buffer contents gakked from leptonica,
  // but based on well-documented headers (see Wikipedia etc.).
  // Note that we can be fooled if we're passed random binary data;
  // we make the call based on as few as two bytes (JPEG).
  const StringPiece& buf = original_contents_;
  if (buf.size() >= 8) {
    // Note that gcc rightly complains about constant ranges with the
    // negative char constants unless we cast.
    switch (CharToInt(buf[0])) {
      case 0xff:
        // Either jpeg or jpeg2
        // (the latter we don't handle yet, and don't bother looking for).
        if (CharToInt(buf[1]) == 0xd8) {
          image_type_ = IMAGE_JPEG;
          FindJpegSize();
        }
        break;
      case 0x89:
        // Possible png.
        if (StringPiece(buf.data(), ImageHeaders::kPngHeaderLength) ==
            StringPiece(ImageHeaders::kPngHeader,
                        ImageHeaders::kPngHeaderLength)) {
          image_type_ = IMAGE_PNG;
          FindPngSize();
        }
        break;
      case 'G':
        // Possible gif.
        if ((StringPiece(buf.data(), ImageHeaders::kGifHeaderLength) ==
             StringPiece(ImageHeaders::kGifHeader,
                         ImageHeaders::kGifHeaderLength)) &&
            (buf[ImageHeaders::kGifHeaderLength] == '7' ||
             buf[ImageHeaders::kGifHeaderLength] == '9') &&
            buf[ImageHeaders::kGifHeaderLength + 1] == 'a') {
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

// Compute whether a PNG can have transparent / semi-transparent pixels
// by walking the image data in accordance with the spec:
//   http://www.w3.org/TR/PNG/
// If the colour type (UK spelling from spec) includes an alpha channel, or
// there is a tRNS section with at least one entry before IDAT, then we assume
// the image contains non-opaque pixels and return true.
bool Image::ComputePngTransparency(const StringPiece& buf) {
  // We assume the image has transparency until we prove otherwise.
  // This allows us to deal conservatively with truncation etc.
  bool has_transparency = true;
  if (buf.size() > ImageHeaders::kPngColourTypeOffset &&
      ((buf[ImageHeaders::kPngColourTypeOffset] &
        ImageHeaders::kPngAlphaChannel) == 0)) {
    // The colour type indicates that there is no dedicated alpha channel.  Now
    // we must look for a tRNS section indicating the existence of transparent
    // colors or palette entries.
    size_t section_start = ImageHeaders::kPngHeaderLength;
    while (section_start + ImageHeaders::kPngSectionHeaderLength < buf.size()) {
      size_t section_size = PngIntAtPosition(buf, section_start);
      if (PngSectionIdIs(ImageHeaders::kPngIDAT, buf, section_start)) {
        // tRNS section must occur before first IDAT.  This image doesn't have a
        // tRNS section, and thus doesn't have transparency.
        has_transparency = false;
        break;
      } else if (PngSectionIdIs(ImageHeaders::kPngtRNS, buf, section_start) &&
                 section_size > 0) {
        // Found a nonempty tRNS section.  This image has_transparency.
        break;
      } else {
        // Move on to next section.
        section_start += section_size + ImageHeaders::kPngSectionMinSize;
      }
    }
  }
  return has_transparency;
}

// Returns true if the image has transparency (an alpha channel, or a
// transparent color).  Note that certain ambiguously-formatted images might
// yield false positive results here; we don't check whether alpha channels
// contain non-opaque data, nor do we check if a distinguished transparent color
// is actually used in an image.  We assume that if the image file contains
// flags for transparency, it does so for a reason.
bool Image::HasTransparency(const StringPiece& buf) {
  bool result;
  switch (image_type()) {
    case IMAGE_PNG:
      result = ComputePngTransparency(buf);
      break;
    case IMAGE_GIF:
      // This means we didn't translate to png for whatever reason.
      result = true;
      break;
    default:
      result = false;
      break;
  }
  return result;
}

// Makes sure OpenCV version of image is loaded if that is possible.
// Returns value of opencv_load_possible_ after load attempted.
// Note that if the load fails, opencv_load_possible_ will be false
// and future calls to LoadOpenCv will fail fast.
bool Image::LoadOpenCv() {
  if (!(opencv_image_ == NULL && opencv_load_possible_)) {
    // Already attempted load, fall through.
  } else if (image_type() == IMAGE_UNKNOWN) {
    // Can't load, remember that fact.
    opencv_load_possible_ = false;
  } else {
    // Attempt to load into opencv.
    StringPiece image_data_source(original_contents_);
    if (image_type_ == IMAGE_GIF) {
      // OpenCV doesn't understand gif format directly, but png works well.  So
      // we perform a pre-emptive early translation to png, which we'll end
      // up keeping if the OpenCV load or resize operations fail.
      opencv_load_possible_ = output_valid_ || ComputeOutputContents();
      if (opencv_load_possible_) {
        image_data_source = output_contents_;
      }
    }
    if (original_contents_.size() == 0) {
      opencv_load_possible_ = LoadOpenCvEmpty();
    } else if (opencv_load_possible_) {
      opencv_load_possible_ = !HasTransparency(image_data_source);
      if (opencv_load_possible_) {
        opencv_load_possible_ = LoadOpenCvFromBuffer(image_data_source);
      }
    }
    if (opencv_load_possible_) {
      // A bit of belt and suspenders dimension checking.  We used to do this
      // for every image we loaded, but now we only do it when we're already
      // paying the cost of OpenCV image conversion.
      bool has_dims = ImageUrlEncoder::HasValidDimensions(dims_);
      if (has_dims && (dims_.width() != opencv_image_->width)) {
        handler_->Error(url_.c_str(), 0,
                        "Computed width %d doesn't match OpenCV %d",
                        dims_.width(), opencv_image_->width);
      }
      if (has_dims && (dims_.height() != opencv_image_->height)) {
        handler_->Error(url_.c_str(), 0,
                        "Computed height %d doesn't match OpenCV %d",
                        dims_.height(), opencv_image_->height);
      }
    }
  }
  return opencv_load_possible_;
}

// Get rid of OpenCV image data gracefully (requires a call to OpenCV).
void Image::CleanOpenCv() {
  if (opencv_image_ != NULL) {
    cvReleaseImage(&opencv_image_);
  }
}

bool Image::LoadOpenCvEmpty() {
  // empty canvas -- width and height must be set already.
  if (ImageUrlEncoder::HasValidDimensions(dims_)) {
    // TODO(abliss): Need to figure out the right values for these.
    int depth = 8, channels = 3;
    opencv_image_ = cvCreateImage(cvSize(dims_.width(), dims_.height()),
                                  depth, channels);
    cvSetZero(opencv_image_);
    return true;
  } else {
    return false;
  }
}

#ifdef USE_OPENCV_2_1

bool Image::LoadOpenCvFromBuffer(const StringPiece& data) {
  CvMat cv_original_contents =
      cvMat(1, data.size(), CV_8UC1, const_cast<char*>(data.data()));

  // Note: this is more convenient than imdecode as it lets us
  // get an image pointer directly, and not just a Mat
  opencv_image_ = cvDecodeImage(&cv_original_contents);
  return opencv_image_ != NULL;
}

bool Image::SaveOpenCvToBuffer(OpenCvBuffer* buf) {
  // This is preferable to cvEncodeImage as it makes it easy to avoid a copy.
  // Note: period included with the extension on purpose.
  return cv::imencode(content_type()->file_extension(), opencv_image_, *buf);
}

StringPiece Image::OpenCvBufferToStringPiece(const OpenCvBuffer& buf) {
  return StringPiece(reinterpret_cast<const char*>(&buf[0]), buf.size());
}

#else

bool Image::TempFileForImage(FileSystem* fs,
                             const StringPiece& contents,
                             GoogleString* filename) {
  GoogleString tmp_filename;
  bool ok = fs->WriteTempFile(file_prefix_, contents, &tmp_filename, handler_);
  if (ok) {
    *filename = StrCat(tmp_filename, content_type()->file_extension());
    ok = fs->RenameFile(tmp_filename.c_str(), filename->c_str(), handler_);
  }
  return ok;
}

bool Image::LoadOpenCvFromBuffer(const StringPiece& data) {
  StdioFileSystem fs;
  GoogleString filename;
  bool ok = TempFileForImage(&fs, data, &filename);
  if (ok) {
    opencv_image_ = cvLoadImage(filename.c_str());
    fs.RemoveFile(filename.c_str(), handler_);
  }
  return opencv_image_ != NULL;
}

bool Image::SaveOpenCvToBuffer(OpenCvBuffer* buf) {
  StdioFileSystem fs;
  GoogleString filename;
  bool ok = TempFileForImage(&fs, StringPiece(), &filename);
  if (ok) {
    cvSaveImage(filename.c_str(), opencv_image_);
    ok = fs.ReadFile(filename.c_str(), buf, handler_);
    fs.RemoveFile(filename.c_str(), handler_);
  }
  return ok;
}

StringPiece Image::OpenCvBufferToStringPiece(const OpenCvBuffer& buf) {
  return StringPiece(buf);
}

#endif

void Image::Dimensions(ImageDim* natural_dim) {
  if (!ImageUrlEncoder::HasValidDimensions(dims_)) {
    ComputeImageType();
  }
  *natural_dim = dims_;
}

bool Image::ResizeTo(const ImageDim& new_dim) {
  CHECK(ImageUrlEncoder::HasValidDimensions(new_dim));
  if ((new_dim.width() <= 0) || (new_dim.height() <= 0)) {
    return false;
  }

  if (changed_) {
    // If we already resized, drop data and work with original image.
    UndoChange();
  }
  bool ok = opencv_image_ != NULL || LoadOpenCv();
  if (ok) {
    IplImage* rescaled_image =
        cvCreateImage(cvSize(new_dim.width(), new_dim.height()),
                      opencv_image_->depth,
                      opencv_image_->nChannels);
    ok = rescaled_image != NULL;
    if (ok) {
      cvResize(opencv_image_, rescaled_image);
      cvReleaseImage(&opencv_image_);
      opencv_image_ = rescaled_image;
      changed_ = true;
      output_valid_ = false;
    }
  }
  return changed_;
}

void Image::UndoChange() {
  if (changed_) {
    CleanOpenCv();
    output_valid_ = false;
    image_type_ = IMAGE_UNKNOWN;
    changed_ = false;
  }
}

// Performs image optimization and output
bool Image::ComputeOutputContents() {
  if (!output_valid_) {
    bool ok = true;
    OpenCvBuffer opencv_contents;
    StringPiece contents = original_contents_;
    // Choose appropriate source for image contents.
    // Favor original contents if image unchanged.
    if (changed_ && opencv_image_ != NULL) {
      ok = SaveOpenCvToBuffer(&opencv_contents);
      if (ok) {
        contents = OpenCvBufferToStringPiece(opencv_contents);
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
              GoogleString(contents.data(), contents.size()),
              &output_contents_);
          break;
        case IMAGE_PNG: {
          pagespeed::image_compression::PngReader png_reader;
          ok = PngOptimizer::OptimizePng
              (png_reader,
              GoogleString(contents.data(), contents.size()),
              &output_contents_);
          break;
        }
        case IMAGE_GIF: {
          pagespeed::image_compression::GifReader gif_reader;
          ok = PngOptimizer::OptimizePng
              (gif_reader,
              GoogleString(contents.data(), contents.size()),
              &output_contents_);
          if (ok) {
            image_type_ = IMAGE_PNG;
          }
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

bool Image::DrawImage(Image* image, int x, int y) {
  if (!LoadOpenCv() || !image->LoadOpenCv()) {
    return false;
  }
  ImageDim other_dim;
  image->Dimensions(&other_dim);
  if (!ImageUrlEncoder::HasValidDimensions(dims_) ||
      !ImageUrlEncoder::HasValidDimensions(other_dim) ||
      (other_dim.width() + x > dims_.width())
      || (other_dim.height() + y > dims_.height())) {
    // image will not fit.
    return false;
  }
#ifdef USE_OPENCV_2_1
  // OpenCV 2.1.0 api
  cv::Mat mat(image->opencv_image_, false);
  cv::Mat canvas(opencv_image_, false);
  cv::Mat submat = canvas.rowRange(y, y + other_dim.height())
      .colRange(x, x + other_dim.width());
  mat.copyTo(submat);
#else
  // OpenCV 1.0.0 api
  CvMat mat;
  cvGetMat(image->opencv_image_, &mat);
  CvMat canvas;
  cvGetMat(opencv_image_, &canvas);
  CvMat submat;
  cvGetRows(opencv_image_, &submat, y, y + other_dim.height(), 1);
  CvMat submat2;
  cvGetCols(&submat, &submat2, x, x + other_dim.width());
  cvCopy(&mat, &submat2);
#endif
  changed_ = true;
  return true;
}

}  // namespace net_instaweb
