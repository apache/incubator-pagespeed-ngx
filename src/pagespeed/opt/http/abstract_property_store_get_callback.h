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

// Author: pulkitg@google.com (Pulkit Goyal)

#ifndef PAGESPEED_OPT_HTTP_ABSTRACT_PROPERTY_STORE_GET_CALLBACK_H_
#define PAGESPEED_OPT_HTTP_ABSTRACT_PROPERTY_STORE_GET_CALLBACK_H_

#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

// Abstract class which manages the storage system lookup. Any PropertyStore can
// derive from this class and objects of this class are created by
// PropertyStore::Get().
// PropertyStores must return an implementation of this interface from Get.
// TODO(pulkitg): Revisit this to make this inner class of PropertyStore.
class AbstractPropertyStoreGetCallback {
 public:
  // Try to finish all the pending lookups if possible and call Done as soon as
  // possible.
  virtual void FastFinishLookup() = 0;
  // Deletes this after Done has run.
  // Callback is no more useful after this is called, so it should be called
  // only after all required operations are done by client on this callback.
  virtual void DeleteWhenDone() = 0;

 protected:
  AbstractPropertyStoreGetCallback();
  virtual ~AbstractPropertyStoreGetCallback();

 private:
  DISALLOW_COPY_AND_ASSIGN(AbstractPropertyStoreGetCallback);
};

}  // namespace net_instaweb
#endif  // PAGESPEED_OPT_HTTP_ABSTRACT_PROPERTY_STORE_GET_CALLBACK_H_
