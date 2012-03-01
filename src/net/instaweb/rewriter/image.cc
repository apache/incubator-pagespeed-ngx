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

#include <algorithm>
#include <cstddef>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/image_data_lookup.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/rewriter/public/webp_optimizer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
extern "C" {
#ifdef USE_SYSTEM_LIBWEBP
#include "webp/decode.h"
#else
#include "third_party/libwebp/webp/decode.h"
#endif
}
#ifdef USE_SYSTEM_OPENCV
#include "cv.h"
#include "highgui.h"
#else
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#endif
#include "pagespeed/image_compression/gif_reader.h"
#include "pagespeed/image_compression/image_converter.h"
#include "pagespeed/image_compression/jpeg_optimizer.h"
#include "pagespeed/image_compression/png_optimizer.h"

#if (CV_MAJOR_VERSION == 2 && CV_MINOR_VERSION >= 1) || (CV_MAJOR_VERSION > 2)
#include <vector>
#define USE_OPENCV_2_1
#else
#include "net/instaweb/util/public/stdio_file_system.h"
#endif

using pagespeed::image_compression::ImageConverter;
using pagespeed::image_compression::JpegCompressionOptions;
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
const int kMaxJpegQuality = 100;

}  // namespace ImageHeaders

// TODO(jmaessen): Put ImageImpl into private namespace.

class ImageImpl : public Image {
 public:
  ImageImpl(int width, int height, Type type,
            const StringPiece& tmp_dir, MessageHandler* handler);
  ImageImpl(const StringPiece& original_contents,
            const GoogleString& url,
            const StringPiece& file_prefix,
            CompressionOptions* options,
            MessageHandler* handler);

  virtual void Dimensions(ImageDim* natural_dim);
  virtual bool ResizeTo(const ImageDim& new_dim);
  virtual bool DrawImage(Image* image, int x, int y);
  virtual bool EnsureLoaded(bool output_useful);
  virtual void SetTransformToLowRes();

 private:
  // byte buffer type most convenient for working with given OpenCV version
#ifdef USE_OPENCV_2_1
  typedef std::vector<unsigned char> OpenCvBuffer;
#else
  typedef GoogleString OpenCvBuffer;
#endif
  virtual ~ImageImpl();

  // Concrete helper methods called by parent class
  virtual void ComputeImageType();
  virtual bool ComputeOutputContents();

  bool QuickLoadGifToOutputContents();

  // Helper methods
  static bool ComputePngTransparency(const StringPiece& buf);

  // Internal methods used only in the implementation
  void UndoChange();
  void FindJpegSize();
  void FindPngSize();
  void FindGifSize();
  void FindWebpSize();
  bool HasTransparency(const StringPiece& buf);
  bool LoadOpenCv();
  void CleanOpenCv();

  // Convert the given options object to jpeg compression options.
  void ConvertToJpegOptions(const Image::CompressionOptions& options,
                            JpegCompressionOptions* jpeg_options);

  // Initializes an empty image.
  bool LoadOpenCvEmpty();

  // Assumes all filetype + transparency checks have been done.
  // Reads data, writes to opencv_image_
  bool LoadOpenCvFromBuffer(const StringPiece& data);

  // Reads from opencv_image_, writes to buf
  bool SaveOpenCvToBuffer(OpenCvBuffer* buf);

  // Encodes 'buf' in a StringPiece
  static StringPiece OpenCvBufferToStringPiece(const OpenCvBuffer& buf);

#ifndef USE_OPENCV_2_1
  // Helper that creates & writes a temporary file for us in proper prefix with
  // proper extension.
  bool TempFileForImage(FileSystem* fs, const StringPiece& contents,
                        GoogleString* filename);
#endif

  const GoogleString file_prefix_;
  MessageHandler* handler_;
  IplImage* opencv_image_;        // Lazily filled on OpenCV load.
  bool opencv_load_possible_;     // Attempt opencv_load in future?
  bool changed_;
  const GoogleString url_;
  ImageDim dims_;
  scoped_ptr<Image::CompressionOptions> options_;
  bool low_quality_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ImageImpl);
};

