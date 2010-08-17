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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_H_

#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "third_party/opencv/src/opencv/include/opencv/cv.h"

namespace net_instaweb {

struct ContentType;
class FileSystem;
class MessageHandler;
class Writer;

// The following four helper functions were moved here for testability.  We ran
// into problems with sign extension under different compiler versions, and we'd
// like to catch regressions on that front in the future.

// char to int *without sign extension*.
inline int charToInt(char c) {
  uint8 uc = static_cast<uint8>(c);
  return static_cast<int>(uc);
}

inline int JpegIntAtPosition(const StringPiece& buf, size_t pos) {
  return (charToInt(buf[pos]) << 8) |
         (charToInt(buf[pos + 1]));
}

inline int GifIntAtPosition(const StringPiece& buf, size_t pos) {
  return (charToInt(buf[pos + 1]) << 8) |
         (charToInt(buf[pos]));
}

inline int PngIntAtPosition(const StringPiece& buf, size_t pos) {
  return (charToInt(buf[pos    ]) << 24) |
         (charToInt(buf[pos + 1]) << 16) |
         (charToInt(buf[pos + 2]) << 8) |
         (charToInt(buf[pos + 3]));
}

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

  // Image owns none of its inputs.  All of the arguments to Image(...) (the
  // original_contents in particular) must outlive the Image object itself.  The
  // intent is that an Image is created in a scoped fashion from an existing
  // known resource.
  Image(const StringPiece& original_contents,
        const std::string& url,
        const StringPiece& file_prefix,
        FileSystem* file_system,
        MessageHandler* handler);

  ~Image() {
    CleanOpenCV();
  }

  enum Type {
    IMAGE_UNKNOWN = 0,
    IMAGE_JPEG,
    IMAGE_PNG,
    IMAGE_GIF,
  };

  // Note that at the moment asking for image dimensions can be expensive as it
  // invokes an external library.  This method can fail (returning false) for
  // various reasons: we don't understand the image format (eg a gif), we can't
  // find the headers, the library doesn't support a particular encoding, etc.
  // In general, we deal with failure here by passing data through unaltered.
  bool Dimensions(int* width, int* height);

  int input_size() const {
    return original_contents_.size();
  }

  int output_size() {
    int ret;
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

  bool ResizeTo(int width, int height);

  // Method that lets us bail out if a resize actually cost space!
  void UndoResize() {
    if (resized_) {
      CleanOpenCV();
      output_valid_ = false;
      image_type_ = IMAGE_UNKNOWN;
      resized_ = false;
    }
  }

  // Return image-appropriate content type, or NULL if no content type is known.
  // Result is a top-level const pointer.
  const ContentType* content_type();

  // Returns the best known image contents.  If image type is not understood,
  // then Contents() will have NULL data().
  StringPiece Contents();
  // Encode contents directly into data_url if image type is understood
  bool AsInlineData(std::string* data_url);

 private:
  void ComputeImageType();
  void FindJpegSize();
  inline void FindPngSize();
  inline void FindGifSize();
  bool LoadOpenCV();
  void CleanOpenCV();
  bool ComputeOutputContents();

  friend class ImageTest;

  static const char kPngHeader[];
  static const size_t kPngHeaderLength;
  static const char kPngIHDR[];
  static const size_t kPngIHDRLength;
  static const size_t kIHDRDataStart;
  static const size_t kPngIntSize;

  static const char kGifHeader[];
  static const size_t kGifHeaderLength;
  static const size_t kGifDimStart;
  static const size_t kGifIntSize;

  static const size_t kJpegIntSize;

  std::string file_prefix_;
  FileSystem* file_system_;
  MessageHandler* handler_;
  Type image_type_;
  StringPiece original_contents_;
  std::string output_contents_;
  bool output_valid_;
  std::string opencv_filename_;
  IplImage* opencv_image_;
  bool opencv_load_possible_;
  bool resized_;
  const std::string& url_;
  int width_, height_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_H_
