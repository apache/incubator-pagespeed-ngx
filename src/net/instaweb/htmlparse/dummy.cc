/*
 * Copyright 2013 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
//
// This file exists solely so that we can provide the instaweb_htmlparse_core
// target PageSpeed Insights wants --- the OS X build wants
// libraries to have at least /some/ code of their own.

namespace net_instaweb {

int instaweb_htmlparse_core_dummy_function() {
  return 0;
}

}  // namespace net_instaweb
