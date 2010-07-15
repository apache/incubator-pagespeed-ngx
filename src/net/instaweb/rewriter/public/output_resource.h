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
//
// Output resources are created by a ResourceManager. They must be able to
// write contents and return their url (so that it can be href'd on a page).

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_OUTPUT_RESOURCE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_OUTPUT_RESOURCE_H_

#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class MetaData;
class Writer;

class OutputResource {
 public:
  OutputResource() : writer_(NULL) { }
  virtual ~OutputResource();

  // Deprecated interface for writing the output file in chunks.  To
  // be removed soon.
  bool StartWrite(MessageHandler* message_handler);
  bool WriteChunk(const StringPiece& buf, MessageHandler* handler);
  bool EndWrite(MessageHandler* message_handler);

  // Writer-based interface for writing the output file.
  virtual Writer* BeginWrite(MessageHandler* message_handler) = 0;
  virtual bool EndWrite(Writer* writer, MessageHandler* message_handler) = 0;

  virtual std::string url() const = 0;
  virtual std::string filename() const = 0;

  virtual StringPiece name() const = 0;
  virtual StringPiece suffix() const = 0;
  virtual const MetaData* metadata() const = 0;
  virtual MetaData* metadata() = 0;

  // In a scalable installation where the sprites must be kept in a
  // database, we cannot serve HTML that references new resources
  // that have not been committed yet, and committing to a database
  // may take too long to block on the HTML rewrite.  So we will want
  // to refactor this to check to see whether the desired resource is
  // already known.  For now we'll assume we can commit to serving the
  // resource during the HTML rewriter.
  virtual bool IsReadable() const = 0;
  virtual bool IsWritten() const = 0;

  // Read the output resource back in and send it to a writer.
  virtual bool Read(const StringPiece& filename, Writer* writer,
                    MetaData* response_headers, MessageHandler* handler)
      const = 0;

 private:
  Writer* writer_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_OUTPUT_RESOURCE_H_
