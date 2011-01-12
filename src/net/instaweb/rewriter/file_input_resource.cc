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

#include "net/instaweb/rewriter/public/file_input_resource.h"

#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/http/public/response_headers.h"

namespace net_instaweb {

FileInputResource::~FileInputResource() {
}

bool FileInputResource::Load(MessageHandler* message_handler) {
  FileSystem* file_system = resource_manager_->file_system();
  if (file_system->ReadFile(filename_.c_str(), &value_, message_handler)) {
    resource_manager_->SetDefaultHeaders(type_, &meta_data_);
    value_.SetHeaders(&meta_data_);
  }
  return loaded();
}

}  // namespace net_instaweb
