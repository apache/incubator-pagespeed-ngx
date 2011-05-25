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

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_MOCK_RESOURCE_CALLBACK_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_MOCK_RESOURCE_CALLBACK_H_

#include "net/instaweb/rewriter/public/resource.h"

namespace net_instaweb {

class MockResourceCallback : public Resource::AsyncCallback {
 public:
  MockResourceCallback(const ResourcePtr& resource)
      : Resource::AsyncCallback(resource) {}
  virtual ~MockResourceCallback();

  virtual void Done(bool success) {
    success_ = success;
    done_ = true;
  }

  bool success() const { return success_; }
  bool done() const { return done_; }

 private:
  bool success_;
  bool done_;

  DISALLOW_COPY_AND_ASSIGN(MockResourceCallback);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_MOCK_RESOURCE_CALLBACK_H_
