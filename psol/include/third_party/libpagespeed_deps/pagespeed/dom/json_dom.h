// Copyright 2011 Google Inc.
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

#ifndef PAGESPEED_DOM_JSON_DOM_H_
#define PAGESPEED_DOM_JSON_DOM_H_

#include "pagespeed/core/dom.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace pagespeed {
namespace dom {

// Create DomDocument from JSON. The created docuemnt owns the JSON.
pagespeed::DomDocument* CreateDocument(const base::DictionaryValue* json);

}  // namespace dom
}  // namespace pagespeed

#endif  // PAGESPEED_DOM_JSON_DOM_H_
