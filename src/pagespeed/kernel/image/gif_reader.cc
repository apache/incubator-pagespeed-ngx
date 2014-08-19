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

#include <setjmp.h>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/image/image_frame_interface.h"
#include "pagespeed/kernel/image/image_util.h"

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

using net_instaweb::MessageHandler;
using pagespeed::image_compression::ScopedPngStruct;

namespace {

// GIF interlace tables.
static const int kInterlaceOffsets[] = { 0, 4, 2, 1 };
static const int kInterlaceJumps[] = { 8, 8, 4, 2 };
const int kInterlaceNumPass = arraysize(kInterlaceOffsets);
const int kNumColorForUint8 = 256;
const int kGifPaletteSize = 256;

// Flag used to indicate that a gif extension contains transparency
// information.
static const unsigned char kTransparentFlag = 0x01;

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
  pagespeed::image_compression::ScanlineStreamInput* input =
      static_cast<pagespeed::image_compression::ScanlineStreamInput*>(
          gif_file->UserData);
  if (input->offset() + length <= input->length()) {
    memcpy(data, input->data() + input->offset(), length);
    input->set_offset(input->offset() + length);
    return length;
  } else {
    PS_LOG_INFO(input->message_handler(), "Unexpected EOF.");
    return 0;
  }
}

bool AddTransparencyChunk(png_structp png_ptr,
                          png_infop info_ptr,
                          int transparent_palette_index,
                          MessageHandler* handler) {
  const int num_trans = transparent_palette_index + 1;
  if (num_trans <= 0 || num_trans > info_ptr->num_palette) {
    PS_LOG_INFO(handler, "Transparent palette index out of bounds.");
    return false;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    return false;
  }
  // TODO(huibao): to optimize tRNS size, could move transparent index to
  // the head of the palette.
  png_byte trans[kNumColorForUint8];
  // First, set all palette indices to fully opaque.
  memset(trans, 0xff, num_trans);
  // Set the one transparent index to fully transparent.
  trans[transparent_palette_index] =
      pagespeed::image_compression::kAlphaTransparent;
  png_set_tRNS(png_ptr, info_ptr, trans, num_trans, NULL);
  return true;
}

