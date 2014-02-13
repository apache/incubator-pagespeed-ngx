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
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/image/scanline_utils.h"

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
const uint8 kAlphaOpaque = 255;
const uint8 kAlphaTransparent = 0;
const int kNumColorForUint8 = 256;
const int kPaletteBackgroundIndex = 256;
const int kGifPaletteSize = kPaletteBackgroundIndex + 1;

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
  trans[transparent_palette_index] = kAlphaTransparent;
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
    Reset();
  }

  bool Initialize(const void* image_buffer, size_t buffer_length) {
    gif_input_.Initialize(image_buffer, buffer_length);
    return InitializeGifFile();
  }

  bool Initialize(const std::string& image_string) {
    gif_input_.Initialize(image_string);
    return InitializeGifFile();
  }

  bool Reset() {
    if (gif_file_ != NULL) {
      if (DGifCloseFile(gif_file_) == GIF_ERROR) {
        PS_LOG_ERROR(message_handler_, "Failed to close GIF file.");
        return false;
      }
      gif_file_ = NULL;
    }
    gif_input_.Reset();
    return true;
  }

  // Size of the output image which corresponds to the "logical screen" in GIF.
  size_t width() { return gif_file_->SWidth; }
  size_t height() { return gif_file_->SHeight; }

  // Position of the encoded image with respect to the logical screen. The
  // uncovered portion of the logical screen will be filled with the background
  // color.
  int first_row() { return gif_file_->Image.Top; }
  int first_col() { return gif_file_->Image.Left; }
  int last_row() {
    return gif_file_->Image.Top + gif_file_->Image.Height - 1;
  }
  int last_col() {
    return gif_file_->Image.Left + gif_file_->Image.Width - 1;
  }

  GifFileType* gif_file() { return gif_file_; }
  size_t offset() { return gif_input_.offset(); }
  void set_offset(size_t val) { gif_input_.set_offset(val); }

 private:
  bool InitializeGifFile() {
#if GIFLIB_MAJOR < 5
    gif_file_ = DGifOpen(&gif_input_, ReadGifFromStream);
#else
    gif_file_ = DGifOpen(&gif_input_, ReadGifFromStream, NULL);
#endif

    if (gif_file_ == NULL) {
      PS_LOG_INFO(message_handler_, "Failed to open GIF file.");
      return false;
    }
    return true;
  }

 private:
  GifFileType* gif_file_;
  MessageHandler* message_handler_;
  ScanlineStreamInput gif_input_;
};

GifScanlineReaderRaw::GifScanlineReaderRaw(
    MessageHandler* handler)
  : message_handler_(handler) {
  Reset();
}

GifScanlineReaderRaw::~GifScanlineReaderRaw() {
}

bool GifScanlineReaderRaw::Reset() {
  pixel_format_ = UNSUPPORTED;
  is_progressive_ = false;
  width_ = 0;
  height_ = 0;
  row_ = 0;
  pixel_size_ = 0;
  bytes_per_row_ = 0;
  was_initialized_ = false;

  if (gif_struct_.get() != NULL) {
    gif_struct_->Reset();
  }

  return true;
}

