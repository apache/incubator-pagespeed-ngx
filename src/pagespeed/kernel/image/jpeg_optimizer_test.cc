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

// Author: Bryan McQuade, Matthew Steele

#include <vector>

#include "base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/jpeg_optimizer.h"
#include "pagespeed/kernel/image/jpeg_optimizer_test_helper.h"
#include "pagespeed/kernel/image/test_utils.h"

// DO NOT INCLUDE LIBJPEG HEADERS HERE. Doing so causes build errors
// on Windows. If you need to call out to libjpeg, please add helper
// methods in jpeg_optimizer_test_helper.h.

namespace {

using pagespeed::image_compression::ColorSampling;
using pagespeed::image_compression::JpegCompressionOptions;
using pagespeed::image_compression::JpegLossyOptions;
using pagespeed::image_compression::kJpegTestDir;
using pagespeed::image_compression::OptimizeJpeg;
using pagespeed::image_compression::OptimizeJpegWithOptions;
using pagespeed::image_compression::ReadTestFileWithExt;
using pagespeed_testing::image_compression::GetNumScansInJpeg;
using pagespeed_testing::image_compression::GetJpegNumComponentsAndSamplingFactors;  // NOLINT
using pagespeed_testing::image_compression::IsJpegSegmentPresent;
using pagespeed_testing::image_compression::GetColorProfileMarker;
using pagespeed_testing::image_compression::GetExifDataMarker;

// The JPEG_TEST_DIR_PATH macro is set by the gyp target that builds this file.
const char kAppSegmentsJpegFile[] = "app_segments.jpg";

struct ImageCompressionInfo {
  const char* filename;
  size_t original_size;
  size_t compressed_size;
  size_t lossy_compressed_size;
  size_t progressive_size;
  size_t progressive_and_lossy_compressed_size;
};

ImageCompressionInfo kValidImages[] = {
  { "sjpeg1.jpg", 1552, 1536, 1165, 1774, 1410 },
  { "sjpeg3.jpg", 44084, 41664, 26924, 40997, 25814 },
  { "sjpeg6.jpg", 149600, 147163, 89671, 146038, 84641 },
  { "testgray.jpg", 5014, 3072, 3060, 3094, 3078 },
  { "sjpeg2.jpg", 3612, 3283, 3630, 3475, 3798 },
  { "sjpeg4.jpg", 168895, 168240, 51389, 162867, 49186 },
  { "test411.jpg", 6883, 4367, 3709, 4540, 3854 },
  { "test420.jpg", 6173, 3657, 3653, 3796, 3793 },
  { "test422.jpg", 6501, 3985, 3712, 4152, 3849 },
};

const char *kInvalidFiles[] = {
  "notajpeg.png",   // A png.
  "notajpeg.gif",   // A gif.
  "emptyfile.jpg",  // A zero-byte file.
  "corrupt.jpg",    // Invalid huffman code in the image data section.
};

const size_t kValidImageCount = arraysize(kValidImages);
const size_t kInvalidFileCount = arraysize(kInvalidFiles);

void AssertColorSampling(const GoogleString& data,
                         int expected_h_sampling_factor,
                         int expected_v_sampling_factor) {
  int num_components, h_sampling_factor, v_sampling_factor;
  ASSERT_TRUE(GetJpegNumComponentsAndSamplingFactors(data,
                                                     &num_components,
                                                     &h_sampling_factor,
                                                     &v_sampling_factor));
  ASSERT_LE(1, num_components);
  ASSERT_EQ(expected_h_sampling_factor, h_sampling_factor);
  ASSERT_EQ(expected_v_sampling_factor, v_sampling_factor);
}

void AssertJpegOptimizeWithSampling(
    const GoogleString &src_data, GoogleString* dest_data,
    ColorSampling color_sampling, int h_sampling_factor,
    int v_sampling_factor) {
  dest_data->clear();
  JpegCompressionOptions options;
  options.lossy = true;
  options.lossy_options.color_sampling = color_sampling;

  ASSERT_TRUE(OptimizeJpegWithOptions(src_data, dest_data, options));
  AssertColorSampling(*dest_data, h_sampling_factor, v_sampling_factor);
}

TEST(JpegOptimizerTest, ValidJpegs) {
  for (size_t i = 0; i < kValidImageCount; ++i) {
    GoogleString src_data;
    ReadTestFileWithExt(kJpegTestDir, kValidImages[i].filename, &src_data);
    GoogleString dest_data;
    ASSERT_TRUE(OptimizeJpeg(src_data, &dest_data));
    EXPECT_EQ(kValidImages[i].original_size, src_data.size())
        << kValidImages[i].filename;
    EXPECT_EQ(kValidImages[i].compressed_size, dest_data.size())
        << kValidImages[i].filename;

    ASSERT_LE(dest_data.size(), src_data.size());
  }
}

TEST(JpegOptimizerTest, ValidJpegsLossy) {
  for (size_t i = 0; i < kValidImageCount; ++i) {
    GoogleString src_data;
    ReadTestFileWithExt(kJpegTestDir, kValidImages[i].filename, &src_data);
    pagespeed::image_compression::JpegCompressionOptions options;
    options.lossy = true;
    GoogleString dest_data;
    ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options))
        << kValidImages[i].filename;
    EXPECT_EQ(kValidImages[i].original_size, src_data.size())
        << kValidImages[i].filename;
    EXPECT_EQ(kValidImages[i].lossy_compressed_size, dest_data.size())
        << kValidImages[i].filename;
  }
}

