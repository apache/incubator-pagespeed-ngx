// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GZipFilter applies gzip and deflate content encoding/decoding to a data
// stream. As specified by HTTP 1.1, with gzip encoding the content is
// wrapped with a gzip header, and with deflate encoding the content is in
// a raw, headerless DEFLATE stream.
//
// Internally GZipFilter uses zlib inflate to do decoding.
//
// GZipFilter is a subclass of Filter. See the latter's header file filter.h
// for sample usage.

#ifndef NET_BASE_GZIP_FILTER_H_
#define NET_BASE_GZIP_FILTER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/filter.h"

typedef struct z_stream_s z_stream;

namespace net {

class GZipHeader;

class GZipFilter : public Filter {
 public:
  virtual ~GZipFilter();

  // Initializes filter decoding mode and internal control blocks.
  // Parameter filter_type specifies the type of filter, which corresponds to
  // either gzip or deflate decoding. The function returns true if success and
  // false otherwise.
  // The filter can only be initialized once.
  bool InitDecoding(Filter::FilterType filter_type);

  // Decodes the pre-filter data and writes the output into the dest_buffer
  // passed in.
  // The function returns FilterStatus. See filter.h for its description.
  //
  // Upon entry, *dest_len is the total size (in number of chars) of the
  // destination buffer. Upon exit, *dest_len is the actual number of chars
  // written into the destination buffer.
  //
  // This function will fail if there is no pre-filter data in the
  // stream_buffer_. On the other hand, *dest_len can be 0 upon successful
  // return. For example, the internal zlib may process some pre-filter data
  // but not produce output yet.
  virtual FilterStatus ReadFilteredData(char* dest_buffer, int* dest_len);

 private:
  enum DecodingStatus {
    DECODING_UNINITIALIZED,
    DECODING_IN_PROGRESS,
    DECODING_DONE,
    DECODING_ERROR
  };

  enum DecodingMode {
    DECODE_MODE_GZIP,
    DECODE_MODE_DEFLATE,
    DECODE_MODE_UNKNOWN
  };

  enum GZipCheckHeaderState {
    GZIP_CHECK_HEADER_IN_PROGRESS,
    GZIP_GET_COMPLETE_HEADER,
    GZIP_GET_INVALID_HEADER
  };

  static const int kGZipFooterSize = 8;

  // Only to be instantiated by Filter::Factory.
  GZipFilter();
  friend class Filter;

  // Parses and verifies the GZip header.
  // Upon exit, the function updates gzip_header_status_ accordingly.
  //
  // The function returns Filter::FILTER_OK if it gets a complete header and
  // there are more data in the pre-filter buffer.
  // The function returns Filter::FILTER_NEED_MORE_DATA if it parses all data
  // in the pre-filter buffer, either getting a complete header or a partial
  // header. The caller needs to check gzip_header_status_ and call this
  // function again for partial header.
  // The function returns Filter::FILTER_ERROR if error occurs.
  FilterStatus CheckGZipHeader();

  // Internal function to decode the pre-filter data and writes the output into
  // the dest_buffer passed in.
  //
  // This is the internal version of ReadFilteredData. See the latter's
  // comments for the use of function.
  FilterStatus DoInflate(char* dest_buffer, int* dest_len);

  // Inserts a zlib header to the data stream before calling zlib inflate.
  // This is used to work around server bugs. See more comments at the place
  // it is called in gzip_filter.cc.
  // The function returns true on success and false otherwise.
  bool InsertZlibHeader();

  // Skip the 8 byte GZip footer after z_stream_end
  void SkipGZipFooter();

  // Tracks the status of decoding.
  // This variable is initialized by InitDecoding and updated only by
  // ReadFilteredData.
  DecodingStatus decoding_status_;

  // Indicates the type of content decoding the GZipFilter is performing.
  // This variable is set only once by InitDecoding.
  DecodingMode decoding_mode_;

  // Used to parse the gzip header in gzip stream.
  // It is used when the decoding_mode_ is DECODE_MODE_GZIP.
  scoped_ptr<GZipHeader> gzip_header_;

  // Tracks the progress of parsing gzip header.
  // This variable is maintained by gzip_header_.
  GZipCheckHeaderState gzip_header_status_;

  // A flag used by InsertZlibHeader to record whether we've successfully added
  // a zlib header to this stream.
  bool zlib_header_added_;

  // Tracks how many bytes of gzip footer have been received.
  int gzip_footer_bytes_;

  // The control block of zlib which actually does the decoding.
  // This data structure is initialized by InitDecoding and updated only by
  // DoInflate, with InsertZlibHeader being the exception as a workaround.
  scoped_ptr<z_stream> zlib_stream_;

  // For robustness, when we see the solo sdch filter, we chain in a gzip filter
  // in front of it, with this flag to indicate that the gzip decoding might not
  // be needed.  This handles a strange case where "Content-Encoding: sdch,gzip"
  // is reduced by an errant proxy to "Content-Encoding: sdch", while the
  // content is indeed really gzipped result of sdch :-/.
  // If this flag is set, then we will revert to being a pass through filter if
  // we don't get a valid gzip header.
  bool possible_sdch_pass_through_;

  DISALLOW_COPY_AND_ASSIGN(GZipFilter);
};

}  // namespace net

#endif  // NET_BASE_GZIP_FILTER_H__
