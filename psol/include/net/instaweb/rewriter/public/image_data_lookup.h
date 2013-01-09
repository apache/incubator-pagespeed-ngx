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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_DATA_LOOKUP_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_DATA_LOOKUP_H_

#include <cstddef>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

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

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_DATA_LOOKUP_H_