bool ReadImageDescriptor(GifFileType* gif_file,
                         png_structp png_ptr,
                         png_infop info_ptr,
                         png_color* palette,
                         MessageHandler* handler) {
  if (DGifGetImageDesc(gif_file) == GIF_ERROR) {
    PS_DLOG_INFO(handler, "Failed to get image descriptor.");
    return false;
  }
  if (gif_file->ImageCount != 1) {
    PS_DLOG_INFO(handler, "Unable to optimize image with %d frames.", \
                 gif_file->ImageCount);
    return false;
  }
  const GifWord row = gif_file->Image.Top;
  const GifWord pixel = gif_file->Image.Left;
  const GifWord width = gif_file->Image.Width;
  const GifWord height = gif_file->Image.Height;

  // Validate coordinates.
  if (pixel + width > gif_file->SWidth ||
      row + height > gif_file->SHeight) {
    PS_DLOG_INFO(handler, "Image coordinates outside of resolution.");
    return false;
  }

  // Populate the color map.
  ColorMapObject* color_map =
      gif_file->Image.ColorMap != NULL ?
      gif_file->Image.ColorMap : gif_file->SColorMap;

  if (color_map == NULL) {
    PS_DLOG_INFO(handler, "Failed to find color map.");
    return false;
  }

  if (color_map->ColorCount < 0 || color_map->ColorCount > kNumColorForUint8) {
    PS_DLOG_INFO(handler, "Invalid color count %d", color_map->ColorCount);
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
        PS_DLOG_INFO(handler, "Failed to DGifGetLine");
        return false;
      }
    }
  } else {
    // Need to deinterlace. The deinterlace code is based on algorithm
    // in giflib.
    for (int i = 0; i < kInterlaceNumPass; ++i) {
      for (int j = row + kInterlaceOffsets[i];
           j < row + height;
           j += kInterlaceJumps[i]) {
        if (DGifGetLine(gif_file,
                        static_cast<GifPixelType*>(
                            &info_ptr->row_pointers[j][pixel]),
                        width) == GIF_ERROR) {
          PS_DLOG_INFO(handler, "Failed to DGifGetLine");
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
                   int* out_transparent_index,
                   MessageHandler* handler) {
  GifByteType* extension = NULL;
  int ext_code = 0;
  if (DGifGetExtension(gif_file, &ext_code, &extension) == GIF_ERROR) {
    PS_DLOG_INFO(handler, "Failed to read extension.");
    return false;
  }

  // We only care about one extension type, the graphics extension,
  // which can contain transparency information.
  if (ext_code == GRAPHICS_EXT_FUNC_CODE) {
    // Make sure that the extension has the expected length.
    if (extension[0] < 4) {
      PS_DLOG_INFO(handler, \
                   "Received graphics extension with unexpected length.");
      return false;
    }
    // The first payload byte contains the flags. Check to see whether the
    // transparency flag is set.
    if ((extension[1] & kTransparentFlag) != 0) {
      if (*out_transparent_index >= 0) {
        // The transparent index has already been set. Ignore new
        // values.
        PS_DLOG_INFO(handler, \
                     "Found multiple transparency entries. Using first entry.");
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
      PS_DLOG_INFO(handler, "Failed to read next extension.");
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
                  bool require_opaque,
                  MessageHandler* handler) {
  if (static_cast<png_size_t>(gif_file->SHeight) >
      PNG_UINT_32_MAX/png_sizeof(png_bytep)) {
    PS_DLOG_INFO(handler, "GIF image is too big to process.");
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
    paletted_png.reset(new ScopedPngStruct(ScopedPngStruct::READ, handler));
    if (!paletted_png->valid()) {
      PS_LOG_DFATAL(handler, "Invalid ScopedPngStruct r: %d", \
                    paletted_png->valid());
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
  png_color palette[kNumColorForUint8];
  while (!found_terminator) {
    GifRecordType record_type = UNDEFINED_RECORD_TYPE;
    if (DGifGetRecordType(gif_file, &record_type) == GIF_ERROR) {
      PS_DLOG_INFO(handler, "Failed to read GifRecordType");
      return false;
    }
    switch (record_type) {
      case IMAGE_DESC_RECORD_TYPE:
        if (!ReadImageDescriptor(gif_file, paletted_png_ptr,
                                 paletted_info_ptr, palette, handler)) {
          return false;
        }
        break;

      case EXTENSION_RECORD_TYPE:
        if (!ReadExtension(gif_file,
                           paletted_png_ptr,
                           paletted_info_ptr,
                           &transparent_palette_index,
                           handler)) {
          return false;
        }
        break;

      case TERMINATE_RECORD_TYPE:
        found_terminator = true;
        break;

      default:
        PS_DLOG_INFO(handler, "Found unexpected record type %d", record_type);
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
                                transparent_palette_index, handler)) {
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

const int GifFrameReader::kNoTransparentIndex = -1;

// Utility to skip over the extra subblocks in a GIF file extension so
// we leave the file in a state where we've finished processing this
// extension and are ready to begin parsing another.
ScanlineStatus SkipOverGifExtensionSubblocks(GifFileType* gif_file,
                                             GifByteType* extension,
                                             MessageHandler* message_handler) {
  while (extension != NULL) {
    if (DGifGetExtensionNext(gif_file, &extension) == GIF_ERROR) {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler,
                              SCANLINE_STATUS_PARSE_ERROR,
                              FRAME_GIFREADER,
                              "Failed to read next extension.");
    }
  }
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

// Utility to expand a color index from a palette.
void ExpandColorIndex(const ColorMapObject* colormap,
                      const int color_index,
                      PixelRgbaChannels rgba) {
  if ((colormap != NULL) &&
      (colormap->Colors != NULL) &&
      (color_index < colormap->ColorCount)) {
    const GifColorType color = colormap->Colors[color_index];
    rgba[RGBA_RED] = color.Red;
    rgba[RGBA_GREEN] = color.Green;
    rgba[RGBA_BLUE] = color.Blue;
    rgba[RGBA_ALPHA] = kAlphaOpaque;
  }
}

GifReader::GifReader(MessageHandler* handler)
    : message_handler_(handler) {
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
    PS_LOG_DFATAL(message_handler_, "Unsupported transform %d", transforms);
    return false;
  }

  bool expand_colormap = ((transforms & PNG_TRANSFORM_EXPAND) != 0);
  bool strip_alpha = ((transforms & PNG_TRANSFORM_STRIP_ALPHA) != 0);

  // Wrap the resource's response body in a structure that keeps a
  // pointer to the body and a read offset, and pass a pointer to this
  // object as the user data to be received by the GIF read function.
  ScanlineStreamInput input(message_handler_);
  input.Initialize(body);

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
                             require_opaque, message_handler_);
  if (DGifCloseFile(gif_file) == GIF_ERROR) {
    PS_DLOG_INFO(message_handler_, "Failed to close GIF.");
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

class ScopedGifStruct {
 public:
  explicit ScopedGifStruct(MessageHandler* handler) :
      gif_file_(NULL),
      message_handler_(handler),
      gif_input_(ScanlineStreamInput(handler)) {
  }

  ~ScopedGifStruct() {
    ScanlineStatus status;
    Reset(&status);
    LOG_IF(ERROR, !status.Success());
  }

  bool Initialize(const void* image_buffer,
                  size_t buffer_length,
                  ScanlineStatus* status) {
    if (Reset(status)) {
      gif_input_.Initialize(image_buffer, buffer_length);
      return InitializeGifFile(status);
    }
    return false;
  }

  bool Initialize(const std::string& image_string, ScanlineStatus* status) {
    if (Reset(status)) {
      gif_input_.Initialize(image_string);
      return InitializeGifFile(status);
    }
    return false;
  }

  bool Reset(ScanlineStatus* status) {
    if (gif_file_ != NULL) {
      if (DGifCloseFile(gif_file_) == GIF_ERROR) {
        *status = PS_LOGGED_STATUS(PS_LOG_INFO,
                                   message_handler_,
                                   SCANLINE_STATUS_INTERNAL_ERROR,
                                   FRAME_GIFREADER,
                                   "Failed to close GIF file.");
        return false;
      }
      gif_file_ = NULL;
    }
    gif_input_.Reset();
    *status = ScanlineStatus(SCANLINE_STATUS_SUCCESS);
    return true;
  }

  GifFileType* gif_file() { return gif_file_; }
  size_t offset() { return gif_input_.offset(); }
  void set_offset(size_t val) { gif_input_.set_offset(val); }

 private:
  bool InitializeGifFile(ScanlineStatus* status) {
#if GIFLIB_MAJOR < 5
    gif_file_ = DGifOpen(&gif_input_, ReadGifFromStream);
#else
    gif_file_ = DGifOpen(&gif_input_, ReadGifFromStream, NULL);
#endif

    if (gif_file_ == NULL) {
      *status = PS_LOGGED_STATUS(PS_LOG_INFO,
                                 message_handler_,
                                 SCANLINE_STATUS_INTERNAL_ERROR,
                                 FRAME_GIFREADER,
                                 "Failed to open GIF file.");
      return false;
    }
    *status = ScanlineStatus(SCANLINE_STATUS_SUCCESS);
    return true;
  }

 private:
  GifFileType* gif_file_;
  MessageHandler* message_handler_;
  ScanlineStreamInput gif_input_;
};

FrameSpec::DisposalMethod GifDisposalToFrameSpecDisposal(int gif_disposal) {
  if ((gif_disposal > FrameSpec::DISPOSAL_UNKNOWN) &&
      (gif_disposal <= FrameSpec::DISPOSAL_RESTORE)) {
    return static_cast<FrameSpec::DisposalMethod>(gif_disposal);
  }
  return FrameSpec::DISPOSAL_NONE;
}

GifFrameReader::GifFrameReader(MessageHandler* handler)
    : MultipleFrameReader(handler) {
  Reset();
}

GifFrameReader::~GifFrameReader() {
}

ScanlineStatus GifFrameReader::Reset() {
  image_initialized_ = false;
  frame_initialized_ = false;

  image_spec_.Reset();
  frame_spec_.Reset();

  has_loop_count_ = false;
  next_frame_ = 0;

  next_row_ = 0;
  frame_transparent_index_ = kNoTransparentIndex;

  // Note that gif_struct and gif_palette_ are allocated once in the
  // first call to Initialize(), and re-used across Reset()s

  // Note that frame_buffer_ and frame_index_ are allocated each time
  // PrepareNextFrame() is called.

  ScanlineStatus status(SCANLINE_STATUS_SUCCESS);
  if (gif_struct_.get() != NULL) {
    gif_struct_->Reset(&status);
  }

  return status;
}

ScanlineStatus GifFrameReader::ProcessExtensionAffectingFrame() {
  //
  // For more information on the format of the Graphics Control
  // Extension (GCE) refer to
  // http://www.w3.org/Graphics/GIF/spec-gif89a.txt
  // http://www.matthewflickinger.com/lab/whatsinagif/bits_and_bytes.asp#graphics_control_extension_block

  // Expected values, masks, shifts for the various fields in the
  // Graphics Control Extension.
  static const int kGifGceExpectedSize = 4;
  static const int kGifGceDisposeMask = 0x07;
  static const int kGifGceDisposeShift = 2;
  static const int kGifGceTransparentMask = 0x01;

  // Indices of fields in the Graphics Control Extension.
  static const int kGifGceSizeIndex = 0;
  static const int kGifGceFlagsIndex = 1;
  static const int kGifGceDelayLoIndex = 2;
  static const int kGifGceDelayHiIndex = 3;
  static const int kGifGceTransparentIndexIndex = 4;

  GifFileType* gif_file = gif_struct_->gif_file();

  GifByteType* extension = NULL;
  int ext_code = 0;
  if (DGifGetExtension(gif_file, &ext_code, &extension) == GIF_ERROR) {
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                            SCANLINE_STATUS_PARSE_ERROR,
                            FRAME_GIFREADER,
                            "Failed to read extension.");
  }

  if (ext_code == GRAPHICS_EXT_FUNC_CODE) {
    if (extension[kGifGceSizeIndex] != kGifGceExpectedSize) {
      return PS_LOGGED_STATUS(
          PS_LOG_ERROR, message_handler(),
          SCANLINE_STATUS_PARSE_ERROR,
          FRAME_GIFREADER,
          "Received graphics extension with unexpected length.");
    }
    const int flags = extension[kGifGceFlagsIndex];
    const int dispose = (flags >> kGifGceDisposeShift) & kGifGceDisposeMask;
    const int delay =
        extension[kGifGceDelayLoIndex] |
        (extension[kGifGceDelayHiIndex] << 8);  // In 10 ms units.
    frame_spec_.duration_ms = delay * 10;

    FrameSpec::DisposalMethod frame_dispose =
        GifDisposalToFrameSpecDisposal(dispose);
    if (frame_dispose != FrameSpec::DISPOSAL_UNKNOWN) {
      frame_spec_.disposal = frame_dispose;
    } else {
      // For non-animated images, the disposal method does not
      // matter, so just note errors and move on. For animated
      // images, an unrecognized disposal methods constitutes an
      // error.
      if (image_spec_.num_frames == 1) {
        PS_LOG_INFO(message_handler(),
                    "Unrecognized disposal method %d.", dispose);
        frame_spec_.disposal = FrameSpec::DISPOSAL_NONE;
      } else {
        return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                                SCANLINE_STATUS_PARSE_ERROR,
                                FRAME_GIFREADER,
                                "Unrecognized disposal method %d.", dispose);
      }
    }

    // Gif89a standard permits multiple Graphic Control Extensions
    // (corresponding to EXTENSION_RECORD_TYPE) in a GIF file. Each
    // Graphic Control Extension only affects the Image Extension or
    // Text Extension which immediately follows it. There may be
    // Text Extensions in the GIF file, too. Thus, we want to get
    // the Control Extensions preceding the next Image Extension.
    //
    // Reference: http://www.w3.org/Graphics/GIF/spec-gif89a.txt

    // In case the next record is an Image Extension (i.e. the
    // next frame), get the transparent index.
    frame_transparent_index_ =
        ((flags & kGifGceTransparentMask) ?
         extension[kGifGceTransparentIndexIndex] : kNoTransparentIndex);
  }

  // We skip all other extension block types, such as
  // APPLICATION_EXT_FUNC_CODE, COMMENT_EXT_FUNC_CODE,
  // PLAINTEXT_EXT_FUNC_CODE.

  return SkipOverGifExtensionSubblocks(gif_file, extension, message_handler());
}

ScanlineStatus GifFrameReader::ProcessExtensionAffectingImage(
    bool past_first_frame) {
  // For more information on the format of the Application Extension
  // (AE) block refer to
  // http://shortn/_19L2jvGJc9

  // Expected values for the various fields in the Application
  // Extension block,
  static const char* kGifAeIdentifier = "NETSCAPE2.0";
  static const int kGifAeIdentifierLength = strlen(kGifAeIdentifier);
  static const int kGifAeLoopCountExpectedLength = 3;
  static const int kGifAeLoopCountExpectedFixedConst = 1;

  // Indices of fields in the Application Extension block,
  static const int kGifAeIdentifierLengthIndex = 0;
  static const int kGifAeLoopCountLengthIndex = 0;
  static const int kGifAeLoopCountFixedConstIndex = 1;
  static const int kGifAeLoopCountLoIndex = 2;
  static const int kGifAeLoopCountHiIndex = 3;

  GifFileType* gif_file = gif_struct_->gif_file();
  GifByteType* extension = NULL;
  int ext_code = 0;

  if (DGifGetExtension(gif_file, &ext_code, &extension) == GIF_ERROR) {
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                            SCANLINE_STATUS_PARSE_ERROR,
                            FRAME_GIFREADER,
                            "Failed to read extension.");
  }

  if  (ext_code == APPLICATION_EXT_FUNC_CODE) {
    if (extension == NULL) {
      return PS_LOGGED_STATUS(
          PS_LOG_ERROR, message_handler(),
          SCANLINE_STATUS_PARSE_ERROR,
          FRAME_GIFREADER,
          "NULL Application Extension Block.");
    }
    if (extension[kGifAeIdentifierLengthIndex] != kGifAeIdentifierLength) {
      return PS_LOGGED_STATUS(
          PS_LOG_ERROR, message_handler(),
          SCANLINE_STATUS_PARSE_ERROR,
          FRAME_GIFREADER,
          "Application extension block size has unexpected size.");
    }
    if (!memcmp(extension + 1, kGifAeIdentifier, kGifAeIdentifierLength)) {
      // Recognize and parse Netscape2.0 NAB extension for loop count.
      if (DGifGetExtensionNext(gif_file, &extension) == GIF_ERROR) {
        return PS_LOGGED_STATUS(
            PS_LOG_ERROR, message_handler(),
            SCANLINE_STATUS_PARSE_ERROR,
            FRAME_GIFREADER,
            "DGifGetExtensionNext failed while trying to get loop count");
      }
      if (((extension[kGifAeLoopCountLengthIndex] !=
            kGifAeLoopCountExpectedLength) &&
           (extension[kGifAeLoopCountFixedConstIndex] !=
            kGifAeLoopCountExpectedFixedConst))) {
        return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                                SCANLINE_STATUS_PARSE_ERROR,
                                FRAME_GIFREADER,
                                "animation loop count: wrong size/marker");
      }
      if (past_first_frame) {
        // This is allowed by the spec but appears to be rare in practice.
        PS_LOG_INFO(message_handler(),
                    "Animation loop count in unexpected location.");
      }
      if (has_loop_count_) {
        PS_LOG_INFO(
            message_handler(),
            "Multiple loop counts encountered. Using the last one.");
      }
      has_loop_count_ = true;
      image_spec_.loop_count = (extension[kGifAeLoopCountLoIndex] |
                                (extension[kGifAeLoopCountHiIndex] << 8));
    } else {
      // An extension containing metadata.
      // Do nothing for now.
    }
  }

  return SkipOverGifExtensionSubblocks(gif_file, extension, message_handler());
}

ScanlineStatus GifFrameReader::Initialize(const void* image_buffer,
                                          size_t buffer_length) {
  if (image_initialized_) {
    // Reset the reader if it has been initialized before.
    Reset();
  } else {
    // Allocate and initialize gif_struct_, if that has not been done.
    if (gif_struct_ == NULL) {
      gif_struct_.reset(new ScopedGifStruct(message_handler()));
      if (gif_struct_ == NULL) {
        return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                                SCANLINE_STATUS_MEMORY_ERROR,
                                FRAME_GIFREADER,
                                "Failed to allocate ScopedGifStruct.");
      }
    }
    // Allocate and initialize gif_palette_, if that has not been done.
    if (gif_palette_ == NULL) {
      gif_palette_.reset(new PaletteRGBA[kGifPaletteSize]);
      if (gif_palette_ == NULL) {
        return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                                SCANLINE_STATUS_MEMORY_ERROR,
                                FRAME_GIFREADER,
                                "Failed to allocate PaletteRGBA.");
      }
    }
  }

  ScanlineStatus status(SCANLINE_STATUS_SUCCESS);
  if (gif_struct_->Initialize(image_buffer, buffer_length, &status)) {
    status = GetImageData();
  }
  if (!status.Success()) {
    Reset();
    return status;
  }

  next_frame_ = 0;
  image_initialized_ = true;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

// Helper function for GetImageData
inline void ClearGifColorMap(GifFileType* gif_file) {
#if GIFLIB_MAJOR < 5
  // For each frame, we call DGifGetImageDesc, and that always
  // allocates a new Image.ColorMap without freeing any previous color
  // map stored there; in fact, the only free of that data field
  // occurs in DGifCloseFile. Thus, to prevent a memory leak, we need
  // to clear it manually before each call to DGifGetImageDesc.
  if (gif_file->Image.ColorMap != NULL) {
    FreeMapObject(gif_file->Image.ColorMap);
    gif_file->Image.ColorMap = NULL;
  }
#endif
}

// TODO(vchudnov): Consider simplifying this function so that it only
// determines whether there's more than one frame, rather than the
// number of frames. This would avoid having to scan the whole file
// here and again when we read pixels. Doing this would involve moving
// the logic to detect more frames in the stream to HasMoreFrames(),
// and updating image_spec dynamically. Cons: if, in the wild, the
// Applications Extension Blocks are found anywhere in the file (as
// seems to be allowed by the spec), then we need to scan through the
// whole file anyway.
ScanlineStatus GifFrameReader::GetImageData() {
  GifFileType* gif_file = gif_struct_->gif_file();
  size_t offset = gif_struct_->offset();
  image_spec_.width =  gif_file->SWidth;
  image_spec_.height = gif_file->SHeight;

  GifRecordType record_type = UNDEFINED_RECORD_TYPE;
  while (record_type != TERMINATE_RECORD_TYPE) {
    DVLOG(1) << "GetImageData: offset: ", gif_struct_->offset();

    if (DGifGetRecordType(gif_file, &record_type) == GIF_ERROR) {
      return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                              SCANLINE_STATUS_PARSE_ERROR,
                              FRAME_GIFREADER,
                              "DGifGetRecordType()");
    }
    switch (record_type) {
      case IMAGE_DESC_RECORD_TYPE:
        DVLOG(1) << "GetImageData: IMAGE_DESC";

        ClearGifColorMap(gif_file);
        if (DGifGetImageDesc(gif_file) == GIF_ERROR) {
          return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                                  SCANLINE_STATUS_PARSE_ERROR,
                                  FRAME_GIFREADER,
                                  "DGifGetImageDesc()");
        }

        ++image_spec_.num_frames;
        if (image_spec_.num_frames == 1) {
          // We need the first frame info for the quirks mode.
          frame_spec_.top = gif_file->Image.Top;
          frame_spec_.left = gif_file->Image.Left;
          frame_spec_.height = gif_file->Image.Height;
          frame_spec_.width = gif_file->Image.Width;
        }

        // Bypass the pixel data without decoding it.
        int code_size;
        GifByteType* code_block;
        if (DGifGetCode(gif_file, &code_size, &code_block) == GIF_ERROR) {
          return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                                  SCANLINE_STATUS_PARSE_ERROR,
                                  FRAME_GIFREADER,
                                  "DGifGetCode()");
        }
        while (code_block != NULL) {
          if (DGifGetCodeNext(gif_file, &code_block) == GIF_ERROR) {
            return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                                    SCANLINE_STATUS_PARSE_ERROR,
                                    FRAME_GIFREADER,
                                    "DGifGetCodeNext()");
          }
        }

        break;

      case EXTENSION_RECORD_TYPE: {
        DVLOG(1) << "GetImageData: EXTENSION";
        ScanlineStatus status =
            ProcessExtensionAffectingImage(image_spec_.num_frames > 0);
        if (!status.Success()) {
          return status;
        }
        break;
      }

      case TERMINATE_RECORD_TYPE:
        DVLOG(1) << "GetImageData: TERMINATE";
        break;

      default:
        return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                                SCANLINE_STATUS_PARSE_ERROR,
                                FRAME_GIFREADER,
                                "unexpected record %d",
                                record_type);
        break;
    }
  }

  ApplyQuirksModeToImage(quirks_mode(), has_loop_count_, frame_spec_,
                         &image_spec_);

  // Get the global color map, and extract and store the background
  // color from it.
  //
  // TODO(vchudnov): The GIF spec implies there could be more than one
  // global color map in a file. Each global color map replaces any
  // preceding one and applies to the image descriptors following
  // (unless they have a local color map, of course). Find out how
  // giflib deals with this, how often it occurs in practice, and, if
  // necessary, accommodate this situation in code.
  ExpandColorIndex(gif_file->SColorMap,
                   gif_file->SBackGroundColor,
                   image_spec_.bg_color);
  // The GIF background color seems to not be interpreted by Chrome,
  // Firefox, and other image viewers, so regardless of whether we
  // found a background color or not, we don't use it.
  image_spec_.use_bg_color = false;

  gif_struct_->set_offset(offset);
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);;
}

