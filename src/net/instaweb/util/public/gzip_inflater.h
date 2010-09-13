// Copyright 2010 Google Inc.
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_GZIP_INFLATER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_GZIP_INFLATER_H_

#include <string.h>
#include "base/basictypes.h"

typedef struct z_stream_s z_stream;

namespace net_instaweb {

class GzipInflater {
 public:
  GzipInflater();
  ~GzipInflater();

  // Should be called once, before inflating any data.
  bool Init();

  // Should be called once, after inflating is finished.
  void ShutDown();

  // Does the inflater still have input that has not yet been
  // consumed? If true, the caller should call InflateBytes(). If
  // false, the gzip inflater is ready for additional input.
  bool HasUnconsumedInput() const;

  // Pass a gzip-compressed buffer to the gzip inflater. The gzip
  // inflater will inflate the buffer via InflateBytes(). SetInput
  // should not be called if HasUnconsumedInput() is true, and the
  // buffer passed into SetInput should not be modified by the caller
  // until HasUnconsumedInput() returns false.
  bool SetInput(const void *in, size_t in_size);

  // Decompress the input passed in via SetInput. Should be called
  // until HasUnconsumedInput returns false. Returns the number of
  // bytes inflated, or -1 if an error was encountered while
  // inflating.
  int InflateBytes(char *buf, size_t buf_size);

  // Has the entire input been inflated?
  bool finished() const { return finished_; }

  // Was an error encountered during inflating?
  bool error() const { return error_; }

 private:
  void Free();

  z_stream *zlib_;
  bool finished_;
  bool error_;

  DISALLOW_COPY_AND_ASSIGN(GzipInflater);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_GZIP_INFLATER_H_
