/*
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

#include "net/instaweb/rewriter/public/file_input_resource.h"

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class MessageHandler;

FileInputResource::~FileInputResource() {
}

// File input resources don't have expirations, we assume that the resource
// is valid as long as the FileInputResource lives.
bool FileInputResource::IsValidAndCacheable() {
  // TODO(sligocki): Stat the file to make sure it hasn't changed since load.
  // Do we really want to stat it here? We are currently statting it in
  // RewriteContext::OutputPartitionIsValid.
  return response_headers_.status_code() == HttpStatus::kOK;
}

void FileInputResource::FillInPartitionInputInfo(InputInfo* input) {
  CHECK(loaded());
  input->set_type(InputInfo::FILE_BASED);
  input->set_last_modified_time_ms(last_modified_time_sec_ * Timer::kSecondMs);
}

// TODO(sligocki): Is this reasonable? People might want custom headers.
//
// For example, Content-Type is set solely by file extension and will not
// be set if the extension is unknown :/
//
// We also set no Date, Last-Modified, Cache-Control, etc. headers.
void FileInputResource::SetDefaultHeaders(const ContentType* content_type,
                                          ResponseHeaders* header,
                                          MessageHandler* handler) {
  header->set_major_version(1);
  header->set_minor_version(1);
  header->SetStatusAndReason(HttpStatus::kOK);
  header->RemoveAll(HttpAttributes::kContentType);
  if (content_type == NULL) {
    handler->Message(kError, "Loaded resource with no Content-Type %s",
                     url_.c_str());
  } else {
    header->Add(HttpAttributes::kContentType, content_type->mime_type());
  }
}

// Note: We do not save this resource to the HttpCache, so it will be
// reloaded for every request.
bool FileInputResource::Load(MessageHandler* handler) {
  FileSystem* file_system = resource_manager_->file_system();
  if (file_system->ReadFile(filename_.c_str(), &value_, handler) &&
      file_system->Mtime(filename_, &last_modified_time_sec_, handler)) {
    SetDefaultHeaders(type_, &response_headers_, handler);
    value_.SetHeaders(&response_headers_);
  }
  return loaded();
}

}  // namespace net_instaweb
