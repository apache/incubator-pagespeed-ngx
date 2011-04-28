/*
 * Copyright 2011 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/util/public/chunking_writer.h"

#include <algorithm>
#include <cstddef>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class MessageHandler;

ChunkingWriter::ChunkingWriter(Writer* writer, int flush_limit)
    : writer_(writer), flush_limit_(flush_limit), unflushed_bytes_(0) {
}

ChunkingWriter::~ChunkingWriter() {
}

bool ChunkingWriter::Write(const StringPiece& str_orig,
                           MessageHandler* handler) {
  StringPiece str = str_orig;

  // ensure we have some output allowance
  if (!FlushIfNeeded(handler)) {
    return false;
  }

  while (!str.empty()) {
    size_t to_write = str.size();
    if (flush_limit_ > 0) {
      // Figure out how many bytes we can write without going over
      // the flush window limit. flush_limit_ - unflushed_bytes_ is positive
      // since this is always preceeded by a FlushIfNeeded()
      to_write = std::min(static_cast<int>(str.size()),
                          flush_limit_ - unflushed_bytes_);
    }

    if (!writer_->Write(StringPiece(str.data(), to_write), handler)) {
      return false;
    }

    str.remove_prefix(to_write);
    unflushed_bytes_ += to_write;
    if (!FlushIfNeeded(handler)) {
      return false;
    }
  }
  return true;
}

bool ChunkingWriter::Flush(MessageHandler* handler) {
  unflushed_bytes_ = 0;
  return writer_->Flush(handler);
}

bool ChunkingWriter::FlushIfNeeded(MessageHandler* handler) {
  if (flush_limit_ <= 0 || unflushed_bytes_ < flush_limit_) {
    return true;
  }

  return Flush(handler);
}

}  // namespace net_instaweb