ScanlineStatus GifFrameReader::set_quirks_mode(QuirksMode quirks_mode) {
  if (image_initialized_) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            FRAME_GIFREADER,
                            "Can't change quirks mode for initialized image.");
  }
  return MultipleFrameReader::set_quirks_mode(quirks_mode);
}

void GifFrameReader::ApplyQuirksModeToImage(QuirksMode quirks_mode,
                                            const bool got_loop_count,
                                            const FrameSpec& frame_spec,
                                            ImageSpec* image_spec) {
  switch (quirks_mode) {
    case QUIRKS_CHROME:
      // Based on Chrome 35 behavior on Linux.
      if ((frame_spec.width > image_spec->width) ||
          (frame_spec.height > image_spec->height)) {
        image_spec->width = frame_spec.width;
        image_spec->height = frame_spec.height;
      }
      if (got_loop_count) {
        image_spec->loop_count++;
      }
      break;
    case QUIRKS_FIREFOX:
      break;
    case QUIRKS_NONE:
      // We enumerate all cases and purposefully exclude the default
      // label so compiler will complain about unhandled values.
      break;
  }
}

void GifFrameReader::ApplyQuirksModeToFirstFrame(const QuirksMode quirks_mode,
                                                 const ImageSpec& image_spec,
                                                 FrameSpec* frame_spec) {
  switch (quirks_mode) {
    case QUIRKS_CHROME:
      break;
    case QUIRKS_FIREFOX:
      // Based on Firefox 29 behavior on Linux.
      if ((frame_spec->width > image_spec.width) ||
          (frame_spec->height > image_spec.height)) {
        frame_spec->left = 0;
        frame_spec->top = 0;
      }
      break;
    case QUIRKS_NONE:
      // We enumerate all cases and purposefully exclude the default
      // label so compiler will complain about unhandled values.
      break;
  }
}

