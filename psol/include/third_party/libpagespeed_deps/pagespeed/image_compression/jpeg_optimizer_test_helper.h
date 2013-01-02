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
//
// This header only exists to avoid including jpeglib.h, etc directly
// in jpeg_optimizer_test.cc, since doing so causes symbol collisions on
// Windows.

#ifndef PAGESPEED_IMAGE_COMPRESSION_JPEG_OPTIMIZER_TEST_HELPER_H_
#define PAGESPEED_IMAGE_COMPRESSION_JPEG_OPTIMIZER_TEST_HELPER_H_

#include <string>

namespace pagespeed_testing {
namespace image_compression {

// Helper that extracts the number of components and h/v sampling factors.
bool GetJpegNumComponentsAndSamplingFactors(
    const std::string& jpeg,
    int* out_num_components,
    int* out_h_samp_factor,
    int* out_v_samp_factor);

// Helper function to check present of given segment.
bool IsJpegSegmentPresent(const std::string& data, int segment);

// Helper function that returns num of progessive scans for the image.
int GetNumScansInJpeg(const std::string& data);

// Helper function to return the color profile segment marker.
int GetColorProfileMarker();

// Helper function to return the exif data segment marker.
int GetExifDataMarker();

}  // namespace image_compression
}  // namespace pagespeed_testing

#endif  // PAGESPEED_IMAGE_COMPRESSION_JPEG_OPTIMIZER_TEST_HELPER_H_
