// Copyright 2013 Google Inc.
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
//
// Author: sligocki@google.com (Shawn Ligocki)
//
// Content handlers that are usable by any implementation of PSOL.

#ifndef NET_INSTAWEB_SYSTEM_PUBLIC_HANDLERS_H_
#define NET_INSTAWEB_SYSTEM_PUBLIC_HANDLERS_H_

namespace net_instaweb {

class MessageHandler;
class SystemRewriteOptions;
class Writer;

// Handler which serves PSOL console.
// Note: ConsoleHandler always succeeds.
void ConsoleHandler(SystemRewriteOptions* options, Writer* writer,
                    MessageHandler* handler);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SYSTEM_PUBLIC_HANDLERS_H_
