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

#include <vector>

#include "base/basictypes.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#ifdef USE_SYSTEM_OPENCV
#include "cv.h"
#else
#include "third_party/opencv/src/opencv/include/opencv/cv.h"
#endif

#if (CV_MAJOR_VERSION == 2 && CV_MINOR_VERSION >= 1) || (CV_MAJOR_VERSION > 2)
#define USE_OPENCV_2_1
#endif

namespace net_instaweb {

struct ContentType;
class FileSystem;
class MessageHandler;
class Writer;

// The following four helper functions were moved here for testability.  We ran
// into problems with sign extension under different compiler versions, and we'd
// like to catch regressions on that front in the future.

// char to int *without sign extension*.
inline int CharToInt(char c) {
  uint8 uc = static_cast<uint8>(c);
  return static_cast<int>(uc);
}

inline int JpegIntAtPosition(const StringPiece& buf, size_t pos) {
  return (CharToInt(buf[pos]) << 8) |
         (CharToInt(buf[pos + 1]));
}

inline int GifIntAtPosition(const StringPiece& buf, size_t pos) {
  return (CharToInt(buf[pos + 1]) << 8) |
         (CharToInt(buf[pos]));
}

inline int PngIntAtPosition(const StringPiece& buf, size_t pos) {
  return (CharToInt(buf[pos    ]) << 24) |
         (CharToInt(buf[pos + 1]) << 16) |
         (CharToInt(buf[pos + 2]) << 8) |
         (CharToInt(buf[pos + 3]));
}

inline bool PngSectionIdIs(const char* hdr,
                           const StringPiece& buf, size_t pos) {
  return ((buf[pos + 4] == hdr[0]) &&
          (buf[pos + 5] == hdr[1]) &&
          (buf[pos + 6] == hdr[2]) &&
          (buf[pos + 7] == hdr[3]));
}

namespace ImageHeaders {
  // Constants that are shared by Image and its tests.
  extern const char kPngHeader[];
  extern const size_t kPngHeaderLength;
  extern const char kPngIHDR[];
  extern const size_t kPngIHDRLength;
  extern const size_t kIHDRDataStart;
  extern const size_t kPngIntSize;

  extern const char kGifHeader[];
  extern const size_t kGifHeaderLength;
  extern const size_t kGifDimStart;
  extern const size_t kGifIntSize;

  extern const size_t kJpegIntSize;
}  // namespace ImageHeaders

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

  enum Type {
    IMAGE_UNKNOWN = 0,
    IMAGE_JPEG,
    IMAGE_PNG,
    IMAGE_GIF,
  };

  // Image owns none of its inputs.  All of the arguments to Image(...) (the
  // original_contents in particular) must outlive the Image object itself.  The
  // intent is that an Image is created in a scoped fashion from an existing
  // known resource.
  Image(const StringPiece& original_contents,
        const std::string& url,
        const StringPiece& file_prefix,
        MessageHandler* handler);

  // Creates a blank image of the given dimensions and type.
  // For now, this is assumed to be an 8-bit 3-channel image.
  Image(int width, int height, Type type,
        const StringPiece& tmp_dir, MessageHandler* handler);

  ~Image();

  // Stores the image dimensions in natural_dim (on success, sets
  // natural_dim->{width, height} and
  // ImageUrlEncoder::HasValidDimensions(natural_dim) == true).  This
  // method can fail (ImageUrlEncoder::HasValidDimensions(natural_dim)
  // == false) for various reasons: we don't understand the image
  // format, we can't find the headers, the library doesn't support a
  // particular encoding, etc.  In that case the other fields are left
  // alone.
  void Dimensions(ImageDim* natural_dim);

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

  Type image_type() {
    if (image_type_ == IMAGE_UNKNOWN) {
      ComputeImageType();
    }
    return image_type_;
  }

  // Changes the size of the image to the given width and height.  This will run
  // image processing on the image, and return false if the image processing
  // fails.  Otherwise the image contents and type can change.
  bool ResizeTo(const ImageDim& new_dim);

  // Returns image-appropriate content type, or NULL if no content type is
  // known.  Result is a top-level const pointer and should not be deleted etc.
  const ContentType* content_type();

  // Returns the best known image contents.  If image type is not understood,
  // then Contents() will have NULL data().
  StringPiece Contents();

  // Draws the given image on top of this one at the given offset.  Returns true
  // if successful.
  bool DrawImage(Image* image, int x, int y);

  // Attempts to decode this image and load its raster into memory.  If this
  // returns false, future calls to DrawImage and ResizeTo will fail.
  bool EnsureLoaded() { return LoadOpenCv(); }

 private:
  // byte buffer type most convenient for working with given OpenCV version
#ifdef USE_OPENCV_2_1
  typedef std::vector<unsigned char> OpenCvBuffer;
#else
  typedef std::string OpenCvBuffer;
#endif

  // Internal helper used only in image.cc.
  static bool ComputePngTransparency(const StringPiece& buf);

  // Internal methods used only in image.cc (see there for more).
  void UndoChange();
  void ComputeImageType();
  void FindJpegSize();
  inline void FindPngSize();
  inline void FindGifSize();
  bool HasTransparency(const StringPiece& buf);
  bool LoadOpenCv();
  void CleanOpenCv();
  bool ComputeOutputContents();

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
                        std::string* filename);
#endif

  const std::string file_prefix_;
  MessageHandler* handler_;
  Type image_type_;  // Lazily initialized, initially IMAGE_UNKNOWN.
  const StringPiece original_contents_;
  std::string output_contents_;  // Lazily filled.
  bool output_valid_;             // Indicates output_contents_ now correct.
  IplImage* opencv_image_;        // Lazily filled on OpenCV load.
  bool opencv_load_possible_;     // Attempt opencv_load in future?
  bool changed_;
  const std::string url_;
  ImageDim dims_;

  DISALLOW_COPY_AND_ASSIGN(Image);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_H_
