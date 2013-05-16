/*
 * Copyright 2009 Google Inc.
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

// Author: Bryan McQuade

#include "pagespeed/kernel/image/gif_reader.h"

#include "pagespeed/kernel/base/scoped_ptr.h"

extern "C" {
#ifdef USE_SYSTEM_LIBPNG
#include "png.h"                                               // NOLINT
#else
#include "third_party/libpng/png.h"

// gif_reader currently inspects "private" fields of the png_info
// structure. Thus we need to include pnginfo.h. Eventually we should
// fix the callsites to use the public API only, and remove this
// include.
#if PNG_LIBPNG_VER >= 10400
#include "third_party/libpng/pnginfo.h"
#endif
#endif

#include "third_party/giflib/lib/gif_lib.h"
}

using pagespeed::image_compression::ScopedPngStruct;

namespace {

// GIF interlace tables.
static const int kInterlaceOffsets[] = { 0, 4, 2, 1 };
static const int kInterlaceJumps[] = { 8, 8, 4, 2 };

// Flag used to indicate that a gif extension contains transparency
// information.
static const unsigned char kTransparentFlag = 0x01;

struct GifInput {
  const GoogleString* data_;
  int offset_;
};

// Some older versions of gcc (4.2.x) have trouble with certain setjmp
// declarations. To address this gcc issue, we factor a few setjmp()
// invocations into their own narrowly scoped methods, so gcc can
// process them properly.
bool ProtectedPngSetIhdr(
    png_structp png_ptr, png_infop info_ptr,
    png_uint_32 width, png_uint_32 height, int bit_depth, int color_type,
    int interlace_method, int compression_method, int filter_method) {
  if (setjmp(png_jmpbuf(png_ptr))) {
    return false;
  }
  png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth, color_type,
               interlace_method, compression_method, filter_method);
  return true;
}

bool ProtectedPngSetPlte(png_structp png_ptr, png_infop info_ptr,
                         png_colorp palette, int num_palette) {
  if (setjmp(png_jmpbuf(png_ptr))) {
    return false;
  }
  png_set_PLTE(png_ptr, info_ptr, palette, num_palette);
  return true;
}

int ReadGifFromStream(GifFileType* gif_file, GifByteType* data, int length) {
  GifInput* input = reinterpret_cast<GifInput*>(gif_file->UserData);
  size_t copied = input->data_->copy(reinterpret_cast<char*>(data), length,
                                     input->offset_);
  input->offset_ += copied;
  return copied;
}

bool AddTransparencyChunk(png_structp png_ptr,
                          png_infop info_ptr,
                          int transparent_palette_index) {
  const int num_trans = transparent_palette_index + 1;
  if (num_trans <= 0 || num_trans > info_ptr->num_palette) {
    LOG(INFO) << "Transparent palette index out of bounds.";
    return false;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    return false;
  }
  // TODO(huibao): to optimize tRNS size, could move transparent index to
  // the head of the palette.
  png_byte trans[256];
  // First, set all palette indices to fully opaque.
  memset(trans, 0xff, num_trans);
  // Set the one transparent index to fully transparent.
  trans[transparent_palette_index] = 0;
  png_set_tRNS(png_ptr, info_ptr, trans, num_trans, NULL);
  return true;
}

bool ReadImageDescriptor(GifFileType* gif_file,
                         png_structp png_ptr,
                         png_infop info_ptr,
                         png_color* palette) {
  if (DGifGetImageDesc(gif_file) == GIF_ERROR) {
    DLOG(INFO) << "Failed to get image descriptor.";
    return false;
  }
  if (gif_file->ImageCount != 1) {
    DLOG(INFO) << "Unable to optimize image with "
               << gif_file->ImageCount << " frames.";
    return false;
  }
  const GifWord row = gif_file->Image.Top;
  const GifWord pixel = gif_file->Image.Left;
  const GifWord width = gif_file->Image.Width;
  const GifWord height = gif_file->Image.Height;

  // Validate coordinates.
  if (pixel + width > gif_file->SWidth ||
      row + height > gif_file->SHeight) {
    DLOG(INFO) << "Image coordinates outside of resolution.";
    return false;
  }

  // Populate the color map.
  ColorMapObject* color_map =
      gif_file->Image.ColorMap != NULL ?
      gif_file->Image.ColorMap : gif_file->SColorMap;

  if (color_map == NULL) {
    DLOG(INFO) << "Failed to find color map.";
    return false;
  }

  if (color_map->ColorCount < 0 || color_map->ColorCount > 256) {
    DLOG(INFO) << "Invalid color count " << color_map->ColorCount;
    return false;
  }
  for (int i = 0; i < color_map->ColorCount; ++i) {
    palette[i].red = color_map->Colors[i].Red;
    palette[i].green = color_map->Colors[i].Green;
    palette[i].blue = color_map->Colors[i].Blue;
  }
  if (!ProtectedPngSetPlte(png_ptr, info_ptr, palette, color_map->ColorCount)) {
    return false;
  }

  if (gif_file->Image.Interlace == 0) {
    // Not interlaced. Read each line into the PNG buffer.
    for (GifWord i = 0; i < height; ++i) {
      if (DGifGetLine(gif_file,
                      static_cast<GifPixelType*>(
                          &info_ptr->row_pointers[row + i][pixel]),
                      width) == GIF_ERROR) {
        DLOG(INFO) << "Failed to DGifGetLine";
        return false;
      }
    }
  } else {
    // Need to deinterlace. The deinterlace code is based on algorithm
    // in giflib.
    for (int i = 0; i < 4; ++i) {
      for (int j = row + kInterlaceOffsets[i];
           j < row + height;
           j += kInterlaceJumps[i]) {
        if (DGifGetLine(gif_file,
                        static_cast<GifPixelType*>(
                            &info_ptr->row_pointers[j][pixel]),
                        width) == GIF_ERROR) {
          DLOG(INFO) << "Failed to DGifGetLine";
          return false;
        }
      }
    }
  }

  info_ptr->valid |= PNG_INFO_IDAT;
  return true;
}

// Read a GIF extension. There are various extensions. The only one we
// care about is the transparency extension, so we ignore all other
// extensions.
bool ReadExtension(GifFileType* gif_file,
                   png_structp png_ptr,
                   png_infop info_ptr,
                   int* out_transparent_index) {
  GifByteType* extension = NULL;
  int ext_code = 0;
  if (DGifGetExtension(gif_file, &ext_code, &extension) == GIF_ERROR) {
    DLOG(INFO) << "Failed to read extension.";
    return false;
  }

  // We only care about one extension type, the graphics extension,
  // which can contain transparency information.
  if (ext_code == GRAPHICS_EXT_FUNC_CODE) {
    // Make sure that the extension has the expected length.
    if (extension[0] < 4) {
      DLOG(INFO) << "Received graphics extension with unexpected length.";
      return false;
    }
    // The first payload byte contains the flags. Check to see if the
    // transparency flag is set.
    if ((extension[1] & kTransparentFlag) != 0) {
      if (*out_transparent_index >= 0) {
        // The transparent index has already been set. Ignore new
        // values.
        DLOG(INFO) << "Found multiple transparency entries. Using first entry.";
      } else {
        // We found a transparency entry. The transparent index is in
        // the 4th payload byte.
        *out_transparent_index = extension[4];
      }
    }
  }

  // Some extensions (i.e. the comment extension, the text extension)
  // allow multiple sub-blocks. However, the graphics extension can
  // contain only one sub-block (handled above). Since we only care
  // about the graphics extension, we can safely ignore all subsequent
  // blocks.
  while (extension != NULL) {
    if (DGifGetExtensionNext(gif_file, &extension) == GIF_ERROR) {
      DLOG(INFO) << "Failed to read next extension.";
      return false;
    }
  }

  return true;
}

png_uint_32 AllocatePngPixels(png_structp png_ptr,
                              png_infop info_ptr) {
  // Like libpng's png_read_png, we free the row pointers unless they
  // weren't allocated by libpng, in which case we reuse them.
  png_uint_32 row_size = png_get_rowbytes(png_ptr, info_ptr);
  if (row_size == 0) {
    return 0;
  }

#ifdef PNG_FREE_ME_SUPPORTED
  png_free_data(png_ptr, info_ptr, PNG_FREE_ROWS, 0);
#endif
  if (info_ptr->row_pointers == NULL) {
    // Allocate the array of pointers to each row.
    const png_size_t row_pointers_size =
        info_ptr->height * png_sizeof(png_bytep);
    info_ptr->row_pointers = static_cast<png_bytepp>(
        png_malloc(png_ptr, row_pointers_size));
    memset(info_ptr->row_pointers, 0, row_pointers_size);
#ifdef PNG_FREE_ME_SUPPORTED
    info_ptr->free_me |= PNG_FREE_ROWS;
#endif

    // Allocate memory for each row.
    for (png_uint_32 row = 0; row < info_ptr->height; ++row) {
      info_ptr->row_pointers[row] =
          static_cast<png_bytep>(png_malloc(png_ptr, row_size));
    }
  }
  return row_size;
}

bool ExpandColorMap(png_structp paletted_png_ptr,
                    png_infop paletted_info_ptr,
                    png_color* palette,
                    int transparent_palette_index,
                    png_structp rgb_png_ptr,
                    png_infop rgb_info_ptr) {
  png_uint_32 height = png_get_image_height(paletted_png_ptr,
                                            paletted_info_ptr);
  png_uint_32 width = png_get_image_width(paletted_png_ptr,
                                          paletted_info_ptr);
  bool have_alpha = (transparent_palette_index >= 0);
  if (setjmp(png_jmpbuf(rgb_png_ptr))) {
    return false;
  }
  png_set_IHDR(rgb_png_ptr,
               rgb_info_ptr,
               width,
               height,
               8,  // bit depth
               have_alpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB,
               PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_BASE,
               PNG_FILTER_TYPE_BASE);
  png_uint_32 row_size = AllocatePngPixels(rgb_png_ptr, rgb_info_ptr);
  png_byte bytes_per_pixel = have_alpha ? 4 : 3;

  if (row_size == 0) {
    return false;
  }

  for (png_uint_32 row = 0; row < height; ++row) {
    png_bytep rgb_next_byte = rgb_info_ptr->row_pointers[row];
    if (have_alpha) {
      // Make the row opaque initially.
      memset(rgb_next_byte, 0xff, row_size);
    }
    for (png_uint_32 column = 0; column < width; ++column) {
      png_byte palette_entry = paletted_info_ptr->row_pointers[row][column];
      if (have_alpha &&
          (palette_entry == static_cast<png_byte>(transparent_palette_index))) {
        // Transparent: clear RGBA bytes.
        memset(rgb_next_byte, 0x00, bytes_per_pixel);
      } else {
        // Opaque: Copy RGB, keeping A opaque from above
        memcpy(rgb_next_byte, &(palette[palette_entry]), 3);
      }
      rgb_next_byte += bytes_per_pixel;
    }
  }
  rgb_info_ptr->valid |= PNG_INFO_IDAT;
  return true;
}

bool ReadGifToPng(GifFileType* gif_file,
                  png_structp png_ptr,
                  png_infop info_ptr,
                  bool expand_colormap,
                  bool strip_alpha,
                  bool require_opaque) {
  if (static_cast<png_size_t>(gif_file->SHeight) >
      PNG_UINT_32_MAX/png_sizeof(png_bytep)) {
    DLOG(INFO) << "GIF image is too big to process.";
    return false;
  }

  // If expand_colormap is true, the color-indexed GIF_file is read
  // into paletted_png, and then transformed into a bona fide RGB(A)
  // PNG in png_ptr and info_ptr. If expand_colormap is false, we just
  // read the color-indexed GIF file directly into png_ptr and
  // info_ptr.
  scoped_ptr<ScopedPngStruct> paletted_png;
  png_structp paletted_png_ptr = NULL;
  png_infop paletted_info_ptr = NULL;

  if (expand_colormap) {
    // We read the image into a separate struct before expanding the
    // colormap.
    paletted_png.reset(new ScopedPngStruct(ScopedPngStruct::READ));
    if (!paletted_png->valid()) {
      LOG(DFATAL) << "Invalid ScopedPngStruct r: " << paletted_png->valid();
      return false;
    }
    paletted_png_ptr = paletted_png->png_ptr();
    paletted_info_ptr = paletted_png->info_ptr();
  } else {
    // We read the image directly into the pointers that was passed in.
    paletted_png_ptr = png_ptr;
    paletted_info_ptr = info_ptr;
  }

  if (!ProtectedPngSetIhdr(paletted_png_ptr,
                           paletted_info_ptr,
                           gif_file->SWidth,
                           gif_file->SHeight,
                           8,  // bit depth
                           PNG_COLOR_TYPE_PALETTE,
                           PNG_INTERLACE_NONE,
                           PNG_COMPRESSION_TYPE_BASE,
                           PNG_FILTER_TYPE_BASE)) {
    return false;
  }

  png_uint_32 row_size = AllocatePngPixels(paletted_png_ptr, paletted_info_ptr);
  if (row_size == 0) {
    return false;
  }

  // Fill the rows with the background color.
  memset(paletted_info_ptr->row_pointers[0],
         gif_file->SBackGroundColor, row_size);
  for (png_uint_32 row = 1; row < paletted_info_ptr->height; ++row) {
    memcpy(paletted_info_ptr->row_pointers[row],
           paletted_info_ptr->row_pointers[0],
           row_size);
  }

  int transparent_palette_index = -1;
  bool found_terminator = false;
  png_color palette[256];
  while (!found_terminator) {
    GifRecordType record_type = UNDEFINED_RECORD_TYPE;
    if (DGifGetRecordType(gif_file, &record_type) == GIF_ERROR) {
      DLOG(INFO) << "Failed to read GifRecordType";
      return false;
    }
    switch (record_type) {
      case IMAGE_DESC_RECORD_TYPE:
        if (!ReadImageDescriptor(gif_file, paletted_png_ptr,
                                 paletted_info_ptr, palette)) {
          return false;
        }
        break;

      case EXTENSION_RECORD_TYPE:
        if (!ReadExtension(gif_file,
                           paletted_png_ptr,
                           paletted_info_ptr,
                           &transparent_palette_index)) {
          return false;
        }
        break;

      case TERMINATE_RECORD_TYPE:
        found_terminator = true;
        break;

      default:
        DLOG(INFO) << "Found unexpected record type " << record_type;
        return false;
    }
  }

  // Process transparency.
  if (transparent_palette_index >= 0) {
    if (require_opaque) {
      return false;
    }
    // If the GIF contained a transparency index and we're not
    // stripping alpha, then add it to the PNG now if we're returning
    // a paletted image. If we're not, don't bother since we have all
    // the information we need for ExpandColorMap below.
    if (!strip_alpha && !expand_colormap) {
      if (!AddTransparencyChunk(paletted_png_ptr, paletted_info_ptr,
                                transparent_palette_index)) {
        return false;
      }
    }
  }

  if (expand_colormap) {
    // Generate the non-paletted PNG data into the pointers that were
    // passed in.
    if (!ExpandColorMap(paletted_png_ptr, paletted_info_ptr,
                        palette,
                        strip_alpha ? -1 : transparent_palette_index,
                        png_ptr, info_ptr)) {
      return false;
    }
  }
  return true;
}

}  // namespace

namespace pagespeed {

namespace image_compression {

GifReader::GifReader() {
}

GifReader::~GifReader() {
}

bool GifReader::ReadPng(const GoogleString& body,
                        png_structp png_ptr,
                        png_infop info_ptr,
                        int transforms,
                        bool require_opaque) const {
  int allowed_transforms =
      // These transforms are no-ops when reading a .gif file.
      PNG_TRANSFORM_STRIP_16 |
      PNG_TRANSFORM_GRAY_TO_RGB |
      // We implement this transform explicitly.
      PNG_TRANSFORM_EXPAND |
      // We implement this transform explicitly, regardless of require_opaque.
      PNG_TRANSFORM_STRIP_ALPHA;

  if ((transforms & ~allowed_transforms) != 0) {
    LOG(DFATAL) << "Unsupported transform " << transforms;
    return false;
  }

  bool expand_colormap = ((transforms & PNG_TRANSFORM_EXPAND) != 0);
  bool strip_alpha = ((transforms & PNG_TRANSFORM_STRIP_ALPHA) != 0);

  // Wrap the resource's response body in a structure that keeps a
  // pointer to the body and a read offset, and pass a pointer to this
  // object as the user data to be received by the GIF read function.
  GifInput input;
  input.data_ = &body;
  input.offset_ = 0;
#if GIFLIB_MAJOR < 5
  GifFileType* gif_file = DGifOpen(&input, ReadGifFromStream);
#else
  GifFileType* gif_file = DGifOpen(&input, ReadGifFromStream, NULL);
#endif
  if (gif_file == NULL) {
    return false;
  }

  bool result = ReadGifToPng(gif_file, png_ptr, info_ptr,
                             expand_colormap, strip_alpha,
                             require_opaque);
  if (DGifCloseFile(gif_file) == GIF_ERROR) {
    DLOG(INFO) << "Failed to close GIF.";
  }

  return result;
}

bool GifReader::GetAttributes(const GoogleString& body,
                              int* out_width,
                              int* out_height,
                              int* out_bit_depth,
                              int* out_color_type) const {
  // We need the length of the magic bytes (GIF_STAMP_LEN), plus 2
  // bytes for width, plus 2 bytes for height.
  const size_t kGifMinHeaderSize = GIF_STAMP_LEN + 2 + 2;
  if (body.size() < kGifMinHeaderSize) {
    return false;
  }

  // Make sure this looks like a GIF. Either GIF87a or GIF89a.
  if (strncmp(GIF_STAMP, body.data(), GIF_VERSION_POS) != 0) {
    return false;
  }
  const unsigned char* body_data =
      reinterpret_cast<const unsigned char*>(body.data());
  const unsigned char* width_data = body_data + GIF_STAMP_LEN;
  const unsigned char* height_data = width_data + 2;

  *out_width =
      (static_cast<unsigned int>(width_data[1]) << 8) + width_data[0];
  *out_height =
      (static_cast<unsigned int>(height_data[1]) << 8) + height_data[0];

  // GIFs are always 8 bits per channel, paletted images.
  *out_bit_depth = 8;
  *out_color_type = PNG_COLOR_TYPE_PALETTE;
  return true;
}

}  // namespace image_compression

}  // namespace pagespeed
