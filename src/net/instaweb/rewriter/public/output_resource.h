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
#include "net/instaweb/util/public/file_writer.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/rewriter/public/resource.h"

namespace net_instaweb {

class FilenameEncoder;
class Hasher;
class MessageHandler;

class OutputResource : public Resource {
 public:
  OutputResource(ResourceManager* manager,
                 const ContentType* type,
                 const StringPiece& filter_prefix,
                 const StringPiece& name);
  ~OutputResource();

  virtual bool Read(MessageHandler* message_handler);
  virtual std::string url() const;

  // output-specific
  StringPiece name() const { return name_; }
  std::string filename() const;
  StringPiece suffix() const;

  // In a scalable installation where the sprites must be kept in a
  // database, we cannot serve HTML that references new resources
  // that have not been committed yet, and committing to a database
  // may take too long to block on the HTML rewrite.  So we will want
  // to refactor this to check to see whether the desired resource is
  // already known.  For now we'll assume we can commit to serving the
  // resource during the HTML rewriter.
  bool IsWritten() const;

 private:
  friend class ResourceManager;
  class OutputWriter : public FileWriter {
   public:
    OutputWriter(FileSystem::OutputFile* file, Hasher* hasher)
        : FileWriter(file),
          hasher_(hasher) {
    }

    virtual bool Write(const StringPiece& data, MessageHandler* handler);
    Hasher* hasher() const { return hasher_; }
   private:
    Hasher* hasher_;
  };

  void SetHash(const StringPiece& hash);
  OutputWriter* BeginWrite(MessageHandler* message_handler);
  bool EndWrite(OutputWriter* writer, MessageHandler* message_handler);

  std::string TempPrefix() const;
  std::string NameTail() const;

  FileSystem::OutputFile* output_file_;
  bool writing_complete_;
  std::string filter_prefix_;
  std::string name_;
  std::string hash_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_OUTPUT_RESOURCE_H_
