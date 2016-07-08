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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_H_

#include <cstddef>

#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/image_types.pb.h"
#include "pagespeed/kernel/image/image_util.h"

namespace net_instaweb {
class Histogram;
class MessageHandler;
class Timer;
class Variable;
struct ContentType;

class Image {
 public:
  // Images that are in the process of being transformed are represented by an
  // Image.  This class encapsulates various operations that are sensitive to
  // the format of the compressed image file and of the image libraries we are
  // using.  In particular, the timing of compression and decompression
  // operations may be a bit unexpected, because we may do these operations
  // early in order to retrieve image metadata, or we may choose to skip them
  // entirely if we don't need them or don't understand how to do them.
  //
  // In future we may need to plumb this to other data sources or change how
  // metadata is retrieved; the object is to do so locally in this class without
  // disrupting any of its clients.

  struct ConversionBySourceVariable {
    ConversionBySourceVariable()
        : timeout_count(NULL),
          success_ms(NULL),
          failure_ms(NULL) {}

    Variable* timeout_count;  // # of timed-out conversions.
    Histogram* success_ms;    // Successful conversion duration.
    Histogram* failure_ms;    // Failed (and non-timed-out) conversion duration.
  };

  struct ConversionVariables {
    enum VariableType {
      FROM_UNKNOWN_FORMAT = 0,
      FROM_GIF,
      FROM_PNG,
      FROM_JPEG,
      OPAQUE,
      NONOPAQUE,
      FROM_GIF_ANIMATED,
      NUM_VARIABLE_TYPE
    };
    ConversionBySourceVariable* Get(VariableType var_type) {
      if ((var_type < FROM_UNKNOWN_FORMAT) ||
          (var_type >= NUM_VARIABLE_TYPE)) {
        return NULL;
      }
      return &(vars[var_type]);
    }

    ConversionBySourceVariable vars[NUM_VARIABLE_TYPE];
  };

  struct CompressionOptions {
    CompressionOptions()
        : preferred_webp(pagespeed::image_compression::WEBP_NONE),
          allow_webp_alpha(false),
          allow_webp_animated(false),
          webp_quality(RewriteOptions::kDefaultImageRecompressQuality),
          webp_animated_quality(RewriteOptions::kDefaultImageRecompressQuality),
          jpeg_quality(RewriteOptions::kDefaultImageRecompressQuality),
          progressive_jpeg_min_bytes(
              RewriteOptions::kDefaultProgressiveJpegMinBytes),
          progressive_jpeg(false),
          convert_gif_to_png(false),
          convert_png_to_jpeg(false),
          convert_jpeg_to_webp(false),
          recompress_jpeg(false),
          recompress_png(false),
          recompress_webp(false),
          retain_color_profile(false),
          retain_color_sampling(false),
          retain_exif_data(false),
          use_transparent_for_blank_image(false),
          jpeg_num_progressive_scans(
              RewriteOptions::kDefaultImageJpegNumProgressiveScans),
          webp_conversion_timeout_ms(-1),
          conversions_attempted(0),
          preserve_lossless(false),
          webp_conversion_variables(NULL) {}

    // These options are set by the client to specify what type of
    // conversion to perform:
    pagespeed::image_compression::PreferredLibwebpLevel preferred_webp;
    bool allow_webp_alpha;
    bool allow_webp_animated;
    int64 webp_quality;
    int64 webp_animated_quality;
    int64 jpeg_quality;
    int64 progressive_jpeg_min_bytes;
    bool progressive_jpeg;
    bool convert_gif_to_png;
    bool convert_png_to_jpeg;
    bool convert_jpeg_to_webp;
    bool recompress_jpeg;
    bool recompress_png;
    bool recompress_webp;
    bool retain_color_profile;
    bool retain_color_sampling;
    bool retain_exif_data;
    bool use_transparent_for_blank_image;
    int64 jpeg_num_progressive_scans;
    int64 webp_conversion_timeout_ms;

    // These fields are set by the conversion routines to report
    // characteristics of the conversion process.
    int conversions_attempted;
    bool preserve_lossless;

    ConversionVariables* webp_conversion_variables;
  };

  virtual ~Image();

  // static method to convert image type to content type.
  static const ContentType* TypeToContentType(ImageType t);

