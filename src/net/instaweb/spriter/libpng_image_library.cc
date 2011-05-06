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

#include <errno.h>
#include <png.h>
#include "net/instaweb/spriter/libpng_image_library.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

// We always output at RGBA with 8 bits per channel.
const int BYTES_PER_PIXEL = 4;
// Largest PNG dimension (width or height) that we will attempt to process.
const int MAX_PNG_DIMENSION = 4096;

}  // namespace
namespace net_instaweb {
namespace spriter {

LibpngImageLibrary::Image::Image(ImageLibraryInterface* lib,
                                 png_structp png_struct, png_infop png_info,
                                 png_bytep* rows)
    : ImageLibraryInterface::Image(lib),
      png_struct_(png_struct), png_info_(png_info), rows_(rows) {
}
LibpngImageLibrary::Image::~Image() {
  int width, height;
  GetDimensions(&width, &height);
  png_destroy_read_struct(&png_struct_, &png_info_, NULL);
  for (int i = height - 1; i >= 0; i--) {
    delete[] rows_[i];
  }
  delete[] rows_;
}
bool LibpngImageLibrary::Image::GetDimensions(int* out_width, int* out_height)
    const {
  *out_width = png_get_image_width(png_struct_, png_info_);
  *out_height = png_get_image_height(png_struct_, png_info_);
  return true;
}
const png_bytep* LibpngImageLibrary::Image::Rows() const {
  return rows_;
}

bool LibpngImageLibrary::Canvas::DrawImage(
    const ImageLibraryInterface::Image* image, int x_start, int y_start) {
  int w, h;
  image->GetDimensions(&w, &h);
  if ((w <= 0) || (h <= 0)) {
    return true;
  }
  const png_bytep* rows = static_cast<const Image*>(image)->Rows();
  int their_y = h - 1;
  int my_y = y_start + h - 1;
  int x_start_byte = x_start * BYTES_PER_PIXEL;
  int num_bytes = w * BYTES_PER_PIXEL;
  CHECK(x_start >= 0);
  CHECK(y_start >= 0);
  CHECK(x_start + w <= width_);
  CHECK(y_start + h <= height_);
  while (their_y >= 0) {
    memcpy(rows_[my_y] + x_start_byte, rows[their_y], num_bytes);
    their_y--;
    my_y--;
  }
  return true;
}

bool LibpngImageLibrary::Canvas::WriteToFile(const FilePath& filename,
                                             ImageFormat format) {
  GoogleString write_path = StrCat(base_out_path_, filename);
  FILE* file = fopen(write_path.c_str(), "wb");
  if (file == NULL) {
    delegate_.OnError(
        StrCat("Writing image " , write_path, ": ", strerror(errno)));
    fclose(file);
    return false;
  }

  png_structp png_struct = png_create_write_struct(
      PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png_struct == NULL) {
    delegate_.OnError(
        StrCat("Writing image " , write_path, ": cannot create png struct"));
    fclose(file);
    return false;
  }
  png_infop png_info = png_create_info_struct(png_struct);
  if (png_info == NULL) {
    delegate_.OnError(
        StrCat("Writing image " , write_path, ": cannot create png info"));
    png_destroy_write_struct(&png_struct, &png_info);
    fclose(file);
    return false;
  }
  if (setjmp(png_jmpbuf(png_struct))) {
    delegate_.OnError(
        StrCat("Writing image " , write_path, ": cannot initialize libpng"));
    png_destroy_write_struct(&png_struct, &png_info);
    fclose(file);
    return false;
  }
  png_init_io(png_struct, file);
  if (setjmp(png_jmpbuf(png_struct))) {
    delegate_.OnError(
        StrCat("Writing image " , write_path, ": cannot write header"));
    png_destroy_write_struct(&png_struct, &png_info);
    fclose(file);
    return false;
  }
  const png_byte bit_depth = 8;
  const png_byte color_type = PNG_COLOR_TYPE_RGB_ALPHA;
  png_set_IHDR(png_struct, png_info, width_, height_,
               bit_depth, color_type, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  png_write_info(png_struct, png_info);

  if (setjmp(png_jmpbuf(png_struct))) {
    delegate_.OnError(
        StrCat("Writing image " , write_path, ": cannot write body"));
    png_destroy_write_struct(&png_struct, &png_info);
    fclose(file);
    return false;
  }
  png_write_image(png_struct, rows_);

  if (setjmp(png_jmpbuf(png_struct))) {
    delegate_.OnError(
        StrCat("Writing image " , write_path, ": cannot write end"));
    fclose(file);
    return false;
  }
  png_write_end(png_struct, NULL);
  png_destroy_write_struct(&png_struct, &png_info);
  if (fclose(file) != 0) {
    delegate_.OnError(
        StrCat("Writing image " , write_path, ": ", strerror(errno)));
    return false;
  }
  return true;
}
LibpngImageLibrary::Canvas::Canvas(ImageLibraryInterface* lib,
                                   const Delegate& d,
                                   const GoogleString& base_out_path,
                                   int width, int height)
    : ImageLibraryInterface::Canvas(lib), delegate_(d),
      base_out_path_(base_out_path), width_(width), height_(height) {
  rows_ = new png_bytep[height];
  for (int i = height - 1; i >= 0; i--) {
    rows_[i] = new png_byte[width * BYTES_PER_PIXEL];
    memset(rows_[i], 0, width * BYTES_PER_PIXEL);
  }
}
LibpngImageLibrary::Canvas::~Canvas() {
  for (int i = height_ - 1; i >= 0; i--) {
    delete[] rows_[i];
  }
  delete[] rows_;
}
ImageLibraryInterface::Canvas* LibpngImageLibrary::CreateCanvas(int width,
                                                                int height) {
  return new Canvas(this, *delegate(), base_output_path(), width, height);
}


// Read an image from disk.  Return NULL (after calling delegate
// method) on error.  Caller owns the returned pointer.
ImageLibraryInterface::Image* LibpngImageLibrary::ReadFromFile(
    const FilePath& filename) {
  GoogleString path = StrCat(base_input_path(), filename);
  FILE* file = fopen(path.c_str(), "rb");
  if (file == NULL) {
    delegate()->OnError(StrCat("Reading image " , path, ": ", strerror(errno)));
    return NULL;
  }
  png_byte header[8];
  if (fread(header, 1, 8, file) != 8) {
    delegate()->OnError(StrCat("Image " , path, " has no header."));
    fclose(file);
    return NULL;
  }
  if (png_sig_cmp(header, 0, 8) != 0) {
    delegate()->OnError(StrCat("Image " , path, " not PNG."));
    fclose(file);
    return NULL;
  }
  png_structp png_struct = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                                  NULL, NULL, NULL);
  png_infop png_info = png_create_info_struct(png_struct);
  if (!png_info) {
    png_destroy_read_struct(&png_struct, NULL, NULL);
    delegate()->OnError(StrCat("Image " , path, " could not create png_info"));
    return NULL;
  }
  if (setjmp(png_jmpbuf(png_struct))) {
    png_destroy_read_struct(&png_struct, &png_info, NULL);
    fclose(file);
    delegate()->OnError(StrCat("Image " , path, " could not be decoded."));
    return NULL;
  }
  png_init_io(png_struct, file);
  png_set_sig_bytes(png_struct, 8);
  png_read_info(png_struct, png_info);
  int width = png_get_image_width(png_struct, png_info);
  int height = png_get_image_height(png_struct, png_info);
  if ((width > MAX_PNG_DIMENSION) || (height > MAX_PNG_DIMENSION)) {
    fclose(file);
    delegate()->OnError(StrCat("Image " , path, " is too big."));
    return NULL;
  }
  if ((width <= 0) || (height <= 0)) {
    delegate()->OnError(StrCat("Image " , path, " has nonpositive dimension."));
    return NULL;
  }

  // Expand the image to 8-bit RGBA.  Code taken from libpng manpage.
  int color_type = png_get_color_type(png_struct, png_info);
  int bit_depth = png_get_bit_depth(png_struct, png_info);
  if (color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_palette_to_rgb(png_struct);
  }
  if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth < 8)) {
    png_set_gray_1_2_4_to_8(png_struct);
  }
  if (png_get_valid(png_struct, png_info, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png_struct);
  }
  if (bit_depth == 16) {
    png_set_strip_16(png_struct);
  }
  if (bit_depth < 8) {
    png_set_packing(png_struct);
  }
  if ((color_type == PNG_COLOR_TYPE_RGB) ||
      (color_type == PNG_COLOR_TYPE_GRAY)) {
    png_set_add_alpha(png_struct, 0xff, PNG_FILLER_AFTER);
  }
  if ((color_type == PNG_COLOR_TYPE_GRAY) ||
      (color_type == PNG_COLOR_TYPE_GRAY_ALPHA)) {
        png_set_gray_to_rgb(png_struct);
  }
  // TODO(abliss): what to do with background color and gamma?
  png_read_update_info(png_struct, png_info);
  png_bytep* rows = new png_bytep[height];
  for (int i = height - 1; i >= 0; i--) {
    rows[i] = new png_byte[width * BYTES_PER_PIXEL];
  }
  png_read_image(png_struct, rows);
  png_read_end(png_struct, png_info);
  fclose(file);
  return new Image(this, png_struct, png_info, rows);
}


LibpngImageLibrary::LibpngImageLibrary(const FilePath& base_input_path,
                                       const FilePath& base_output_path,
                                       Delegate* delegate)
    : ImageLibraryInterface(base_input_path, base_output_path, delegate) {
}


}  // namespace spriter
}  // namespace net_instaweb
