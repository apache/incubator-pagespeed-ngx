// Copyright 2012 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_BROWSING_CONTEXT_BROWSING_CONTEXT_FACTORY_H_
#define PAGESPEED_BROWSING_CONTEXT_BROWSING_CONTEXT_FACTORY_H_

#include "base/basictypes.h"

namespace pagespeed {

class DomDocument;
class PagespeedInput;
class Resource;
class TopLevelBrowsingContext;

namespace browsing_context {

class BrowsingContextFactory {
 public:
  explicit BrowsingContextFactory(const PagespeedInput* pagespeed_input);

  pagespeed::TopLevelBrowsingContext* CreateTopLevelBrowsingContext(
      const DomDocument* document, const Resource* primary_resource);

 private:
  const pagespeed::PagespeedInput* const pagespeed_input_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingContextFactory);
};

}  // namespace browsing_context
}  // namespace pagespeed

#endif  // PAGESPEED_BROWSING_CONTEXT_BROWSING_CONTEXT_FACTORY_H_
