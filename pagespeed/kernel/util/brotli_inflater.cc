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

#include "pagespeed/kernel/util/brotli_inflater.h"

#include <cstddef>
#include <limits>

#include "base/logging.h"
#include "third_party/brotli/src/dec/decode.h"
#include "third_party/brotli/src/enc/encode.h"
#include "third_party/brotli/src/enc/streams.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/stack_buffer.h"
#include "pagespeed/kernel/base/writer.h"

using brotli::BrotliMemIn;
using brotli::BrotliParams;
using brotli::BrotliStringOut;

namespace net_instaweb {

BrotliInflater::BrotliInflater()
    : state_used_(false),
      brotli_state_(BrotliCreateState(nullptr, nullptr, nullptr),
                    &BrotliDestroyState) { }

BrotliInflater::~BrotliInflater() { }

void BrotliInflater::ResetState() {
  if (state_used_) {
    brotli_state_.reset(BrotliCreateState(nullptr, nullptr, nullptr));
  }
  state_used_ = true;
}

bool BrotliInflater::Compress(StringPiece in, int compression_level,
                              MessageHandler* handler, Writer* writer) {
  // For creating a BrotliStringOut.
  GoogleString out_str;
  // Set the compression level ("quality" in brotli terms).
  BrotliParams params;
  params.quality = compression_level;
  BrotliMemIn brotli_input(in.data(), in.length());
  BrotliStringOut brotli_output(&out_str, std::numeric_limits<int>::max());
  // Compress in one shot with BrotliCompress.
  if (BrotliCompress(params, &brotli_input, &brotli_output)) {
    return writer->Write(out_str, handler);
  }
  return false;
}

bool BrotliInflater::Compress(StringPiece in, MessageHandler* handler,
                              Writer* writer) {
  // Default quality is 11 for brotli.
  return BrotliInflater::Compress(in, 11, handler, writer);
}

bool BrotliInflater::DecompressHelper(StringPiece in, MessageHandler* handler,
                                      Writer* writer) {
  // Mostly taken from BrotliDecompress in the tool "bro".
  // https://raw.githubusercontent.com/google/brotli/v0.2.0/tools/bro.cc
  char output[kStackBufferSize];
  size_t total_out = 0;
  size_t available_in = in.length();
  const char* next_in = in.data();
  BrotliResult result = BROTLI_RESULT_NEEDS_MORE_INPUT;
  ResetState();
  if (!brotli_state_.get()) {
    return false;  // Memory allocation failed.
  }
  while (result != BROTLI_RESULT_SUCCESS) {
    size_t available_out = sizeof(output);
    char* next_out = output;
    result = BrotliDecompressStream(
        &available_in, reinterpret_cast<const unsigned char**>(&next_in),
        &available_out, reinterpret_cast<unsigned char**>(&next_out),
        &total_out, brotli_state_.get());
    CHECK(next_in >= in.data());
    CHECK_LE(available_out, sizeof(output));
    in.remove_prefix(next_in - in.data());
    switch (result) {
      case BROTLI_RESULT_NEEDS_MORE_INPUT:
        // We should never hit this case because the compressed input isn't
        // streamed.
        handler->Message(kWarning, "BROTLI_RESULT_NEEDS_MORE_INPUT");
        return false;
      case BROTLI_RESULT_ERROR:
        // Decompressing failed.
        handler->Message(kError, "BROTLI_RESULT_ERROR");
        return false;
      case BROTLI_RESULT_NEEDS_MORE_OUTPUT:
        // Need to flush the output buffer to the writer.
        break;
      case BROTLI_RESULT_SUCCESS:
        // Decompression succeeded, write out the last chunk if needed.
        break;
    }
    StringPiece chunk(output, sizeof(output) - available_out);
    if (!chunk.empty() && !writer->Write(chunk, handler)) {
      return false;
    }
  }
  return true;  // BROTLI_RESULT_SUCCESS
}

bool BrotliInflater::Decompress(StringPiece in, MessageHandler* handler,
                                Writer* writer) {
  BrotliInflater brotli;
  return brotli.DecompressHelper(in, handler, writer);
}

}  // namespace net_instaweb