ScanlineStatus GifFrameReader::CreateColorMap() {
  GifFileType* gif_file = gif_struct_->gif_file();

  // Populate the color map.
  ColorMapObject* color_map =
      gif_file->Image.ColorMap != NULL ?
      gif_file->Image.ColorMap : gif_file->SColorMap;
  if (color_map == NULL) {
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            FRAME_GIFREADER,
                            "missing colormap in image and screen");
  }

  GifColorType* palette_in = color_map->Colors;
  if (palette_in == NULL) {
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            FRAME_GIFREADER,
                            "Could not find colormap in the GIF image.");
  }
  if (color_map->ColorCount > kGifPaletteSize) {
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            FRAME_GIFREADER,
                            "ColorCount is too large: %d",
                            color_map->ColorCount);
  }

  for (int i = 0; i < color_map->ColorCount; ++i) {
    gif_palette_[i].red_ = palette_in[i].Red;
    gif_palette_[i].green_ = palette_in[i].Green;
    gif_palette_[i].blue_ = palette_in[i].Blue;
    gif_palette_[i].alpha_ = kAlphaOpaque;
  }

  // Process the transparency information. The output format will be RGBA
  // if the transparent color has been specified, or RGB otherwise.
  if ((frame_transparent_index_ >= 0) &&
      (frame_transparent_index_ < color_map->ColorCount)) {
    frame_spec_.pixel_format = RGBA_8888;
    gif_palette_[frame_transparent_index_].alpha_ = kAlphaTransparent;
  } else {
    frame_spec_.pixel_format = RGB_888;
  }

  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