TEST(JpegOptimizerTest, ValidJpegLossyAndColorSampling) {
  int test_422_file_idx = 8;
  GoogleString src_data;
  GoogleString src_filename = kValidImages[test_422_file_idx].filename;
  ReadTestFileWithExt(kJpegTestDir, src_filename.c_str(), &src_data);

  JpegCompressionOptions options;
  options.lossy = true;

  GoogleString dest_data;
  // Calling optimize will use default color sampling which is 420.
  ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  size_t lossy_420_size =
      kValidImages[test_422_file_idx].lossy_compressed_size;
  EXPECT_EQ(lossy_420_size, dest_data.size()) << src_filename;
  AssertColorSampling(dest_data, 2, 2);

  // Calling optimize with ColorSampling::YUV420 will give samping as 420.
  AssertJpegOptimizeWithSampling(src_data, &dest_data,
                                 pagespeed::image_compression::YUV420, 2, 2);
  EXPECT_EQ(lossy_420_size, dest_data.size()) << src_filename;

  // Calling optimize with ColorSampling::RETAIN will leave samping as 422.
  AssertJpegOptimizeWithSampling(src_data, &dest_data,
                                 pagespeed::image_compression::RETAIN, 2, 1);
  size_t lossy_retain_size = dest_data.size();
  EXPECT_GT(lossy_retain_size, lossy_420_size) << src_filename;

  // Calling optimize with ColorSampling::YUV422 will give samping as 422.
  AssertJpegOptimizeWithSampling(src_data, &dest_data,
                                 pagespeed::image_compression::YUV422, 2, 1);
  EXPECT_EQ(lossy_retain_size, dest_data.size()) << src_filename;

  // Calling optimize with ColorSampling::YUV444 will give samping as 444.
  AssertJpegOptimizeWithSampling(src_data, &dest_data,
                                 pagespeed::image_compression::YUV444, 1, 1);
  EXPECT_LT(lossy_retain_size, dest_data.size()) << src_filename;
}

