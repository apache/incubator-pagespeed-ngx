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

#ifndef NET_INSTAWEB_APACHE_MD5_HASHER_H_
#define NET_INSTAWEB_APACHE_MD5_HASHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/md5.h"
#include "net/instaweb/util/public/hasher.h"

using net_instaweb::Hasher;

namespace net_instaweb {

class Md5Hasher : public Hasher {
 public:
  Md5Hasher() { }
  virtual ~Md5Hasher();

  // Interface to accummulate a hash of data.
  virtual void Reset() { MD5Init(&ctx_); }
  virtual void Add(const net_instaweb::StringPiece& content) {
    MD5Update(&ctx_, content.data(), content.size());
  }
  virtual void ComputeHash(std::string* hash);
 private:
  MD5Context ctx_;

  DISALLOW_COPY_AND_ASSIGN(Md5Hasher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_MD5_HASHER_H_
