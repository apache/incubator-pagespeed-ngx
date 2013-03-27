// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UTF_OFFSET_STRING_CONVERSIONS_H_
#define BASE_UTF_OFFSET_STRING_CONVERSIONS_H_

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/string16.h"
#include "base/string_piece.h"

// Like the conversions in utf_string_conversions.h, but also takes one or more
// offsets (|offset[s]_for_adjustment|) into the source strings, each offset
// will be adjusted to point at the same logical place in the result strings.
// If this isn't possible because an offset points past the end of the source
// strings or into the middle of a multibyte sequence, the offending offset will
// be set to string16::npos. |offset[s]_for_adjustment| may be NULL.
BASE_EXPORT bool UTF8ToUTF16AndAdjustOffset(const char* src,
                                            size_t src_len,
                                            string16* output,
                                            size_t* offset_for_adjustment);
BASE_EXPORT bool UTF8ToUTF16AndAdjustOffsets(
    const char* src,
    size_t src_len,
    string16* output,
    std::vector<size_t>* offsets_for_adjustment);

BASE_EXPORT string16 UTF8ToUTF16AndAdjustOffset(const base::StringPiece& utf8,
                                                size_t* offset_for_adjustment);
BASE_EXPORT string16 UTF8ToUTF16AndAdjustOffsets(
    const base::StringPiece& utf8,
    std::vector<size_t>* offsets_for_adjustment);

BASE_EXPORT std::string UTF16ToUTF8AndAdjustOffset(
    const base::StringPiece16& utf16,
    size_t* offset_for_adjustment);
BASE_EXPORT std::string UTF16ToUTF8AndAdjustOffsets(
    const base::StringPiece16& utf16,
    std::vector<size_t>* offsets_for_adjustment);

// Limiting function callable by std::for_each which will replace any value
// which is equal to or greater than |limit| with npos.
template <typename T>
struct LimitOffset {
  explicit LimitOffset(size_t limit)
    : limit_(limit) {}

  void operator()(size_t& offset) {
    if (offset >= limit_)
      offset = T::npos;
  }

  size_t limit_;
};

// Stack object which, on destruction, will update a vector of offsets based on
// any supplied adjustments.  To use, declare one of these, providing the
// address of the offset vector to adjust.  Then Add() any number of Adjustments
// (each Adjustment gives the |original_offset| of a substring and the lengths
// of the substring before and after transforming).  When the OffsetAdjuster
// goes out of scope, all the offsets in the provided vector will be updated.
class BASE_EXPORT OffsetAdjuster {
 public:
  struct BASE_EXPORT Adjustment {
    Adjustment(size_t original_offset,
               size_t original_length,
               size_t output_length);

    size_t original_offset;
    size_t original_length;
    size_t output_length;
  };

  explicit OffsetAdjuster(std::vector<size_t>* offsets_for_adjustment);
  ~OffsetAdjuster();

  void Add(const Adjustment& adjustment);

 private:
  void AdjustOffset(std::vector<size_t>::iterator offset);

  std::vector<size_t>* offsets_for_adjustment_;
  std::vector<Adjustment> adjustments_;
};

#endif  // BASE_UTF_OFFSET_STRING_CONVERSIONS_H_