TEST(JpegOptimizerTest, ValidJpegRetainColorProfile) {
  GoogleString src_data;
  ReadTestFileWithExt(kJpegTestDir, kAppSegmentsJpegFile, &src_data);

  GoogleString dest_data;

  // Testing lossless flow.
  JpegCompressionOptions options;
  options.retain_color_profile = true;

  ASSERT_TRUE(IsJpegSegmentPresent(src_data, GetColorProfileMarker()));
  ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  ASSERT_TRUE(IsJpegSegmentPresent(dest_data, GetColorProfileMarker()));

  options.retain_color_profile = false;
  dest_data.clear();
  ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  ASSERT_FALSE(IsJpegSegmentPresent(dest_data, GetColorProfileMarker()));

  // Testing lossy flow.
  options.lossy = true;
  options.retain_color_profile = true;
  dest_data.clear();
  ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  ASSERT_TRUE(IsJpegSegmentPresent(dest_data, GetColorProfileMarker()));

  options.retain_color_profile = false;
  dest_data.clear();
  ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  ASSERT_FALSE(IsJpegSegmentPresent(dest_data, GetColorProfileMarker()));
}

TEST(JpegOptimizerTest, ValidJpegRetainExifData) {
  GoogleString src_data;
  ReadTestFileWithExt(kJpegTestDir, kAppSegmentsJpegFile, &src_data);

  GoogleString dest_data;

  // Testing lossless flow.
  JpegCompressionOptions options;
  options.retain_exif_data = true;

  ASSERT_TRUE(IsJpegSegmentPresent(src_data, GetExifDataMarker()));
  ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  ASSERT_TRUE(IsJpegSegmentPresent(dest_data, GetExifDataMarker()));

  options.retain_exif_data = false;
  dest_data.clear();
  ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  ASSERT_FALSE(IsJpegSegmentPresent(dest_data, GetExifDataMarker()));

  // Testing lossy flow.
  options.lossy = true;
  options.retain_exif_data = true;
  dest_data.clear();
  ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  ASSERT_TRUE(IsJpegSegmentPresent(dest_data, GetExifDataMarker()));

  options.retain_exif_data = false;
  dest_data.clear();
  ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  ASSERT_FALSE(IsJpegSegmentPresent(dest_data, GetExifDataMarker()));
}

TEST(JpegOptimizerTest, ValidJpegLossyWithNProgressiveScans) {
  GoogleString src_data;
  ReadTestFileWithExt(kJpegTestDir, kAppSegmentsJpegFile, &src_data);

  GoogleString dest_data;

  // Testing lossless flow.
  JpegCompressionOptions options;
  options.progressive = true;

  EXPECT_EQ(1, GetNumScansInJpeg(src_data));
  ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  int num_scans = GetNumScansInJpeg(dest_data);
  EXPECT_LT(1, num_scans);

  dest_data.clear();
  options.lossy = true;
  options.lossy_options.num_scans = 3;
  ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  EXPECT_EQ(3, GetNumScansInJpeg(dest_data));

  dest_data.clear();
  // jpeg has max scan limit based on image color spaces, So trying to set
  // num scans to a large value should be handled gracefully and default to
  // jpeg scan limit if the specified value is greater.
  options.lossy_options.num_scans = 1000;
  ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  EXPECT_EQ(num_scans, GetNumScansInJpeg(dest_data));
}

TEST(JpegOptimizerTest, ValidJpegsProgressive) {
  for (size_t i = 0; i < kValidImageCount; ++i) {
    GoogleString src_data;
    ReadTestFileWithExt(kJpegTestDir, kValidImages[i].filename, &src_data);
    pagespeed::image_compression::JpegCompressionOptions options;
    options.progressive = true;
    GoogleString dest_data;
    ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options))
        << kValidImages[i].filename;
    EXPECT_EQ(kValidImages[i].original_size, src_data.size())
        << kValidImages[i].filename;
    EXPECT_EQ(kValidImages[i].progressive_size, dest_data.size())
        << kValidImages[i].filename;
  }
}

TEST(JpegOptimizerTest, ValidJpegsProgressiveAndLossy) {
  for (size_t i = 0; i < kValidImageCount; ++i) {
    GoogleString src_data;
    ReadTestFileWithExt(kJpegTestDir, kValidImages[i].filename, &src_data);
    pagespeed::image_compression::JpegCompressionOptions options;
    options.lossy = true;
    options.progressive = true;
    GoogleString dest_data;
    ASSERT_TRUE(OptimizeJpegWithOptions(src_data, &dest_data, options))
        << kValidImages[i].filename;
    EXPECT_EQ(kValidImages[i].original_size, src_data.size())
        << kValidImages[i].filename;
    EXPECT_EQ(kValidImages[i].progressive_and_lossy_compressed_size,
              dest_data.size()) << kValidImages[i].filename;
  }
}

