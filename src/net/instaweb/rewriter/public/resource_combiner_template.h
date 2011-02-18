/*
 * Copyright 2011 Google Inc.
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

// Author: abliss@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_COMBINER_TEMPLATE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_COMBINER_TEMPLATE_H_

#include <vector>

#include "net/instaweb/rewriter/public/resource_combiner.h"

namespace net_instaweb {

class MessageHandler;

// A templatized extension of ResourceCombiner that can track elements of a
// custom type.
template<typename T>
class ResourceCombinerTemplate : public ResourceCombiner {
 public:
  ResourceCombinerTemplate(RewriteDriver* rewrite_driver,
                           const StringPiece& path_prefix,
                           const StringPiece& extension)
      : ResourceCombiner(rewrite_driver, path_prefix, extension) {
  }
  virtual ~ResourceCombinerTemplate() {}

  virtual bool AddElement(T element, const StringPiece& url,
                          MessageHandler* handler) {
    if (AddResource(url, handler)) {
      elements_.push_back(element);
      return true;
    }
    return false;
  }
  T element(int i) const { return elements_[i]; }

  virtual void Clear() {
    elements_.clear();
    ResourceCombiner::Clear();
  }
 protected:
  std::vector<T> elements_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_COMBINER_TEMPLATE_H_
