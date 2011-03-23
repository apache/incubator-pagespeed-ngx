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

// Author: skerner@google.com (Sam Kerner)

#ifndef NET_INSTAWEB_SPRITER_IMAGE_LIBRARY_INTERFACE_H_
#define NET_INSTAWEB_SPRITER_IMAGE_LIBRARY_INTERFACE_H_

#include "base/basictypes.h"
#include <string>
#include "net/instaweb/spriter/public/image_spriter.pb.h"

// TODO(skerner): #include image_spriter.pb.h is needed to allow use of
// enum ImageFormat.  Break this dependency and remove the include.

namespace net_instaweb {
namespace spriter {

// Class ImageLibraryInterface provides an abstract interface to manipulate
// images.  This interface hides the details of any particular image
// processing library.  This makes it easy to port to an environment
// where a different image processing library is preferred.
//
// Subclasses create methods that act on two types of objects:
// * Image: An imitable rectangular region of pixels.
// * Canvas: A mutable rectangular region of pixels.

class ImageLibraryInterface {
 public:
  // TODO(skerner): Chromium's base lib has a class FilePath that
  // handles paths in a cross-platform way.  Use it.
  typedef std::string FilePath;

  // Users of the image library interface provide a delegate which
  // is informend of errors.
  class Delegate {
   public:
    virtual void OnError(const std::string& error) = 0;
    virtual ~Delegate() {}
  };

  // Images are immutable rectangular regions of pixels.
  class Image {
   public:
    // Get the width and height of an image.
    virtual bool GetDimensions(int* out_width, int* out_height) = 0;
    virtual ~Image() {}
   protected:
    // Only methods of ImageLibraryInterface may create images.
    explicit Image(ImageLibraryInterface* lib) : lib_(lib) {}
    ImageLibraryInterface* lib_;
   private:
    DISALLOW_COPY_AND_ASSIGN(Image);
  };

  // Read an image from disk.  Return NULL (after calling delegate
  // method) on error.  Caller owns the returned pointer.
  virtual Image* ReadFromFile(const FilePath& path) = 0;

  // Canvases are mutable rectangles onto which a program may draw.
  // For now, we support stamping images into a canvas, and writing
  // a canvas to a file.
  class Canvas {
   public:
    virtual bool DrawImage(const Image* image, int x, int y) = 0;
    virtual bool WriteToFile(
        const FilePath& write_path, ImageFormat format) = 0;
    virtual ~Canvas() {}
   protected:
    explicit Canvas(ImageLibraryInterface* lib) : lib_(lib) {}
    ImageLibraryInterface* lib_;
   private:
    DISALLOW_COPY_AND_ASSIGN(Canvas);
  };

  virtual Canvas* CreateCanvas(int width, int height) = 0;

  // Constructor for custom subclasses.  Prefer to use
  // ImageLibraryInterfaceFactory() if possible.
  ImageLibraryInterface(Delegate* delegate)
      : delegate_(delegate) {
  }

  virtual ~ImageLibraryInterface() {}

  // Use this factory method to get a usable image library object.
  static ImageLibraryInterface* ImageLibraryInterfaceFactory(
      const std::string& library_name);

 protected:
  // Use ImageLibraryInterfaceFactory() to access an image library.
  ImageLibraryInterface(const FilePath& base_input_path,
                        const FilePath& base_output_path,
                        Delegate* delegate);

  // Used by subclasses:
  const FilePath& base_input_path() { return base_input_path_; }
  const FilePath& base_output_path() { return base_output_path_; }
  const Delegate* delegate() { return delegate_; }

 private:
  // File path under which all read operations (such as reading an image)
  // are done.
  FilePath base_input_path_;

  // File path under which all write operations (such as writing a canvas)
  // are done.
  FilePath base_output_path_;

  // |delegate_|'s methods are called when an exceptional event (such as an
  // error) has occurred.
  const Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ImageLibraryInterface);
};

}  // namespace spriter
}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SPRITER_IMAGE_LIBRARY_INTERFACE_H_
