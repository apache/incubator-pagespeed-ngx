/**
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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

OutputResource::~OutputResource() {
}

// Deprecated interface for writing the output file in chunks.  To
// be removed soon.
bool OutputResource::StartWrite(MessageHandler* message_handler) {
  assert(writer_ == NULL);
  writer_ = BeginWrite(message_handler);
  return writer_ != NULL;
}

bool OutputResource::WriteChunk(const StringPiece& buf,
                                MessageHandler* handler) {
  return writer_->Write(buf, handler);
}

bool OutputResource::EndWrite(MessageHandler* message_handler) {
  return EndWrite(writer_, message_handler);
}

}  // namespace net_instaweb