// ProcessSingleImageGif() checks whether the GIF file is valid and whether it
// contains only one image (i.e., not an animated GIF). It also returns the
// offset of the image and the index to the transparent color, if transparency
// has been specified.
//
// Note: Currently we only support single image GIF (non-animated GIF). We would
// like to to find out the number of images before decoding the pixel data.
// However, The GIF format does not include the number of images in the header.
// A GIF stream is simply a concatenation of all images. We have to scan all of
// the chunks (records) to find out this number. If the file is found to contain
// only one image, we then jump directly to the starting position of the image
// record and decode the pixel data.
//
// Note: giflib has an "ImageCount" field in the "GifFileType" struct. But this
// field is not valid until the whole stream has been processed.
//
// Reference: http://www.w3.org/Graphics/GIF/spec-gif89a.txt
//
ScanlineStatus GifScanlineReaderRaw::ProcessSingleImageGif(
    size_t* first_frame_offset,
    int* transparent_index) {
  *first_frame_offset = 0;
  *transparent_index = -1;

  int num_frames = 0;
  bool found_terminator = false;
  GifFileType* gif_file = gif_struct_->gif_file();

  while (!found_terminator) {
    GifRecordType record_type = UNDEFINED_RECORD_TYPE;
    if (DGifGetRecordType(gif_file, &record_type) == GIF_ERROR) {
      return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                              SCANLINE_STATUS_PARSE_ERROR,
                              SCANLINE_GIFREADERRAW,
                              "DGifGetRecordType()");
    }

    // Mark the offset of input image stream.
    size_t current_offset = gif_struct_->offset();

    switch (record_type) {
      case IMAGE_DESC_RECORD_TYPE:
        if (DGifGetImageDesc(gif_file) == GIF_ERROR) {
          return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                                  SCANLINE_STATUS_PARSE_ERROR,
                                  SCANLINE_GIFREADERRAW,
                                  "DGifGetImageDesc()");
        }

        // Currently we only support single frame GIF.
        ++num_frames;
        if (num_frames > 1) {
          return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                                  SCANLINE_STATUS_UNSUPPORTED_FEATURE,
                                  SCANLINE_GIFREADERRAW,
                                  "multiple-frame GIF");
        } else {
          *first_frame_offset = current_offset;
        }

        // Bypass the pixel data without decoding it.
        int code_size;
        GifByteType* code_block;
        if (DGifGetCode(gif_file, &code_size, &code_block) == GIF_ERROR) {
          return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                                  SCANLINE_STATUS_PARSE_ERROR,
                                  SCANLINE_GIFREADERRAW,
                                  "DGifGetCode()");
        }
        while (code_block != NULL) {
          if (DGifGetCodeNext(gif_file, &code_block) == GIF_ERROR) {
            return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                                    SCANLINE_STATUS_PARSE_ERROR,
                                    SCANLINE_GIFREADERRAW,
                                    "DGifGetCodeNext()");
          }
        }

        break;

      case EXTENSION_RECORD_TYPE:
        {
          // Variable "index" is initialized to "-1" so ReadExtension() will
          // assign a new value to it.
          int index = -1;
          if (!ReadExtension(gif_file, NULL, NULL, &index, message_handler_)) {
            return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                                    SCANLINE_STATUS_PARSE_ERROR,
                                    SCANLINE_GIFREADERRAW,
                                    "ReadExtension()");
          }

          // Gif89a standard permits multiple Graphic Control Extensions
          // (corresponding to EXTENSION_RECORD_TYPE) in a GIF file. Each
          // Graphic Control Extension only affects the Image Extension or
          // Text Extension which immediately follows it. Although we only
          // allow a single Image Extension (no animation) in the GIF file,
          // there may be Text Extensions in the file, too. Thus, we want to get
          // the Control Extensions immediately before the first (actually
          // the only) Image Extension.
          //
          // Reference: http://www.w3.org/Graphics/GIF/spec-gif89a.txt
          if (num_frames == 0) {
            // In case the next record is the first (and only) Image Extension,
            // get the transparent index.
            *transparent_index = index;
          }
        }
        break;

      case TERMINATE_RECORD_TYPE:
        found_terminator = true;
        break;

      default:
        return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                                SCANLINE_STATUS_PARSE_ERROR,
                                SCANLINE_GIFREADERRAW,
                                "unexpected record %d",
                                record_type);
    }
  }
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus GifScanlineReaderRaw::CreateColorMap(int transparent_index) {
  GifFileType* gif_file = gif_struct_->gif_file();

  // Populate the color map.
  ColorMapObject* color_map =
    gif_file->Image.ColorMap != NULL ?
    gif_file->Image.ColorMap : gif_file->SColorMap;
  if (color_map == NULL) {
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            SCANLINE_GIFREADERRAW,
                            "missing colormap in image and screen");
  }

  GifColorType* palette_in = color_map->Colors;
  if (palette_in == NULL) {
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            SCANLINE_GIFREADERRAW,
                            "Could not find colormap in the GIF image.");
  }

  for (int i = 0; i < color_map->ColorCount; ++i) {
    gif_palette_[i].red_ = palette_in[i].Red;
    gif_palette_[i].green_ = palette_in[i].Green;
    gif_palette_[i].blue_ = palette_in[i].Blue;
    gif_palette_[i].alpha_ = kAlphaOpaque;
  }

  // If the image does not cover the entire screen, the background color will
  // be used to fill the uncovered portion. The background color will be stored
  // in the 257th element of color palette.
  if (HasVisibleBackground()) {
    int background_index = gif_file->SBackGroundColor;
    if (background_index >= color_map->ColorCount) {
      return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                              SCANLINE_STATUS_INTERNAL_ERROR,
                              SCANLINE_GIFREADERRAW,
                              "Invalid background color in the GIF image.");
    }
    gif_palette_[kPaletteBackgroundIndex].red_ =
        gif_palette_[background_index].red_;
    gif_palette_[kPaletteBackgroundIndex].green_ =
        gif_palette_[background_index].green_;
    gif_palette_[kPaletteBackgroundIndex].blue_ =
        gif_palette_[background_index].blue_;

    if (background_index == transparent_index) {
      gif_palette_[kPaletteBackgroundIndex].alpha_ = kAlphaTransparent;
    } else {
      gif_palette_[kPaletteBackgroundIndex].alpha_ = kAlphaOpaque;
    }
  }

  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

