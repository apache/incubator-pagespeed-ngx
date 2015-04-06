// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pagespeed/kernel/image/jpeg_optimizer_test_helper.h"

#include <setjmp.h>
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/image/jpeg_reader.h"

extern "C" {
#ifdef USE_SYSTEM_LIBJPEG
#include "jpeglib.h"                                                 // NOLINT
#else
#include "third_party/libjpeg_turbo/src/jpeglib.h"
#endif
}

using net_instaweb::MockMessageHandler;
using net_instaweb::NullMutex;
using pagespeed::image_compression::JpegReader;

namespace pagespeed_testing {
namespace image_compression {

// Marker for APPN segment can obtained by adding N to JPEG_APP0. There is no
// direct constant to refer them. The offsets here are part of jpeg codex, for
// example JPEG_APP0 + 2 refers to APP2 which should always correspond to color
// profile information.
const int kColorProfileMarker = JPEG_APP0 + 2;
const int kExifDataMarker = JPEG_APP0 + 1;

bool GetJpegNumComponentsAndSamplingFactors(
    const GoogleString& jpeg,
    int* out_num_components,
    int* out_h_samp_factor,
    int* out_v_samp_factor) {
  MockMessageHandler message_handler_(new NullMutex);
  JpegReader reader(&message_handler_);
  jpeg_decompress_struct* jpeg_decompress = reader.decompress_struct();

  jmp_buf env;
  if (setjmp(env)) {
    return false;
  }

  // Need to install env so that it will be longjmp()ed to on error.
  jpeg_decompress->client_data = static_cast<void *>(&env);

  reader.PrepareForRead(jpeg.data(), jpeg.size());
  jpeg_read_header(jpeg_decompress, TRUE);
  *out_num_components = jpeg_decompress->num_components;
  *out_h_samp_factor = jpeg_decompress->comp_info[0].h_samp_factor;
  *out_v_samp_factor = jpeg_decompress->comp_info[0].v_samp_factor;
  return true;
}

bool IsJpegSegmentPresent(const GoogleString& data, int segment) {
  MockMessageHandler message_handler_(new NullMutex);
  JpegReader reader(&message_handler_);
  jpeg_decompress_struct* jpeg_decompress = reader.decompress_struct();

  jmp_buf env;
  if (setjmp(env)) {
    return false;
  }

  // Need to install env so that it will be longjmp()ed to on error.
  jpeg_decompress->client_data = static_cast<void *>(&env);

  reader.PrepareForRead(data.data(), data.size());
  jpeg_save_markers(jpeg_decompress, segment, 0xFFFF);
  jpeg_read_header(jpeg_decompress, TRUE);

  bool is_marker_present = false;
  for (jpeg_saved_marker_ptr marker = jpeg_decompress->marker_list;
       marker != NULL; marker = marker->next) {
    if (marker->marker == segment) {
      is_marker_present = true;
      break;
    }
  }

  return is_marker_present;
}

int GetNumScansInJpeg(const GoogleString& data) {
  MockMessageHandler message_handler_(new NullMutex);
  JpegReader reader(&message_handler_);
  jpeg_decompress_struct* jpeg_decompress = reader.decompress_struct();

  jmp_buf env;
  if (setjmp(env)) {
    return false;
  }

  // Need to install env so that it will be longjmp()ed to on error.
  jpeg_decompress->client_data = static_cast<void *>(&env);

  reader.PrepareForRead(data.data(), data.size());
  jpeg_read_header(jpeg_decompress, TRUE);

  jpeg_decompress->buffered_image = true;
  jpeg_start_decompress(jpeg_decompress);

  int num_scans = 0;
  while (!jpeg_input_complete(jpeg_decompress)) {
    if (jpeg_consume_input(jpeg_decompress) == JPEG_SCAN_COMPLETED) {
      num_scans++;
    }
  }

  return num_scans;
}

int GetColorProfileMarker() {
  return kColorProfileMarker;
}

int GetExifDataMarker() {
  return kExifDataMarker;
}

}  // namespace image_compression
}  // namespace pagespeed_testing

