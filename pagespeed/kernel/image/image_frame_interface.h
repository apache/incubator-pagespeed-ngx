/*
 * Copyright 2014 Google Inc.
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

// Author: Victor Chudnovsky

// This file defines the MultipleFrame API for reading
// and writing static and animated images.

#ifndef PAGESPEED_KERNEL_IMAGE_IMAGE_FRAME_INTERFACE_H_
#define PAGESPEED_KERNEL_IMAGE_IMAGE_FRAME_INTERFACE_H_

#include <cstddef>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/scanline_status.h"

namespace net_instaweb {
class MessageHandler;
}

namespace pagespeed {

namespace image_compression {

using net_instaweb::MessageHandler;

struct FrameSpec;

struct ImageSpec {
  ImageSpec();
  void Reset();

  size_px width;
  size_px height;
  size_px num_frames;

  // This is the total number of times to loop through all the frames
  // (NOT the *repeat* number).
  unsigned int loop_count;

  PixelRgbaChannels bg_color;
  bool use_bg_color;

  // Whether the image size was adjusted (as can happen when
  // implementing some quirks modes).
  bool image_size_adjusted;

  // Returns x truncated to a valid column index in [0, width]. Note
  // that a value of 'width' denotes the first invalid index.
  size_px TruncateXIndex(size_px x) const;

  // Returns y truncated to a valid row index in [0, height]. Note
  // that a value of 'height' denotes the first invalid index.
  size_px TruncateYIndex(size_px y) const;

  // Returns true iff frame_spec fits entirely within this ImageSpec.
  bool CanContainFrame(const FrameSpec& frame_spec) const;

  GoogleString ToString() const;
  bool Equals(const ImageSpec& other) const;
};

// FrameSpec must fit entirely within its image; in other words
// ImageSpec::CanContainFrame(frame_spec) should return true.
struct FrameSpec {
  enum DisposalMethod {
    DISPOSAL_UNKNOWN = 0,
    DISPOSAL_NONE,
    DISPOSAL_BACKGROUND,

    // TODO(vchudnov): May not be supported by WebP. If that's the
    // case, treat as DISPOSAL_BACKGROUND instead.
    DISPOSAL_RESTORE
  };

  FrameSpec();
  void Reset();

  size_px width;
  size_px height;
  size_px top;
  size_px left;
  PixelFormat pixel_format;
  size_t duration_ms;
  DisposalMethod disposal;

  // Whether this frame was progressively encoded by the origin site, so that
  // it could begin to be rendered even before the entire image was
  // transferred. This does not affect the data format passed in or out of
  // this API, but provides a hint that API clients may choose to take.
  bool hint_progressive;

  GoogleString ToString() const;
  bool Equals(const FrameSpec& other) const;
};

#if defined(IF_OK_RUN)
#error "Preprocessor macro collision."
#endif

// Boilerplate for the convenience methods below. This code snippet
// invokes _CALL only if _STATUS does not indicate a pre-existing
// error, and returns a bool indicating success or failure of the
// preceding state as well as _CALL, if invoked. Note that this
// snippet returns from the function in which it is embedded.
#define IF_OK_RUN(_STATUS, _CALL)                               \
  if (!_STATUS->Success()) {                                    \
    return false;                                               \
  }                                                             \
  *_STATUS = _CALL;                                             \
  return _STATUS->Success();

// Interface for reading both animated and static images.
//
// Typical usage of this API is as follows:
//
//   Initialize()
//   GetImageSpec()  // optional
//   while (HasMoreFrames()) {
//     PrepareNextFrame()
//     GetFrameSpec()  // optional
//     while (HasMoreScanlines()) {
//       ReadNextScanline()
//     }
//   }
//
// The "optional" lines above are not necessary for just reading an
// image. You might want to call them, though, if the metadata they
// contain is of interest, or if you're passing the read image to a
// MultipleFrameWriter, which needs the metadata in ImageSpec and
// FrameSpec (see below).
class MultipleFrameReader {
 public:
  explicit MultipleFrameReader(MessageHandler* handler);
  virtual ~MultipleFrameReader();

  // Resets the MultipleFrameReader to its initial state.
  virtual ScanlineStatus Reset() = 0;

  // Initializes MultipleFrameReader to read image data of length
  // 'buffer_length_' from 'image_buffer_'. This function should take
  // care of calling Reset() if necessary, so that the sequence
  // "Reset(); Initialize();" is never needed.
  virtual ScanlineStatus Initialize() = 0;

  // Sets 'buffer_length_' and 'image_buffer_' and calls
  // Initialize(). Do not override this function; override the no-arg
  // overload instead.
  ScanlineStatus Initialize(const void* image_buffer, size_t buffer_length) {
    image_buffer_ = image_buffer;
    buffer_length_ = buffer_length;
    return Initialize();
  }

  // Returns true iff the image being read has additional frames beyond
  // the current frame being read. For any well-formed image with at
  // least one frame (or for a well-formed static image), this will
  // return true before the first call to PrepareNextFrame().
  virtual bool HasMoreFrames() const = 0;

  // Returns true iff the current frame has more scanlines that have
  // not yet been read.
  virtual bool HasMoreScanlines() const = 0;

  // Prepares to read scanlines from the frame after the current
  // one. Must be called before reading from the first frame.
  virtual ScanlineStatus PrepareNextFrame() = 0;

  // Reads the next available scanline in the current frame and copies
  // a pointer to it into '*out_scanline_bytes'. This class retains
  // ownership of the read scanline. The scanline encodes as many
  // pixels as the width of the current frame, which is not
  // necessarily the width of the whole image.
  virtual ScanlineStatus ReadNextScanline(const void** out_scanline_bytes) = 0;

  // Assigns to '*frame_spec' the FrameSpec describing
  // the current frame.
  //
  // TODO(vchudnov): Consider simplifying this method to return
  // frame_spec rather than the ScanlineStatus.
  virtual ScanlineStatus GetFrameSpec(FrameSpec* frame_spec) const = 0;

  // Copies into '*image_spec' the ImageSpec describing
  // the image.
  virtual ScanlineStatus GetImageSpec(ImageSpec* image_spec) const = 0;

  // The message handler used by this class. Neither the caller nor
  // this class have ownership.
  MessageHandler* message_handler() const {
    return message_handler_;
  }

  virtual ScanlineStatus set_quirks_mode(QuirksMode quirks_mode) {
    quirks_mode_ = quirks_mode;
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }

  virtual QuirksMode quirks_mode() const {
    return quirks_mode_;
  }

  // Convenience forms of the methods above. If 'status' indicates an
  // error on entry, each of these methods does nothing and returns
  // false. Otherwise, it calls the corresponding method above,
  // updates 'status', and returns true iff the call succeeded.

  bool Reset(ScanlineStatus* status) {
    IF_OK_RUN(status, Reset());
  }
  bool Initialize(const void* image_buffer, size_t buffer_length,
                  ScanlineStatus* status) {
    IF_OK_RUN(status, Initialize(image_buffer, buffer_length));
  }
  bool Initialize(ScanlineStatus* status) {
    IF_OK_RUN(status, Initialize());
  }
  bool PrepareNextFrame(ScanlineStatus* status) {
    IF_OK_RUN(status, PrepareNextFrame());
  }
  bool ReadNextScanline(const void** out_scanline_bytes,
                        ScanlineStatus* status) {
    IF_OK_RUN(status, ReadNextScanline(out_scanline_bytes));
  }
  bool GetFrameSpec(FrameSpec* frame_spec,
                    ScanlineStatus* status) {
    IF_OK_RUN(status, GetFrameSpec(frame_spec));
  }
  bool GetImageSpec(ImageSpec* image_spec,
                    ScanlineStatus* status) {
    IF_OK_RUN(status, GetImageSpec(image_spec));
  }

  bool set_quirks_mode(QuirksMode quirks_mode,
                       ScanlineStatus* status) {
    IF_OK_RUN(status, set_quirks_mode(quirks_mode));
  }

 protected:
  const void* image_buffer_;
  size_t buffer_length_;

 private:
  // Handles logging info, warning, and error messages.
  MessageHandler* message_handler_;

  // The browser quirks mode to implement, if any.
  QuirksMode quirks_mode_;

  DISALLOW_COPY_AND_ASSIGN(MultipleFrameReader);
};


// Interface for writing both animated and static images.
//
// Typical usage of this API is as follows:
//   Initialize();
//   PrepareImage();
//   while (have_frame) {
//     PrepareNextFrame();
//     while (have_scanline) {
//         WriteNextScanline();
//     }
//   }
//   FinalizeWrite();
class MultipleFrameWriter {
 public:
  explicit MultipleFrameWriter(MessageHandler* handler);
  virtual ~MultipleFrameWriter();

  // Initializes the writer to use the format-specific configuration
  // in 'config' to write an image to 'out'. It is up to the client to
  // ensure 'config' points to data of the right type for the given
  // child class it invokes.
  virtual ScanlineStatus Initialize(const void* config,
                                    GoogleString* out) = 0;

  // Prepares to write an image with the characteristics in
  // 'image_spec'.
  virtual ScanlineStatus PrepareImage(const ImageSpec* image_spec) = 0;

  // Prepares to write scanlines to the frame after the current one by
  // setting its properties to 'frame_spec'. Must be called before
  // writing to the first frame.
  virtual ScanlineStatus PrepareNextFrame(const FrameSpec* frame_spec) = 0;

  // Writes 'scanline_bytes' to the next scanline of the current
  // frame. The scanline's width in pixels and pixel format (and thus,
  // its width in bytes) are as set in the preceding call to
  // PrepareNextFrame(); that many bytes from '*scanline_bytes' are
  // read.
  virtual ScanlineStatus WriteNextScanline(const void *scanline_bytes) = 0;

  // Finalizes the image once all the frames have been written.
  virtual ScanlineStatus FinalizeWrite() = 0;

  // The message handler used by this class. Neither the caller nor
  // this class have ownership.
  MessageHandler* message_handler() const {
    return message_handler_;
  }

  // Convenience forms of the methods above. If 'status' indicates an
  // error on entry, each of these methods does nothing and returns
  // false. Otherwise, it calls the corresponding method above,
  // updates 'status', and returns true iff the call succeeded.

  bool Initialize(const void* config, GoogleString* out,
                  ScanlineStatus* status) {
    IF_OK_RUN(status, Initialize(config, out));
  }
  bool PrepareImage(const ImageSpec* image_spec, ScanlineStatus* status) {
    IF_OK_RUN(status, PrepareImage(image_spec));
  }
  bool PrepareNextFrame(const FrameSpec* frame_spec, ScanlineStatus* status) {
    IF_OK_RUN(status, PrepareNextFrame(frame_spec));
  }
  bool WriteNextScanline(const void* scanline_bytes, ScanlineStatus* status) {
    IF_OK_RUN(status, WriteNextScanline(scanline_bytes));
  }
  bool FinalizeWrite(ScanlineStatus* status) {
    IF_OK_RUN(status, FinalizeWrite());
  }

 private:
  // Handles logging info, warning, and error messages.
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(MultipleFrameWriter);
};

#undef IF_OK_RUN

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_IMAGE_FRAME_INTERFACE_H_
