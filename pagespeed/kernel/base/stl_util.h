/*
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef PAGESPEED_KERNEL_BASE_STL_UTIL_H_
#define PAGESPEED_KERNEL_BASE_STL_UTIL_H_

#include <algorithm>
#include <vector>

#include "base/stl_util.h"

template<class T>
bool STLFind(const T& collection, const typename T::value_type& value) {
  return std::find(collection.begin(), collection.end(), value) !=
      collection.end();
}

#endif  // PAGESPEED_KERNEL_BASE_STL_UTIL_H_
