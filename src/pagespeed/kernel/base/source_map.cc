// Copyright 2013 Google Inc. All Rights Reserved.
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
// Author: sligocki@google.com (Shawn Ligocki)

#include "pagespeed/kernel/base/source_map.h"


#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/json.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

namespace source_map {

char EncodeBase64(int val) {
  // Note: This constant also exists in
  // third_party/instaweb/src/third_party/base64/base64.cc
  // We have elected not to share it.
  static const char base64_chars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";
  if (val < 0 || val >= 64) {
    LOG(DFATAL) << "Invalid value passed into EncodeBase64 " << val;
    return '?';
  }
  return base64_chars[val];
}

GoogleString EncodeVlq(const int32 input_val) {
  // Base64 has 6-bits of data per char.
  // For VLQ Base64 encoding, the top (6th) bit is the continuation bit and the
  // bottom 5 bits are data bits.
  static const int kNumDataBits = 5;
  static const int kContinuationBit = 1 << kNumDataBits;  // 100000
  static const int kDataMask = kContinuationBit - 1;       // 011111

#ifndef NDEBUG
  for (int i = 0; i < kNumDataBits; ++i) {
    DCHECK(kDataMask & (1 << i)) << kDataMask;
  }
#endif

  GoogleString result;

  // We use a 64-bit int to avoid overflow issues. Note that input_val is
  // restricted to 32-bit so there is significant padding.
  // So (val = -val) and (val <<= 1) below are both safe.
  int64 val = input_val;

  // Sign is stored in low bit.
  bool is_negative = (val < 0);
  if (is_negative) {
    val = -val;
  }
  val <<= 1;
  if (is_negative) {
    val |= 0x1;
  }
  DCHECK(val >= 0) << val;

  // Little endian VLQ format.
  while (val > kDataMask) {
    int block = val & kDataMask;
    block |= kContinuationBit;
    result += EncodeBase64(block);
    DCHECK(val >= 0) << val;  // >> acts strangely with negative values.
    val >>= kNumDataBits;
  }
  DCHECK((val & kContinuationBit) == 0) << val;
  result += EncodeBase64(val);

  return result;
}

// Encode to the compact mappings format, which is a ;-separated list of
// ,-separated lists of base64 VLQ values.
bool EncodeMappings(const std::vector<Mapping>& mappings,
                    GoogleString* result) {
  int current_gen_line = 0;
  bool first_segment_in_line = true;
  for (int i = 0, mappings_size = mappings.size(); i < mappings_size; ++i) {
    if (mappings[i].gen_line < current_gen_line) {
      LOG(DFATAL) << "Mappings are not sorted.";
      return false;
    }

    // gen_line is not encoded into the fields, instead each line in the
    // generated file is ; delineated in the VLQ.
    while (mappings[i].gen_line > current_gen_line) {
      *result += ";";
      first_segment_in_line = true;
      ++current_gen_line;
    }

    // Fields to encode in base64 VLQ.
    // 1) Generated column number
    if (first_segment_in_line) {
      // First segment for each line must list absolute column number.
      *result += EncodeVlq(mappings[i].gen_col);
    } else {
      // Subsequent ones will list column number as a diff from previous one
      // as a space saving measure.
      *result += EncodeVlq(mappings[i].gen_col - mappings[i - 1].gen_col);
    }

    // 2) Source file number
    if (i == 0) {
      // First segment of the file must list absolute numbers.
      *result += EncodeVlq(mappings[i].src_file);
    } else {
      // Subsequent ones list diffs.
      *result += EncodeVlq(mappings[i].src_file - mappings[i - 1].src_file);
    }

    // 3) Source line number
    if (i == 0) {
      *result += EncodeVlq(mappings[i].src_line);
    } else {
      *result += EncodeVlq(mappings[i].src_line - mappings[i - 1].src_line);
    }

    // 4) Source column number
    if (i == 0) {
      *result += EncodeVlq(mappings[i].src_col);
    } else {
      *result += EncodeVlq(mappings[i].src_col - mappings[i - 1].src_col);
    }

    // Note: We do not add (5) Names.

    first_segment_in_line = false;

    // Segments are comma-separated, but don't add a trailing comma nor a comma
    // if a semicolon (line change) will be added.
    if (i + 1 < mappings_size &&
        mappings[i + 1].gen_line == current_gen_line) {
      *result += ",";
    }
  }

  return true;
}

GoogleString PercentEncode(StringPiece url) {
  GoogleString result;
  for (int i = 0, n = url.size(); i < n; ++i) {
    switch (url[i]) {
      case '<':
        result += "%3C";
        break;
      case '>':
        result += "%3E";
        break;
      default:
        result.push_back(url[i]);
        break;
    }
  }
  return result;
}

bool Encode(StringPiece generated_url,
            StringPiece source_url,
            const std::vector<Mapping>& mappings,
            GoogleString* encoded_source_map) {
  GoogleString encoded_mappings;
  bool success = EncodeMappings(mappings, &encoded_mappings);
  if (success) {
    Json::Value json;
    json["version"] = 3;
    if (!generated_url.empty()) {
      json["file"] = PercentEncode(generated_url).c_str();
    }
    // Sources array with one value.
    json["sources"][0] = PercentEncode(source_url).c_str();
    // Note: We do not provide names functionality.
    json["names"] = Json::arrayValue;  // Empty array.
    json["mappings"] = encoded_mappings.c_str();

    // Standard XSSI protection.
    // http://www.html5rocks.com/en/tutorials/developertools/sourcemaps/#toc-xssi
    *encoded_source_map += ")]}'\n";

    Json::FastWriter writer;
    *encoded_source_map += writer.write(json);
  }
  return success;
}

}  // namespace source_map

}  // namespace net_instaweb
