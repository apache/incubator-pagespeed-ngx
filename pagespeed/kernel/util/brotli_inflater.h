/*
 * Copyright 2016 Google Inc.
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

// Author: jcrowell@google.com (Jeffrey Crowell)

#ifndef PAGESPEED_KERNEL_UTIL_BROTLI_INFLATER_H_
#define PAGESPEED_KERNEL_UTIL_BROTLI_INFLATER_H_

#include <memory>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"

struct BrotliDecoderStateStruct;

namespace net_instaweb {

class Writer;
class MessageHandler;

// TODO(jcrowell): Add compression interface that can handle multiple
// compressors (gzip/deflate, brotli, etc.).
class BrotliInflater {
 public:
  BrotliInflater();
  ~BrotliInflater();

  // Compresses a StringPiece, writing output to Writer.  Returns false
  // if there was some kind of failure, though none are expected.
  // If no compression level is specified, the default of 11 (maximum
  // compression/highest quality) is used.
  // TODO(jcrowell): add api that takes in &out_string as an argument, to remove
  // string copy.
  static bool Compress(StringPiece in, MessageHandler* handler, Writer* writer);
  static bool Compress(StringPiece in, int compression_level,
                       MessageHandler* handler, Writer* writer);

  // Decompresses a StringPiece, writing output to Writer.  Returns false
  // if there was some kind of failure, such as a corrupt input.
  static bool Decompress(StringPiece in, MessageHandler* handler,
                         Writer* writer);
  bool DecompressHelper(StringPiece in, MessageHandler* handler,
                        Writer* writer);

  // TODO(jcrowell): Add API with properly sized output buffer (taken from
  // X-Original-Content-Length).

 private:
  // Reset the BrotliState.
  void ResetState();

  // Keep track of if the internal state is "dirty", if so, refreshed by
  // ResetState() before Decompression.
  bool state_used_;
  std::unique_ptr<BrotliDecoderStateStruct, void(*)(BrotliDecoderStateStruct*)>
      brotli_state_;

  DISALLOW_COPY_AND_ASSIGN(BrotliInflater);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_BROTLI_INFLATER_H_
