// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: jmaessen@google.com (Jan-Willem Maessen)

#ifndef PAGESPEED_KERNEL_UTIL_INPUT_FILE_NONCE_GENERATOR_H_
#define PAGESPEED_KERNEL_UTIL_INPUT_FILE_NONCE_GENERATOR_H_

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/util/nonce_generator.h"

namespace net_instaweb {

class MessageHandler;

// Implements a NonceGenerator that simply draws data from a cryptographic file
// such as /dev/urandom.
class InputFileNonceGenerator : public NonceGenerator {
 public:
  // Takes ownership of file and mutex, but not of handler or file_system.
  InputFileNonceGenerator(FileSystem::InputFile* file, FileSystem* file_system,
                          AbstractMutex* mutex, MessageHandler* handler)
      : NonceGenerator(mutex), file_(file),
        file_system_(file_system), handler_(handler) { }
  virtual ~InputFileNonceGenerator();

 protected:
  virtual uint64 NewNonceImpl();

 private:
  FileSystem::InputFile* file_;
  FileSystem* file_system_;
  MessageHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(InputFileNonceGenerator);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_INPUT_FILE_NONCE_GENERATOR_H_
