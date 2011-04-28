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

#include "net/instaweb/util/public/gzip_inflater.h"

#include <cstddef>
#include <cstdlib>
#include "base/logging.h"
#ifdef USE_SYSTEM_ZLIB
#include "zlib.h"
#else
#include "third_party/zlib/zlib.h"
#endif
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

GzipInflater::GzipInflater(InflateType type)
    : zlib_(NULL),
      finished_(false),
      error_(false),
      type_(type) {
}

GzipInflater::~GzipInflater() {
  Free();
}

void GzipInflater::Free() {
  if (zlib_ == NULL) {
    // Already freed.
    return;
  }

  int err = inflateEnd(zlib_);
  if (err != Z_OK) {
    error_ = true;
  }

  free(zlib_);

  zlib_ = NULL;
}

bool GzipInflater::Init() {
  if (zlib_ != NULL) {
    return false;
  }

  zlib_ = static_cast<z_stream *>(malloc(sizeof(z_stream)));
  if (zlib_ == NULL) {
    return false;
  }
  memset(zlib_, 0, sizeof(z_stream));

  // Tell zlib that the data is gzip-encoded or deflate-encode.
  int window_size = 31;  // window size of 15, plus 16 for gzip
  if (type_ == kDeflate) {
    window_size = -15;  // window size of -15 for row deflate.
  }
  int err = inflateInit2(zlib_, window_size);

  if (err != Z_OK) {
    Free();
    error_ = true;
    return false;
  }

  return true;
}

bool GzipInflater::HasUnconsumedInput() const {
  if (zlib_ == NULL) {
    return false;
  }

  if (finished_ || error_) {
    return false;
  }

  return zlib_->avail_in > 0;
}

bool GzipInflater::SetInput(const void *in, size_t in_size) {
  if (zlib_ == NULL) {
    return false;
  }

  if (HasUnconsumedInput()) {
    return false;
  }

  if (finished_) {
    return false;
  }

  if (error_) {
    return false;
  }

  if (in == NULL || in_size == 0) {
    return false;
  }

  // The zlib library won't modify the buffer, but it does not use
  // const here, so we must const cast.
  zlib_->next_in  = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(in));
  zlib_->avail_in = static_cast<uInt>(in_size);

  return true;
}

int GzipInflater::InflateBytes(char *buf, size_t buf_size) {
  if (zlib_ == NULL) {
    return -1;
  }

  if (!HasUnconsumedInput()) {
    return -1;
  }

  if (finished_) {
    return -1;
  }

  if (error_) {
    return -1;
  }

  if (buf == NULL || buf_size == 0) {
    return -1;
  }

  const size_t inflated_bytes_before = zlib_->total_out;

  zlib_->next_out = reinterpret_cast<Bytef *>(buf);
  zlib_->avail_out = static_cast<uInt>(buf_size);

  int err = inflate(zlib_, Z_SYNC_FLUSH);

  const size_t inflated_bytes = zlib_->total_out - inflated_bytes_before;

  if (err == Z_STREAM_END) {
    finished_ = true;
  } else if (err == Z_OK) {
    if (inflated_bytes < buf_size) {
      // Sanity check that if we didn't fill the output buffer, it's
      // because we consumed all of the input.
      DCHECK(!HasUnconsumedInput());
    }
  } else if (err == Z_BUF_ERROR) {
    // Sanity check that if we encountered this error, it's because we
    // were unable to write any inflated bytes to the output
    // buffer. zlib documentation says that this is a non-terminal
    // error, so we do not set error_ to true here.
    DCHECK_EQ(inflated_bytes, static_cast<size_t>(0));
  } else {
    error_ = true;
    return -1;
  }

  return static_cast<int>(inflated_bytes);
}

void GzipInflater::ShutDown() {
  Free();
}

}  // namespace net_instaweb