// Some images have screen size smaller than that of the encoded pixels,
// so we may need to extend the screen (image) size.
void GifScanlineReaderRaw::ComputeOrExtendImageSize() {
  width_ = gif_struct_->width();
  if (width_ <= gif_struct_->last_col()) {
    width_ = gif_struct_->last_col() + 1;
  }
  height_ = gif_struct_->height();
  if (height_ <= gif_struct_->last_row()) {
    height_ = gif_struct_->last_row() + 1;
  }
}

bool GifScanlineReaderRaw::HasVisibleBackground() {
  return (gif_struct_->first_row() > 0 || gif_struct_->first_col() > 0 ||
          gif_struct_->last_row() < static_cast<int>(GetImageHeight()) - 1 ||
          gif_struct_->last_col() < static_cast<int>(GetImageWidth()) - 1);

}

// Initialize the reader with the given image stream. Note that image_buffer
// must remain unchanged until the last call to ReadNextScanline().
ScanlineStatus GifScanlineReaderRaw::InitializeWithStatus(
    const void* image_buffer,
    size_t buffer_length) {
  if (was_initialized_) {
    // Reset the reader if it has been initialized before.
    Reset();
  } else {
    // Allocate and initialize gif_struct_, if that has not been done.
    if (gif_struct_ == NULL) {
      gif_struct_.reset(new ScopedGifStruct(message_handler_));
      if (gif_struct_ == NULL) {
        return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler_,
                                SCANLINE_STATUS_MEMORY_ERROR,
                                SCANLINE_GIFREADERRAW,
                                "Failed to allocate ScopedGifStruct.");
      }
    }
    // Allocate and initialize gif_palette_, if that has not been done.
    if (gif_palette_ == NULL) {
      gif_palette_.reset(new PaletteRGBA[kGifPaletteSize]);
      if (gif_palette_ == NULL) {
        return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler_,
                                SCANLINE_STATUS_MEMORY_ERROR,
                                SCANLINE_GIFREADERRAW,
                                "Failed to allocate PaletteRGBA.");
      }
    }
  }

  // Set up data input for giflib.
  if (!gif_struct_->Initialize(image_buffer, buffer_length)) {
    Reset();
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            SCANLINE_GIFREADERRAW,
                            "Failed to iInitialize GIF reader.");
  }
  GifFileType* gif_file = gif_struct_->gif_file();

  // Check whether the stream is a valid GIF and contains only one image.
  // If it is, find out the position of the image record and the index of
  // transparent color.
  size_t image1_offset = 0;
  int image1_transparent_index = -1;
  ScanlineStatus process_status = ProcessSingleImageGif(
      &image1_offset,
      &image1_transparent_index);
  if (!process_status.Success()) {
    Reset();
    return process_status;
  }

  // Point giflib to the start of the image record. Get the size and palette
  // information of the image.
  gif_struct_->set_offset(image1_offset);
  if (DGifGetImageDesc(gif_file) == GIF_ERROR) {
    Reset();
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            SCANLINE_GIFREADERRAW,
                            "DGifGetImageDesc()");
  }
  ComputeOrExtendImageSize();

  ScanlineStatus colormap_status = CreateColorMap(image1_transparent_index);
  if (!colormap_status.Success()) {
    Reset();
    return colormap_status;
  }





  // Process the transparency information. The output format will be RGBA
  // if the transparent color has been specified, or RGB otherwise.
  if (image1_transparent_index >= 0) {
    pixel_format_ = RGBA_8888;
    pixel_size_ = 4;
    gif_palette_[image1_transparent_index].alpha_ = kAlphaTransparent;
  } else {
    pixel_format_ = RGB_888;
    pixel_size_ = 3;
  }

  is_progressive_ = (gif_file->Image.Interlace != 0);
  bytes_per_row_ = pixel_size_ * GetImageWidth();
  was_initialized_ = true;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