void ImageImpl::SetTransformToLowRes() {
  low_quality_enabled_ = true;
  options_->webp_quality = 10;
  options_->jpeg_quality = 10;
}

Image::Image(const StringPiece& original_contents)
    : image_type_(IMAGE_UNKNOWN),
      original_contents_(original_contents),
      output_contents_(),
      output_valid_(false) { }

ImageImpl::ImageImpl(const StringPiece& original_contents,
                     const GoogleString& url,
                     const StringPiece& file_prefix,
                     Image::CompressionOptions* options,
                     MessageHandler* handler)
    : Image(original_contents),
      file_prefix_(file_prefix.data(), file_prefix.size()),
      handler_(handler),
      opencv_image_(NULL),
      opencv_load_possible_(true),
      changed_(false),
      url_(url),
      options_(options),
      low_quality_enabled_(false) {}

Image* NewImage(const StringPiece& original_contents,
                const GoogleString& url,
                const StringPiece& file_prefix,
                Image::CompressionOptions* options,
                MessageHandler* handler) {
  return new ImageImpl(original_contents, url, file_prefix, options, handler);
}

Image::Image(Type type)
    : image_type_(type),
      original_contents_(),
      output_contents_(),
      output_valid_(false) { }

ImageImpl::ImageImpl(int width, int height, Type type,
                     const StringPiece& tmp_dir, MessageHandler* handler)
    : Image(type),
      file_prefix_(tmp_dir.data(), tmp_dir.size()),
      handler_(handler),
      opencv_image_(NULL),
      opencv_load_possible_(true),
      changed_(false),
      url_(),
      low_quality_enabled_(false) {
  options_.reset(new Image::CompressionOptions());
  dims_.set_width(width);
  dims_.set_height(height);
}

Image* BlankImage(int width, int height, Image::Type type,
                  const StringPiece& tmp_dir, MessageHandler* handler) {
  return new ImageImpl(width, height, type, tmp_dir, handler);
}

Image::~Image() {
}

ImageImpl::~ImageImpl() {
  CleanOpenCv();
}

