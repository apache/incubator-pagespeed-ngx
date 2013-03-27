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

#ifndef PAGESPEED_PLATFORM_IE_IE_DOM_H_
#define PAGESPEED_PLATFORM_IE_IE_DOM_H_

struct IHTMLDocument3;

namespace pagespeed {

class DomDocument;

namespace ie {

DomDocument* CreateDocument(IHTMLDocument3* document);

}  // namespace ie

}  // namespace pagespeed

#endif  // PAGESPEED_PLATFORM_IE_IE_DOM_H_
