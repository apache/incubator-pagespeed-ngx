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

#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class FilenameEncoder;
class Hasher;
class MessageHandler;
class MetaData;
class Writer;

class OutputResource {
 public:
  OutputResource(const StringPiece& url_prefix,
                 const StringPiece& filename_prefix,
                 const StringPiece& filter_prefix,
                 const StringPiece& name,
                 const StringPiece& suffix,
                 FileSystem* file_system,
                 FilenameEncoder* filename_encoder,
                 Hasher* hasher);
  ~OutputResource();

  // Deprecated interface for writing the output file in chunks.  To
  // be removed soon.
  bool StartWrite(MessageHandler* message_handler);
  bool WriteChunk(const StringPiece& buf, MessageHandler* handler);
  bool EndWrite(MessageHandler* message_handler);

  // Writer-based interface for writing the output file.
  Writer* BeginWrite(MessageHandler* message_handler);
  bool EndWrite(Writer* writer, MessageHandler* message_handler);

  std::string url() const;
  std::string filename() const;

  StringPiece name() const { return name_; }
  StringPiece suffix() const { return suffix_; }
  const MetaData* metadata() const { return &metadata_; }
  MetaData* metadata() { return &metadata_; }

  // In a scalable installation where the sprites must be kept in a
  // database, we cannot serve HTML that references new resources
  // that have not been committed yet, and committing to a database
  // may take too long to block on the HTML rewrite.  So we will want
  // to refactor this to check to see whether the desired resource is
  // already known.  For now we'll assume we can commit to serving the
  // resource during the HTML rewriter.
  bool IsReadable() const;
  bool IsWritten() const;

  // Read the output resource back in and send it to a writer.  This
  // version always reads the file from disk, and will not use the
  // cache.  Consider using ResourceManager::FetchOutputResource.
  bool ReadNoCache(Writer* writer, MetaData* response_headers,
                           MessageHandler* handler) const;

 private:
  friend class ResourceManager;
  void SetHash(const StringPiece& hash);

  std::string TempPrefix() const;
  std::string NameTail() const;

  bool write_http_headers_;
  FileSystem* file_system_;
  FileSystem::OutputFile* output_file_;
  SimpleMetaData metadata_;
  bool writing_complete_;
  std::string url_prefix_;
  std::string filename_prefix_;
  std::string filter_prefix_;
  std::string name_;
  std::string suffix_;
  std::string hash_;
  FilenameEncoder* filename_encoder_;
  Hasher* hasher_;
  Writer* writer_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_OUTPUT_RESOURCE_H_
