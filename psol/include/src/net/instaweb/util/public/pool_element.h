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
//
// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_POOL_ELEMENT_H_
#define NET_INSTAWEB_UTIL_PUBLIC_POOL_ELEMENT_H_

#include <list>

// Include this file when defining an object that will reside in a pool.  There
// are a couple of ways of defining such an object, but all of them require us
// to use the PoolPosition typedef.  Most simply, we can extend the PoolElement
// type defined here---but in practice, we want to avoid multiple inheritance
// just to store a simple back link, and we're better off providing an accessor
// at pool construction time instead.
namespace net_instaweb {

template<class T>
class PoolElement {
 public:
  typedef typename std::list<T*>::iterator Position;

  PoolElement() { }

  // Returns a pointer to a mutable location holding the position of
  // the element in any containing pool.
  Position* pool_position() { return &pool_position_; }

 private:
  Position pool_position_;

  DISALLOW_COPY_AND_ASSIGN(PoolElement);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_POOL_ELEMENT_H_
