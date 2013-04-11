// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SdchFilter applies open_vcdiff content decoding to a datastream.
// This decoding uses a pre-cached dictionary of text fragments to decode
// (expand) the stream back to its original contents.
//
// This SdchFilter internally uses open_vcdiff/vcdec library to do decoding.
//
// SdchFilter is also a subclass of Filter. See the latter's header file
// filter.h for sample usage.

#ifndef NET_BASE_SDCH_FILTER_H_
#define NET_BASE_SDCH_FILTER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "net/base/filter.h"
#include "net/base/net_export.h"
#include "net/base/sdch_manager.h"

namespace open_vcdiff {
class VCDiffStreamingDecoder;
}

namespace net {

class NET_EXPORT_PRIVATE SdchFilter : public Filter {
 public:
  virtual ~SdchFilter();

  // Initializes filter decoding mode and internal control blocks.
  bool InitDecoding(Filter::FilterType filter_type);

  // Decode the pre-filter data and writes the output into |dest_buffer|
  // The function returns FilterStatus. See filter.h for its description.
  //
  // Upon entry, *dest_len is the total size (in number of chars) of the
  // destination buffer. Upon exit, *dest_len is the actual number of chars
  // written into the destination buffer.
  virtual FilterStatus ReadFilteredData(char* dest_buffer,
                                        int* dest_len) OVERRIDE;

 private:
  // Internal status.  Once we enter an error state, we stop processing data.
  enum DecodingStatus {
    DECODING_UNINITIALIZED,
    WAITING_FOR_DICTIONARY_SELECTION,
    DECODING_IN_PROGRESS,
    DECODING_ERROR,
    META_REFRESH_RECOVERY,  // Decoding error being handled by a meta-refresh.
    PASS_THROUGH,  // Non-sdch content being passed without alteration.
  };

  // Only to be instantiated by Filter::Factory.
  explicit SdchFilter(const FilterContext& filter_context);
  friend class Filter;

  // Identify the suggested dictionary, and initialize underlying decompressor.
  Filter::FilterStatus InitializeDictionary();

  // Move data that was internally buffered (after decompression) to the
  // specified dest_buffer.
  int OutputBufferExcess(char* const dest_buffer, size_t available_space);

  // Context data from the owner of this filter.
  const FilterContext& filter_context_;

  // Tracks the status of decoding.
  // This variable is initialized by InitDecoding and updated only by
  // ReadFilteredData.
  DecodingStatus decoding_status_;

  // The underlying decoder that processes data.
  // This data structure is initialized by InitDecoding and updated in
  // ReadFilteredData.
  scoped_ptr<open_vcdiff::VCDiffStreamingDecoder> vcdiff_streaming_decoder_;

  // In case we need to assemble the hash piecemeal, we have a place to store
  // a part of the hash until we "get all 8 bytes plus a null."
  std::string dictionary_hash_;

  // After assembling an entire dictionary hash (the first 9 bytes of the
  // sdch payload, we check to see if it is plausible, meaning it has a null
  // termination, and has 8 characters that are possible in a net-safe base64
  // encoding.  If the hash is not plausible, then the payload is probably not
  // an SDCH encoded bundle, and various error recovery strategies can be
  // attempted.
  bool dictionary_hash_is_plausible_;

  // We hold an in-memory copy of the dictionary during the entire decoding, as
  // it is used directly by the VC-DIFF decoding system.
  // That char* data is part of the dictionary_ we hold a reference to.
  scoped_refptr<SdchManager::Dictionary> dictionary_;

  // The decoder may demand a larger output buffer than the target of
  // ReadFilteredData so we buffer the excess output between calls.
  std::string dest_buffer_excess_;
  // To avoid moving strings around too much, we save the index into
  // dest_buffer_excess_ that has the next byte to output.
  size_t dest_buffer_excess_index_;

  // To get stats on activities, we keep track of source and target bytes.
  // Visit about:histograms/Sdch to see histogram data.
  size_t source_bytes_;
  size_t output_bytes_;

  // Error recovery in content type may add an sdch filter type, in which case
  // we should gracefully perform pass through if the format is incorrect, or
  // an applicable dictionary can't be found.
  bool possible_pass_through_;

  // The URL that is currently being filtered.
  // This is used to restrict use of a dictionary to a specific URL or path.
  GURL url_;

  // To facilitate error recovery, allow filter to know if content is text/html
  // by checking within this mime type (we may do a meta-refresh via html).
  std::string mime_type_;

  DISALLOW_COPY_AND_ASSIGN(SdchFilter);
};

}  // namespace net

#endif  // NET_BASE_SDCH_FILTER_H_