// Decode a progressive GIF. The deinterlace code is based on the algorithm
// in giflib.
ScanlineStatus GifScanlineReaderRaw::DecodeProgressiveGif() {
  GifFileType* gif_file = gif_struct_->gif_file();
  int actual_width = gif_struct_->last_col() - gif_struct_->first_col() + 1;
  for (int pass = 0; pass < kInterlaceNumPass; ++pass) {
    for (int y = gif_struct_->first_row() + kInterlaceOffsets[pass];
         y <= gif_struct_->last_row();
         y += kInterlaceJumps[pass]) {
      GifPixelType* row_pointer = image_index_.get() + y * GetImageWidth();
      if (DGifGetLine(gif_file, row_pointer, actual_width) == GIF_ERROR) {
        return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                                SCANLINE_STATUS_INTERNAL_ERROR,
                                SCANLINE_GIFREADERRAW,
                                "DGifGetLine()");
      }
    }
  }
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus GifScanlineReaderRaw::ReadNextScanlineWithStatus(
    void** out_scanline_bytes) {
  if (!was_initialized_ || !HasMoreScanLines()) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            SCANLINE_GIFREADERRAW,
                            "The GIF image was not initialized or does not "
                            "have more scanlines.");
  }

  // The first time ReadNextScanline() is called, we allocate a buffer
  // to store the decoded pixels. For a non-progressive (non-interlacing)
  // image, we need buffer to store a row of pixels. For a progressive image,
  // we need buffer to store the entire image; we also decode the entire image
  // during the first call.
  if (row_ == 0) {
    image_buffer_.reset(new GifPixelType[bytes_per_row_]);
    if (!is_progressive_) {
      image_index_.reset(new GifPixelType[GetImageWidth()]);
    } else {
      image_index_.reset(new GifPixelType[GetImageWidth() * GetImageHeight()]);

      // For a progressive GIF, we have to decode the entire image before
      // rendering any row.
      ScanlineStatus status = DecodeProgressiveGif();
      if (!status.Success()) {
        PS_LOG_INFO(message_handler_, "Failed to progressively decode GIF.");
        Reset();
        return status;
      }
    }

    if (image_buffer_ == NULL || image_index_ == NULL) {
      Reset();
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler_,
                              SCANLINE_STATUS_MEMORY_ERROR,
                              SCANLINE_GIFREADERRAW,
                              "new GiPixelType[] for image_buffer_ "
                              "or image_index_");
    }
  }

  // Convert the color index to the actual color.
  GifPixelType* color_buffer = image_buffer_.get();
  const PaletteRGBA* background_color = gif_palette_.get() +
                                        kPaletteBackgroundIndex;
  int pixel_index = 0;
  if (row_ >= gif_struct_->first_row() && row_ <= gif_struct_->last_row()) {
    // Find out the color index for the requested row.
    GifPixelType* index_buffer = NULL;
    GifFileType* gif_file = gif_struct_->gif_file();
    int actual_width = gif_struct_->last_col() - gif_struct_->first_col() + 1;
    if (!is_progressive_) {
      // For a non-progressive GIF, we decode the image a row at a time.
      index_buffer = image_index_.get();
      if (DGifGetLine(gif_file, index_buffer, actual_width) == GIF_ERROR) {
        Reset();
        return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                                SCANLINE_STATUS_INTERNAL_ERROR,
                                SCANLINE_GIFREADERRAW,
                                "DGifGetLine()");
      }
    } else {
      // For a progressive GIF, we simply point the output to the corresponding
      // row, because the image has already been decoded.
      index_buffer = image_index_.get() + row_ * GetImageWidth();
    }

    for (; pixel_index < gif_struct_->first_col(); ++pixel_index) {
      // Pad background color to the beginning of the row.
      memcpy(color_buffer, background_color, pixel_size_);
      color_buffer += pixel_size_;
    }
    for (; pixel_index <= gif_struct_->last_col(); ++pixel_index) {
      // Convert the color index to the actual color.
      int color_index = *(index_buffer++);
      memcpy(color_buffer, gif_palette_.get() + color_index, pixel_size_);
      color_buffer += pixel_size_;
    }
  }

  // Pad background color to the end of the row if the current row contains
  // valid output pixels, or to the entire row if not.
  for (; pixel_index < static_cast<int>(GetImageWidth()); ++pixel_index) {
    memcpy(color_buffer, background_color, pixel_size_);
    color_buffer += pixel_size_;
  }

  *out_scanline_bytes = static_cast<void*>(image_buffer_.get());
  ++row_;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

}  // namespace image_compression

}  // namespace pagespeed
