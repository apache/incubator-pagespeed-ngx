/*
 * Copyright 2012 Google Inc.
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

// Author: pulkitg@google.com (Pulkit Goyal)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_IMAGES_CALLBACK_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_IMAGES_CALLBACK_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// This callback class is used to update the critical or above the fold images
// set so that this information can be used by DelayImagesFilter.
class CriticalImagesCallback {
 public:
  CriticalImagesCallback() {}
  virtual ~CriticalImagesCallback();
  virtual void Done(const StringSet& images_url,
                    bool success) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CriticalImagesCallback);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_IMAGES_CALLBACK_H_