  // Stores the image dimensions in natural_dim (on success, sets
  // natural_dim->{width, height} and
  // ImageUrlEncoder::HasValidDimensions(natural_dim) == true).  This
  // method can fail (ImageUrlEncoder::HasValidDimensions(natural_dim)
  // == false) for various reasons: we don't understand the image
  // format, we can't find the headers, the library doesn't support a
  // particular encoding, etc.  In that case the other fields are left
  // alone.
  virtual void Dimensions(ImageDim* natural_dim) = 0;

  // Returns the size of original input in bytes.
  size_t input_size() const {
    return original_contents_.size();
  }

  // Returns the size of output image in bytes.
  size_t output_size() {
    size_t ret;
    if (output_valid_ || ComputeOutputContents()) {
      ret = output_contents_.size();
    } else {
      ret = input_size();
    }
    return ret;
  }

  ImageType image_type() {
    if (image_type_ == IMAGE_UNKNOWN) {
      ComputeImageType();
    }
    return image_type_;
  }

  // Changes the size of the image to the given width and height.  This will run
  // image processing on the image, and return false if the image processing
  // fails.  Otherwise the image contents and type can change.
  virtual bool ResizeTo(const ImageDim& new_dim) = 0;

  // Enable the transformation to low res image. If low res image is enabled,
  // all jpeg images are transformed to low quality jpeg images and all webp
  // images to low quality webp images, if possible.
  virtual void SetTransformToLowRes() = 0;

  // Returns image-appropriate content type, or NULL if no content type is
  // known.  Result is a top-level const pointer and should not be deleted etc.
  const ContentType* content_type() {
    return TypeToContentType(image_type());
  }

  // Returns the best known image contents.  If image type is not understood,
  // then Contents() will have NULL data().
  StringPiece Contents();

  // Draws the given image on top of this one at the given offset.  Returns true
  // if successful.
  virtual bool DrawImage(Image* image, int x, int y) = 0;

  // Attempts to decode this image and load its raster into memory.  If this
  // returns false, future calls to DrawImage and ResizeTo will fail.
  //
  // If output_useful is true, the decoded version might be written out
  // directly to user, so it may be worthwhile to make it efficient.
  virtual bool EnsureLoaded(bool output_useful) = 0;

  // Returns the image URL.
  virtual const GoogleString& url() = 0;

  // Returns the debug message.
  virtual const GoogleString& debug_message() = 0;

  // Returns the resized image debug message.
  virtual const GoogleString& resize_debug_message() = 0;

  // Sets the URL to be printed in debug messages.
  virtual void SetDebugMessageUrl(const GoogleString& url) = 0;

 protected:
  explicit Image(const StringPiece& original_contents);
  explicit Image(ImageType type);

  // Internal helpers
  virtual void ComputeImageType() = 0;
  virtual bool ComputeOutputContents() = 0;

  // Inject desired resized dimensions directly for testing.
  virtual void SetResizedDimensions(const ImageDim& dim) = 0;

  // Determines whether it's a good idea to convert this image to progressive
  // jpeg.
  virtual bool ShouldConvertToProgressive(int64 quality) const = 0;


  ImageType image_type_;  // Lazily initialized, initially IMAGE_UNKNOWN.
  const StringPiece original_contents_;
  GoogleString output_contents_;  // Lazily filled.
  bool output_valid_;             // Indicates output_contents_ now correct.
  bool rewrite_attempted_;        // Indicates if we tried rewriting for this.

 private:
  friend class ImageTestingPeer;
  friend class ImageTest;

  DISALLOW_COPY_AND_ASSIGN(Image);
};

// Image owns none of its inputs.  All of the arguments to NewImage(...) (the
// original_contents in particular) must outlive the Image object itself.  The
// intent is that an Image is created in a scoped fashion from an existing known
// resource.
//
// The options should be set via Image::SetOptions after construction, before
// the image is used for anything but determining its natural dimension size.
//
// TODO(jmarantz): It would seem natural to fold the ImageOptions into the
// Image object itself.
Image* NewImage(const StringPiece& original_contents,
                const GoogleString& url,
                const StringPiece& file_prefix,
                Image::CompressionOptions* options,
                Timer* timer,
                MessageHandler* handler);

// Creates a blank image of the given dimensions and type.
// For now, this is assumed to be an 8-bit 4-channel image transparent image.
Image* BlankImageWithOptions(int width, int height, ImageType type,
                             const StringPiece& tmp_dir,
                             Timer* timer,
                             MessageHandler* handler,
                             Image::CompressionOptions* options);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_H_
