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
// HashOutputResource constructs filenames/urls based on a hash of contents.
// NOTE: resource.url() is not valid until after resource.Write() is called.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_HASH_OUTPUT_RESOURCE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_HASH_OUTPUT_RESOURCE_H_

#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/simple_meta_data.h"


namespace net_instaweb {

class FilenameEncoder;
class Hasher;
class MessageHandler;

class HashOutputResource : public OutputResource {
 public:
  HashOutputResource(const StringPiece& url_prefix,
                     const StringPiece& filename_prefix,
                     const StringPiece& filter_prefix,
                     const StringPiece& name,
                     const StringPiece& suffix,
                     FileSystem* file_system,
                     FilenameEncoder* filename_encoder,
                     Hasher* hasher);

  virtual Writer* BeginWrite(MessageHandler* message_handler);
  virtual bool EndWrite(Writer* writer, MessageHandler* message_handler);
  virtual StringPiece url() const;
  virtual bool Read(Writer* writer, MetaData* response_headers,
                    MessageHandler* handler) const;

  virtual bool IsReadable() const;
  virtual bool IsWritten() const;

  virtual const MetaData* metadata() const { return &metadata_; }
  virtual MetaData* metadata() { return &metadata_; }

 private:
  virtual std::string TempPrefix() const;

  std::string url_;
  std::string filename_;
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
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_HASH_OUTPUT_RESOURCE_H_
