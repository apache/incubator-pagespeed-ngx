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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_CHUNKING_WRITER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_CHUNKING_WRITER_H_

#include "base/basictypes.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

// Wraps around an another writer forcing periodic flushes, and making sure
// writes are not too long.
class ChunkingWriter : public Writer {
 public:
  // This writer will force a flush every flush_limit bytes.
  // If the flush_limit is <= 0 no extra flushing will be performed.
  // This does NOT take ownership of passed-in writer.
  ChunkingWriter(Writer* writer, int flush_limit);
  virtual ~ChunkingWriter();

  virtual bool Write(const StringPiece& str, MessageHandler* handler);
  virtual bool Flush(MessageHandler* handler);

 private:
  // Flushes output if we have enough queued; returns false on Flush failure
  bool FlushIfNeeded(MessageHandler* handler);

  Writer* const writer_;
  const int flush_limit_;
  int unflushed_bytes_;

  DISALLOW_COPY_AND_ASSIGN(ChunkingWriter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CHUNKING_WRITER_H_
