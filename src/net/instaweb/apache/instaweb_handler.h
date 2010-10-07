// Copyright 2010 Google Inc.
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

#ifndef MOD_INSTAWEB_INSTAWEB_HANDLER_H_
#define MOD_INSTAWEB_INSTAWEB_HANDLER_H_

// Forward declaration.
struct request_rec;

namespace net_instaweb {

// The content generator for instaweb generated content, for example, the
// combined CSS file.  Requests for not-instab generated content will be
// declined so that other Apache handlers may operate on them.
int instaweb_handler(request_rec* request);

}  // namespace net_instaweb

#endif  // MOD_INSTAWEB_INSTAWEB_HANDLER_H_
