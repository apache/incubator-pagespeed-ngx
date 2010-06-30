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
// Input resources are created by a ResourceManager. They must be able to
// read contents.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_INPUT_RESOURCE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_INPUT_RESOURCE_H_

#include <string>

namespace net_instaweb {

class MessageHandler;
class MetaData;

class InputResource {
 public:
  virtual ~InputResource();

  // Read complete resource, contents are stored in contents_.
  virtual bool Read(MessageHandler* message_handler) = 0;

  // Getters
  virtual const std::string& url() const = 0;
  virtual const std::string& absolute_url() const = 0;
  virtual bool loaded() const = 0;  // Has file been read/loaded.
  // contents are only available when loaded()
  // ContentsValid(): Based on what was read/loaded, do the contents contain
  // the data for the requested resource?  Implies loaded().
  virtual bool ContentsValid() const = 0;
  virtual const std::string& contents() const = 0;
  virtual const MetaData* metadata() const = 0;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_INPUT_RESOURCE_H_
