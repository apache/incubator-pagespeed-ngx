/*
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

// Author: Satyanarayana Manyam

#include "pagespeed/kernel/image/jpeg_utils.h"

#include <setjmp.h>  // for longjmp
#include <stdio.h>  // provides FILE for jpeglib (needed for certain builds)

#include "base/logging.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/jpeg_reader.h"

extern "C" {
#ifdef USE_SYSTEM_LIBJPEG
#include "jerror.h"                                                 // NOLINT
#include "jpeglib.h"                                                // NOLINT
#else
#include "third_party/libjpeg/jerror.h"
#include "third_party/libjpeg/jpeglib.h"
#endif
}

namespace {
static double ComputeQualityEntriesSum(JQUANT_TBL* quantization_table,
                                       const unsigned int* std_table) {
  double quality_entries_sum = 0;

  // Quality is defined in terms of the base quantization tables used by
  // encoder. Q = quant table, q = compression quality  and S = table used by
  // encoder, Encoder does the following.
  // if q > 0.5 then Q = 2 - 2*q*S otherwise Q = (0.5/q)*S.
  //
  // Refer 'jpeg_add_quant_table (...)' in jcparam.c for more details.
  //
  // Since we dont have access to the table used by encoder. But it is generally
  // close to the standard table defined by JPEG. Hence, we apply inverse
  // function of the above to using standard table and compute the input image
  // jpeg quality.
  for (int i = 0; i < DCTSIZE2; i++) {
    if (quantization_table->quantval[i] == 1) {
      // 1 is the minimum denominator allowed for any value in the quantization
      // matrix and it implies that quality is set 100.
      quality_entries_sum += 1;
    } else {
      double scale_factor =
          static_cast<double>(quantization_table->quantval[i]) / std_table[i];
      quality_entries_sum += (scale_factor > 1.0 ?
                              (0.5/scale_factor) : ((2.0 - scale_factor)/2.0));
    }
  }

  return quality_entries_sum;
}
}  // namespace

namespace pagespeed {

namespace image_compression {

JpegUtils::JpegUtils() {
}

int JpegUtils::GetImageQualityFromImage(const void* image_data,
                                        size_t image_length) {
  JpegReader reader;
  jpeg_decompress_struct* jpeg_decompress = reader.decompress_struct();

  // libjpeg's error handling mechanism requires that longjmp be used
  // to get control after an error.
  jmp_buf env;
  if (setjmp(env)) {
    // This code is run only when libjpeg hit an error, and called
    // longjmp(env).
    return -1;
  }

  // Need to install env so that it will be longjmp()ed to on error.
  jpeg_decompress->client_data = static_cast<void *>(&env);

  reader.PrepareForRead(image_data, image_length);

  // Read jpeg data into the decompression struct.
  jpeg_read_header(jpeg_decompress, TRUE);

  // The standard tables are taken from JPEG spec section K.1.
  static const unsigned int std_luminance_quant_tbl[DCTSIZE2] = {
      16,  11,  10,  16,  24,  40,  51,  61,
      12,  12,  14,  19,  26,  58,  60,  55,
      14,  13,  16,  24,  40,  57,  69,  56,
      14,  17,  22,  29,  51,  87,  80,  62,
      18,  22,  37,  56,  68, 109, 103,  77,
      24,  35,  55,  64,  81, 104, 113,  92,
      49,  64,  78,  87, 103, 121, 120, 101,
      72,  92,  95,  98, 112, 100, 103,  99
  };
  static const unsigned int std_chrominance_quant_tbl[DCTSIZE2] = {
      17,  18,  24,  47,  99,  99,  99,  99,
      18,  21,  26,  66,  99,  99,  99,  99,
      24,  26,  56,  99,  99,  99,  99,  99,
      47,  66,  99,  99,  99,  99,  99,  99,
      99,  99,  99,  99,  99,  99,  99,  99,
      99,  99,  99,  99,  99,  99,  99,  99,
      99,  99,  99,  99,  99,  99,  99,  99,
      99,  99,  99,  99,  99,  99,  99,  99
  };

  double quality_entries_sum = 0;
  double quality_entries_count = 0;
  if (jpeg_decompress->quant_tbl_ptrs[0] != NULL) {
    quality_entries_sum += ComputeQualityEntriesSum(
        jpeg_decompress->quant_tbl_ptrs[0], std_luminance_quant_tbl);
    quality_entries_count += DCTSIZE2;
  }

  if (jpeg_decompress->quant_tbl_ptrs[1] != NULL) {
    quality_entries_sum += ComputeQualityEntriesSum(
        jpeg_decompress->quant_tbl_ptrs[1], std_chrominance_quant_tbl);
    quality_entries_count += DCTSIZE2;
  }

  if (quality_entries_count > 0) {
    // This computed quality is in the form of a fraction, so multiplying with
    // 100 and rounding off to nearest integer.
    double quality = quality_entries_sum * 100 / quality_entries_count;
    return static_cast<int>(quality + 0.5);
  }

  return -1;
}



}  // namespace image_compression

}  // namespace pagespeed
