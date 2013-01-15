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

// Author: Bryan McQuade, Matthew Steele

#ifndef JPEG_READER_H_
#define JPEG_READER_H_

#include <string>

#include "base/basictypes.h"

struct jpeg_decompress_struct;
struct jpeg_error_mgr;

namespace pagespeed {

namespace image_compression {

// A very thin wrapper that configures a jpeg_decompress_struct for
// reading from a std::string. The caller is responsible for
// configuring a jmp_buf and setting it as the client_data of the
// jpeg_decompress_struct, like so:
//
// jmp_buf env;
// if (setjmp(env)) {
//   // error handling
// }
// JpegReader reader;
// jpeg_decompress_struct* jpeg_decompress = reader.decompress_struct();
// jpeg_decompress->client_data = static_cast<void*>(&env);
// reader.PrepareForRead(src);
// // perform other operations on jpeg_decompress.
class JpegReader {
public:
  JpegReader();
  ~JpegReader();

  jpeg_decompress_struct *decompress_struct() const { return jpeg_decompress_; }

  void PrepareForRead(const std::string& src);

private:
  jpeg_decompress_struct *jpeg_decompress_;
  jpeg_error_mgr *decompress_error_;

  DISALLOW_COPY_AND_ASSIGN(JpegReader);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // JPEG_READER_H_
