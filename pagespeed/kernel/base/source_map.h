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
//
// Encoder for Source Map Revision 3. Specification and other info:
// * https://docs.google.com/document/d/1U1RGAehQwRypUTovF1KRlpiOFze0b-_2gc6fAH0KY0k/edit
// * http://www.html5rocks.com/en/tutorials/developertools/sourcemaps/
// * http://en.wikipedia.org/wiki/Variable-length_quantity

#ifndef PAGESPEED_KERNEL_UTIL_SOURCE_MAP_H_
#define PAGESPEED_KERNEL_UTIL_SOURCE_MAP_H_

#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

namespace source_map {

// Declares a mapping between a line # and column # in generated file
// with the corresponding source file #, line # and column #.
struct Mapping {
  int gen_line;
  int gen_col;

  int src_file;
  int src_line;
  int src_col;

  Mapping() : gen_line(0), gen_col(0), src_file(0), src_line(0), src_col(0) {}
  Mapping(int gen_line_arg, int gen_col_arg,
          int src_file_arg, int src_line_arg, int src_col_arg)
      : gen_line(gen_line_arg),
        gen_col(gen_col_arg),
        src_file(src_file_arg),
        src_line(src_line_arg),
        src_col(src_col_arg) {
  }
};

typedef std::vector<Mapping> MappingVector;

// Encodes generated_url, source_url and mappings into encoded_source_map
// which will be the contents of a JSON Source Map v3 file.
// Bool returned answers question "Did this succeed?"
bool Encode(StringPiece generated_url,  // optional: "" to ignore.
            StringPiece source_url,
            // mappings MUST already be sorted by gen_line and then gen_col.
            const MappingVector& mappings,
            GoogleString* encoded_source_map);

// TODO(sligocki)-maybe: Do we want a decoder as well? Might be nice for
// testing purposes, then we could throw a lot of random examples at it and
// make sure they Encode -> Decode back to the original.

// Internal methods. These should not be called directly. Included here for
// test visibility.

// Simply convert val 0-63 into a base64 char.
char EncodeBase64(int val);
// Encodes an arbitrary 32-bit integer into VLQ (Variable Length Quantity)
// base64 (A sequence of base64 chars which use continuation bits to encode
// arbitrary length values.)
GoogleString EncodeVlq(int32 val);
// Encodes mappings into a sequence of ; and , separated VLQ base64 values.
bool EncodeMappings(const MappingVector& mappings,
                    GoogleString* result);
// Percent encode < and > in URLs to avoid XSS shenanigans.
GoogleString PercentEncode(StringPiece url);

}  // namespace source_map

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_SOURCE_MAP_H_
