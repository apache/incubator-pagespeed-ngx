/*
 * Copyright 2011 Google Inc.
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
// Author: abliss@google.com (Adam Bliss)

#ifndef NET_INSTAWEB_SPRITER_LIBPNG_IMAGE_LIBRARY_H_
#define NET_INSTAWEB_SPRITER_LIBPNG_IMAGE_LIBRARY_H_

#include <png.h>
#include "net/instaweb/spriter/image_library_interface.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {
namespace spriter {

/**
 * An implementation of the ImageLibraryInterface using direct calls to libpng.
 * The advantage of this over OpenCV is that it can handle transparency.  The
 * output of this library is always an RGBA PNG with 8 bits per channel (so
 * 16-bit images will be degraded).
 */
class LibpngImageLibrary : public ImageLibraryInterface {
 public:
  LibpngImageLibrary(const FilePath& base_input_path,
                     const FilePath& base_output_path,
                     Delegate* delegate);
  virtual ~LibpngImageLibrary() {}

 protected:
  // Images are immutable rectangular regions of pixels.
  class Image : public ImageLibraryInterface::Image {
   public:
    // Takes ownership of rows.
    Image(ImageLibraryInterface* lib,
          png_structp png_struct, png_infop png_info, png_bytep* rows);
    virtual ~Image();

    virtual bool GetDimensions(int* out_width, int* out_height) const;
    const png_bytep* Rows() const;

   private:
    png_structp png_struct_;
    png_infop png_info_;
    png_bytep* rows_;
    DISALLOW_COPY_AND_ASSIGN(Image);
  };

  // Read an image from disk.  Return NULL (after calling delegate
  // method) on error.  Caller owns the returned pointer.
  virtual ImageLibraryInterface::Image* ReadFromFile(const FilePath& path);

  // Canvases are mutable rectangles onto which a program may draw.
  // For now, we support stamping images into a canvas, and writing
  // a canvas to a file.
  class Canvas : public ImageLibraryInterface::Canvas {
   public:
    Canvas(ImageLibraryInterface* lib, const Delegate* d,
           const GoogleString& base_out_path,
           int width, int height);
    virtual ~Canvas();
    virtual bool DrawImage(const ImageLibraryInterface::Image* image, int x,
                           int y);
    virtual bool WriteToFile(const FilePath& write_path, ImageFormat format);

   private:
    const Delegate* delegate_;
    const GoogleString base_out_path_;
    int width_;
    int height_;
    png_bytep* rows_;
    DISALLOW_COPY_AND_ASSIGN(Canvas);
  };

  virtual ImageLibraryInterface::Canvas* CreateCanvas(int width, int height);

 private:
  friend class LibpngImageLibraryTest;
  DISALLOW_COPY_AND_ASSIGN(LibpngImageLibrary);
};

}  // namespace spriter
}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SPRITER_LIBPNG_IMAGE_LIBRARY_H_