// Decode a progressive GIF. The deinterlace code is based on the algorithm
// in giflib.
ScanlineStatus GifFrameReader::DecodeProgressiveGif() {
  GifFileType* gif_file = gif_struct_->gif_file();
  for (int pass = 0; pass < kInterlaceNumPass; ++pass) {
    for (size_px y = kInterlaceOffsets[pass];
         y < frame_spec_.height;
         y += kInterlaceJumps[pass]) {
      GifPixelType* row_pointer = frame_index_.get() + y * frame_spec_.width;
      if (DGifGetLine(gif_file, row_pointer, frame_spec_.width) == GIF_ERROR) {
        return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                                SCANLINE_STATUS_INTERNAL_ERROR,
                                FRAME_GIFREADER,
                                "DGifGetLine()");
      }
    }
  }
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus GifFrameReader::PrepareNextFrame() {
  // Consume any remaining scanlines
  const void* out_bytes;
  ScanlineStatus status;
  while (HasMoreScanlines()) {
    if (!MultipleFrameReader::ReadNextScanline(&out_bytes, &status)) {
      return status;
    }
  }

  frame_initialized_ = false;
  frame_spec_.Reset();
  frame_transparent_index_ = kNoTransparentIndex;

  if (next_frame_ >= image_spec_.num_frames) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            FRAME_GIFREADER,
                            "PrepareNextFrame: no more frames.");
  }

  GifFileType* gif_file = gif_struct_->gif_file();
  bool found_frame = false;

  while (!found_frame) {
    DVLOG(1) << "PrepareNextFrame: offset: ", gif_struct_->offset();

    GifRecordType record_type = UNDEFINED_RECORD_TYPE;
    if (DGifGetRecordType(gif_file, &record_type) == GIF_ERROR) {
      return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                              SCANLINE_STATUS_PARSE_ERROR,
                              FRAME_GIFREADER,
                              "DGifGetRecordType()");
    }

    switch (record_type) {
      case IMAGE_DESC_RECORD_TYPE: {
        DVLOG(1) << "PrepareNextFrame: IMAGE_DESC";
        ClearGifColorMap(gif_file);
        if (DGifGetImageDesc(gif_file) == GIF_ERROR) {
          return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                                  SCANLINE_STATUS_PARSE_ERROR,
                                  FRAME_GIFREADER,
                                  "DGifGetImageDesc()");
        }
        frame_spec_.top = gif_file->Image.Top;
        frame_spec_.left = gif_file->Image.Left;
        frame_spec_.height = gif_file->Image.Height;
        frame_spec_.width = gif_file->Image.Width;
        if (next_frame_ == 0) {
          ApplyQuirksModeToFirstFrame(quirks_mode(), image_spec_, &frame_spec_);
        }
        ++next_frame_;
        found_frame = true;
        break;
      }

      case EXTENSION_RECORD_TYPE: {
        DVLOG(1) << "PrepareNextFrame: EXTENSION";
        ScanlineStatus status = ProcessExtensionAffectingFrame();
        if (!status.Success()) {
          return status;
        }
        break;
      }

      case TERMINATE_RECORD_TYPE: {
        DVLOG(1) << "PrepareNextFrame: TERMINATE";
        return PS_LOGGED_STATUS(
            PS_LOG_INFO, message_handler(),
            SCANLINE_STATUS_INTERNAL_ERROR,
            FRAME_GIFREADER,
            "PrepareNextFrame: expected to find the next frame, failed.");
        break;
      }

      default: {
        return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                                SCANLINE_STATUS_PARSE_ERROR,
                                FRAME_GIFREADER,
                                "unexpected record %d",
                                record_type);
        break;
      }
    }
  }

  status = CreateColorMap();
  if (!status.Success()) {
    Reset();
    return status;
  }

  frame_spec_.hint_progressive = (gif_file->Image.Interlace != 0);
  size_t bytes_per_row =
      GetBytesPerPixel(frame_spec_.pixel_format) * frame_spec_.width;

  // Allocate the memory we'll need to read the image data in
  // ReadNextScanline().
  frame_buffer_.reset(new GifPixelType[bytes_per_row]);
  if (!frame_spec_.hint_progressive) {
    // We only read one row at a time.
    frame_index_.reset(new GifPixelType[frame_spec_.width]);
  } else {
    // We need to read all the rows before we return from
    // ReadNextScanline() the first time.
    frame_index_.reset(new GifPixelType[frame_spec_.width *
                                        frame_spec_.height]);
  }
  if (frame_buffer_ == NULL || frame_index_ == NULL) {
    Reset();
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_MEMORY_ERROR,
                            FRAME_GIFREADER,
                            "new GiPixelType[] for frame_buffer_ "
                            "or frame_index_");
  }

  next_row_ = 0;
  frame_initialized_ = true;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus GifFrameReader::ReadNextScanline(
    const void** out_scanline_bytes) {
  if (!frame_initialized_ || !HasMoreScanlines()) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            FRAME_GIFREADER,
                            "The GIF image was not initialized or does not "
                            "have more scanlines.");
  }

  // For a progressive GIF, we have to decode the entire frame before
  // rendering any row.
  if (frame_spec_.hint_progressive && (next_row_ == 0)) {
    ScanlineStatus status = DecodeProgressiveGif();
    if (!status.Success()) {
      PS_LOG_INFO(message_handler(), "Failed to progressively decode GIF.");
      Reset();
      return status;
    }
  }

  // Convert the color index to the actual color.
  GifPixelType* color_buffer = frame_buffer_.get();
  const size_t pixel_size = GetBytesPerPixel(frame_spec_.pixel_format);

  // Find out the color index for the requested row.
  GifPixelType* index_buffer = NULL;
  GifFileType* gif_file = gif_struct_->gif_file();
  if (!frame_spec_.hint_progressive) {
    // For a non-progressive GIF, we decode the image a row at a time.
    index_buffer = frame_index_.get();
    if (DGifGetLine(gif_file, index_buffer, frame_spec_.width) == GIF_ERROR) {
      Reset();
      return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler(),
                              SCANLINE_STATUS_INTERNAL_ERROR,
                              FRAME_GIFREADER,
                              "DGifGetLine()");
    }
  } else {
    // For a progressive GIF, we simply point the output to the corresponding
    // row, because the image has already been decoded.
    index_buffer = frame_index_.get() + next_row_ * frame_spec_.width;
  }

  for (size_px pixel_index = 0;
       pixel_index < frame_spec_.width;
       ++pixel_index) {
    // Convert the color index to the actual color.
    int color_index = *(index_buffer++);
    memcpy(color_buffer, gif_palette_.get() + color_index, pixel_size);
    color_buffer += pixel_size;
  }

  *out_scanline_bytes = static_cast<void*>(frame_buffer_.get());
  ++next_row_;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

}  // namespace image_compression

}  // namespace pagespeed