TEST(JpegOptimizerTest, InvalidJpegs) {
  for (size_t i = 0; i < kInvalidFileCount; ++i) {
    GoogleString src_data;
    ReadTestFileWithExt(kJpegTestDir, kInvalidFiles[i], &src_data);
    GoogleString dest_data;
    ASSERT_FALSE(OptimizeJpeg(src_data, &dest_data));
  }
}

TEST(JpegOptimizerTest, InvalidJpegsLossy) {
  for (size_t i = 0; i < kInvalidFileCount; ++i) {
    GoogleString src_data;
    ReadTestFileWithExt(kJpegTestDir, kInvalidFiles[i], &src_data);
    JpegCompressionOptions options;
    options.lossy = true;
    GoogleString dest_data;
    ASSERT_FALSE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  }
}

TEST(JpegOptimizerTest, InvalidJpegsProgressive) {
  for (size_t i = 0; i < kInvalidFileCount; ++i) {
    GoogleString src_data;
    ReadTestFileWithExt(kJpegTestDir, kInvalidFiles[i], &src_data);
    JpegCompressionOptions options;
    options.progressive = true;
    GoogleString dest_data;
    ASSERT_FALSE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  }
}

TEST(JpegOptimizerTest, InvalidJpegsProgressiveAndLossy) {
  for (size_t i = 0; i < kInvalidFileCount; ++i) {
    GoogleString src_data;
    ReadTestFileWithExt(kJpegTestDir, kInvalidFiles[i], &src_data);
    JpegCompressionOptions options;
    options.lossy = true;
    options.progressive = true;
    GoogleString dest_data;
    ASSERT_FALSE(OptimizeJpegWithOptions(src_data, &dest_data, options));
  }
}

// Test that after reading an invalid jpeg, the reader cleans its state so that
// it can read a correct jpeg again.
TEST(JpegOptimizerTest, CleanupAfterReadingInvalidJpeg) {
  // Compress each input image with a reinitialized JpegOptimizer.
  // We will compare these files with the output we get from
  // a JpegOptimizer that had an error.
  std::vector<GoogleString> correctly_compressed;
  for (size_t i = 0; i < kValidImageCount; ++i) {
    GoogleString src_data;
    ReadTestFileWithExt(kJpegTestDir, kValidImages[i].filename, &src_data);
    correctly_compressed.push_back("");
    GoogleString &dest_data = correctly_compressed.back();
    ASSERT_TRUE(OptimizeJpeg(src_data, &dest_data));
  }

  // The invalid files are all invalid in different ways, and we want to cover
  // all the ways jpeg decoding can fail.  So, we want at least as many valid
  // images as invalid ones.
  ASSERT_GE(kValidImageCount, kInvalidFileCount);

  for (size_t i = 0; i < kInvalidFileCount; ++i) {
    GoogleString invalid_src_data;
    ReadTestFileWithExt(kJpegTestDir, kInvalidFiles[i], &invalid_src_data);
    GoogleString invalid_dest_data;

    GoogleString valid_src_data;
    ReadTestFileWithExt(kJpegTestDir, kValidImages[i].filename,
                        &valid_src_data);
    GoogleString valid_dest_data;

    ASSERT_FALSE(OptimizeJpeg(invalid_src_data, &invalid_dest_data));
    ASSERT_TRUE(OptimizeJpeg(valid_src_data, &valid_dest_data));

    // Diff the jpeg created by CreateOptimizedJpeg() with the one created
    // with a reinitialized JpegOptimizer.
    ASSERT_EQ(valid_dest_data, correctly_compressed.at(i));
  }
}

}  // namespace