// Looks through blocks of jpeg stream to find SOFn block
// indicating encoding and dimensions of image.
// Loosely based on code and FAQs found here:
//    http://www.faqs.org/faqs/jpeg-faq/part1/
void ImageImpl::FindJpegSize() {
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
void ImageImpl::FindPngSize() {
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
void ImageImpl::FindGifSize() {
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

void ImageImpl::FindWebpSize() {
  const uint8* webp = reinterpret_cast<const uint8*>(original_contents_.data());
  const int webp_size = original_contents_.size();
  int width = 0, height = 0;
  if (WebPGetInfo(webp, webp_size, &width, &height) > 0) {
    dims_.set_width(width);
    dims_.set_height(height);
  } else {
    handler_->Error(url_.c_str(), 0, "Couldn't find webp dimensions ");
  }
}

// Looks at image data in order to determine image type, and also fills in any
// dimension information it can (setting image_type_ and dims_).
void ImageImpl::ComputeImageType() {
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
      case 'R':
        // Possible Webp
        // Detailed explanation on parsing webp format is available at
        // http://code.google.com/speed/webp/docs/riff_container.html
        if (buf.size() >= 20 && buf.substr(1, 3) == "IFF" &&
            buf.substr(8, 4) == "WEBP") {
          image_type_ = IMAGE_WEBP;
          FindWebpSize();
        }
        break;
      default:
        break;
    }
  }
}

const ContentType* Image::TypeToContentType(Type image_type) {
  const ContentType* res = NULL;
  switch (image_type) {
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
    case IMAGE_WEBP:
      res = &kContentTypeWebp;
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
bool ImageImpl::ComputePngTransparency(const StringPiece& buf) {
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
bool ImageImpl::HasTransparency(const StringPiece& buf) {
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
// and future calls to EnsureLoaded will fail fast.
bool ImageImpl::EnsureLoaded(bool output_useful) {
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
      // we perform a pre-emptive early translation to png.
      // If the output may be useful, the PNG will be optimized, which we will
      // end up keeping if the OpenCV load or resize operations fail.
      // If the output is not expected to be written out, we will produce an
      // unoptimized PNG instead.
      if (output_valid_) {
        // Output bits already available.
        opencv_load_possible_ = true;
      } else {
        // Need to load.
        if (output_useful) {
          opencv_load_possible_ = ComputeOutputContents();
        } else {
          opencv_load_possible_ = QuickLoadGifToOutputContents();
        }
      }
      image_data_source = output_contents_;
    }
    if (original_contents_.size() == 0) {
      opencv_load_possible_ = LoadOpenCvEmpty();
    } else if (opencv_load_possible_) {
      opencv_load_possible_ = !HasTransparency(image_data_source);
      if (opencv_load_possible_) {
        opencv_load_possible_ = LoadOpenCvFromBuffer(image_data_source);
      }
    }
    if (opencv_load_possible_ && ImageUrlEncoder::HasValidDimensions(dims_)) {
      // A bit of belt and suspenders dimension checking.  We used to do this
      // for every image we loaded, but now we only do it when we're already
      // paying the cost of OpenCV image conversion.
      if (dims_.width() != opencv_image_->width) {
        handler_->Error(url_.c_str(), 0,
                        "Computed width %d doesn't match OpenCV %d",
                        dims_.width(), opencv_image_->width);
      }
      if (dims_.height() != opencv_image_->height) {
        handler_->Error(url_.c_str(), 0,
                        "Computed height %d doesn't match OpenCV %d",
                        dims_.height(), opencv_image_->height);
      }
    }
  }
  return opencv_load_possible_;
}

// Get rid of OpenCV image data gracefully (requires a call to OpenCV).
void ImageImpl::CleanOpenCv() {
  if (opencv_image_ != NULL) {
    cvReleaseImage(&opencv_image_);
  }
}

bool ImageImpl::LoadOpenCvEmpty() {
  // empty canvas -- width and height must be set already.
  bool ok = false;
  if (ImageUrlEncoder::HasValidDimensions(dims_)) {
    // TODO(abliss): Need to figure out the right values for these.
    int depth = 8, channels = 3;
    try {
      opencv_image_ = cvCreateImage(cvSize(dims_.width(), dims_.height()),
                                    depth, channels);
      cvSetZero(opencv_image_);
      ok = true;
    } catch (cv::Exception& e) {
      handler_->Message(
          kError, "OpenCv exception in LoadOpenCvEmpty: %s", e.what());
    }
  }
  return ok;
}

#ifdef USE_OPENCV_2_1
// OpenCV 2.1 supports memory-to-memory format conversion.

bool ImageImpl::LoadOpenCvFromBuffer(const StringPiece& data) {
  CvMat cv_original_contents =
      cvMat(1, data.size(), CV_8UC1, const_cast<char*>(data.data()));

  // Note: this is more convenient than imdecode as it lets us
  // get an image pointer directly, and not just a Mat
  try {
    opencv_image_ = cvDecodeImage(&cv_original_contents);
  } catch (cv::Exception& e) {
    handler_->Error(
        url_.c_str(), 0, "OpenCv exception in LoadOpenCvFromBuffer: %s",
        e.what());
    return false;
  }
  return opencv_image_ != NULL;
}

bool ImageImpl::SaveOpenCvToBuffer(OpenCvBuffer* buf) {
  // This is preferable to cvEncodeImage as it makes it easy to avoid a copy.
  // Note: period included with the extension on purpose.
  return cv::imencode(content_type()->file_extension(), cv::Mat(opencv_image_),
                      *buf);
}

#else
// Older OpenCV libraries require compressed data to reside on disk,
// so we need to write image data out and read it back in.

bool ImageImpl::TempFileForImage(FileSystem* fs,
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

bool ImageImpl::LoadOpenCvFromBuffer(const StringPiece& data) {
  StdioFileSystem fs;
  GoogleString filename;
  bool ok = TempFileForImage(&fs, data, &filename);
  if (ok) {
    opencv_image_ = cvLoadImage(filename.c_str());
    fs.RemoveFile(filename.c_str(), handler_);
  }
  return opencv_image_ != NULL;
}

bool ImageImpl::SaveOpenCvToBuffer(OpenCvBuffer* buf) {
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

#endif

StringPiece ImageImpl::OpenCvBufferToStringPiece(const OpenCvBuffer& buf) {
  return StringPiece(reinterpret_cast<const char*>(&buf[0]), buf.size());
}

void ImageImpl::Dimensions(ImageDim* natural_dim) {
  if (!ImageUrlEncoder::HasValidDimensions(dims_)) {
    ComputeImageType();
  }
  *natural_dim = dims_;
}

bool ImageImpl::ResizeTo(const ImageDim& new_dim) {
  CHECK(ImageUrlEncoder::HasValidDimensions(new_dim));
  if ((new_dim.width() <= 0) || (new_dim.height() <= 0)) {
    return false;
  }

  if (changed_) {
    // If we already resized, drop data and work with original image.
    UndoChange();
  }
  bool ok = opencv_image_ != NULL || EnsureLoaded(true);
  if (ok) {
    IplImage* rescaled_image =
        cvCreateImage(cvSize(new_dim.width(), new_dim.height()),
                      opencv_image_->depth,
                      opencv_image_->nChannels);
    ok = rescaled_image != NULL;
    if (ok) {
#ifdef USE_OPENCV_2_1
      {
        // Inlined from: cvResize(opencv_image_, rescaled_image, CV_INTER_AREA);
        cv::Mat src = cv::cvarrToMat(opencv_image_);
        cv::Mat dst = cv::cvarrToMat(rescaled_image);
        DCHECK(src.type() == dst.type());
        cv::resize(src, dst, dst.size(), static_cast<double>(dst.cols)/src.cols,
                   static_cast<double>(dst.rows)/src.rows, CV_INTER_AREA);
      }
#else
      cvResize(opencv_image_, rescaled_image, CV_INTER_AREA);
#endif
      cvReleaseImage(&opencv_image_);
      opencv_image_ = rescaled_image;
      changed_ = true;
      output_valid_ = false;
      output_contents_.clear();
    }
  }
  return changed_;
}

void ImageImpl::UndoChange() {
  if (changed_) {
    CleanOpenCv();
    output_valid_ = false;
    output_contents_.clear();
    image_type_ = IMAGE_UNKNOWN;
    changed_ = false;
  }
}

// Performs image optimization and output
bool ImageImpl::ComputeOutputContents() {
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
      // We copy the data to a string eagerly as we're very likely to need it
      // (only unrecognized formats don't require it, in which case we probably
      // don't get this far in the first place).
      // TODO(jmarantz): The PageSpeed library should, ideally, take StringPiece
      // args rather than const string&.  We would save lots of string-copying
      // if we made that change.
      GoogleString string_for_image(contents.data(), contents.size());
      switch (image_type()) {
        case IMAGE_UNKNOWN:
          break;
        case IMAGE_WEBP:
            ok = ReduceWebpImageQuality(string_for_image,
                                        options_->webp_quality,
                                        &output_contents_);
            // TODO(pulkitg): Convert a webp image to jpeg image if
            // web_preferred_ is false.
          break;
        case IMAGE_JPEG:
          if (options_->webp_preferred && !low_quality_enabled_) {
            // Right now we just compute the webp, and assume that it'll be
            // smaller than the equivalent re-compressed jpg.  Doing jpg
            // recompression *as well* and picking the smaller file is very
            // expensive for what's likely to be minimal gain.  We fall back
            // to jpg reoptimization if webp fails.
            ok = OptimizeWebp(string_for_image, &output_contents_);
            if (!ok) {
              handler_->Error(url_.c_str(), 0,
                              "Failed to create webp!");
            }
          }
          if (ok) {  // && webp_preferred, which is implied.
            image_type_ = IMAGE_WEBP;
          } else {
            JpegCompressionOptions jpeg_options;
            ConvertToJpegOptions(*options_.get(), &jpeg_options);
            ok = pagespeed::image_compression::OptimizeJpegWithOptions(
                string_for_image, &output_contents_, jpeg_options);
          }
          break;
        case IMAGE_PNG: {
          pagespeed::image_compression::PngReader png_reader;

          if ((options_->convert_png_to_jpeg || low_quality_enabled_) &&
              options_->jpeg_quality > 0) {
            bool is_png;
            JpegCompressionOptions jpeg_options;
            ConvertToJpegOptions(*options_.get(), &jpeg_options);
            ok = ImageConverter::OptimizePngOrConvertToJpeg(
                png_reader, string_for_image, jpeg_options,
                &output_contents_, &is_png);
            if (ok && !is_png) {
              image_type_ = IMAGE_JPEG;
            }
          } else {
            pagespeed::image_compression::PngReader png_reader;
            ok = PngOptimizer::OptimizePngBestCompression
                (png_reader, string_for_image, &output_contents_);
          }
          break;
        }
        case IMAGE_GIF: {
          if (low_quality_enabled_) {
            // Currently, gif to jpeg conversion is not present in pagespeed
            // library.
            ok = false;
          } else {
            pagespeed::image_compression::GifReader gif_reader;
            ok = PngOptimizer::OptimizePngBestCompression
                (gif_reader, string_for_image, &output_contents_);
            if (ok) {
              image_type_ = IMAGE_PNG;
            }
          }
          break;
        }
      }
    }
    output_valid_ = ok;
  }
  return output_valid_;
}

// Converts gif into a png in output_contents_ as quickly as possible;
// that is, unlike ComputeOutputContents it does not use BestCompression.
bool ImageImpl::QuickLoadGifToOutputContents() {
  CHECK(!output_valid_);
  CHECK_EQ(image_type(), IMAGE_GIF);
  CHECK(!changed_);

  GoogleString string_for_image(original_contents_.data(),
                                original_contents_.size());
  pagespeed::image_compression::GifReader gif_reader;
  bool ok = PngOptimizer::OptimizePng(gif_reader, string_for_image,
                                      &output_contents_);
  output_valid_ = ok;
  if (ok) {
    image_type_ = IMAGE_PNG;
  }
  return ok;
}

void ImageImpl::ConvertToJpegOptions(const Image::CompressionOptions& options,
                                     JpegCompressionOptions* jpeg_options) {
  jpeg_options->retain_color_profile = options.retain_color_profile;
  jpeg_options->retain_exif_data = options.retain_exif_data;
  jpeg_options->progressive = options.progressive_jpeg;
  if (options.jpeg_quality > 0) {
    jpeg_options->lossy = true;
    jpeg_options->lossy_options.quality =
        std::min(ImageHeaders::kMaxJpegQuality, options.jpeg_quality);
    if (options.progressive_jpeg) {
      jpeg_options->lossy_options.num_scans =
          options.jpeg_num_progressive_scans;
    }
  }
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

bool ImageImpl::DrawImage(Image* image, int x, int y) {
  ImageImpl* impl = static_cast<ImageImpl*>(image);
  if (!EnsureLoaded(false) || !image->EnsureLoaded(false)) {
    return false;
  }
  ImageDim other_dim;
  impl->Dimensions(&other_dim);
  if (!ImageUrlEncoder::HasValidDimensions(dims_) ||
      !ImageUrlEncoder::HasValidDimensions(other_dim) ||
      (other_dim.width() + x > dims_.width())
      || (other_dim.height() + y > dims_.height())) {
    // image will not fit.
    return false;
  }
#ifdef USE_OPENCV_2_1
  // OpenCV 2.1.0 api
  cv::Mat mat(impl->opencv_image_, false);
  cv::Mat canvas(opencv_image_, false);
  cv::Mat submat = canvas.rowRange(y, y + other_dim.height())
      .colRange(x, x + other_dim.width());
  mat.copyTo(submat);
#else
  // OpenCV 1.0.0 api
  CvMat mat;
  cvGetMat(impl->opencv_image_, &mat);
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
