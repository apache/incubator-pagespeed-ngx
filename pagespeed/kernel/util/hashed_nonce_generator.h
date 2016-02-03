// Copyright 2013 Google Inc. All Rights Reserved.
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

// Author: jmaessen@google.com (Jan-Willem Maessen)

#ifndef PAGESPEED_KERNEL_UTIL_HASHED_NONCE_GENERATOR_H_
#define PAGESPEED_KERNEL_UTIL_HASHED_NONCE_GENERATOR_H_

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/util/nonce_generator.h"

namespace net_instaweb {

class Hasher;

// Implements a NonceGenerator using a hasher and a count, starting from an
// initial secret.  See:
// http://en.wikipedia.org/wiki/Cryptographically_secure_pseudorandom_number_generator
// Basically we initialize a key and a counter with random data, then return the
// hash of the string obtained by appending them.  Incrementing the counter
// gives us a new hash.  This means that different instantiations of a
// HashedNonceGenerator must use different keys to avoid repetition of nonce
// values.  Note that (according to the above article) this is sufficient for
// cryptographic nonce generation, but not for generating a cryptographically
// secure bit stream for use as a one-time pad.
class HashedNonceGenerator : public NonceGenerator {
 public:
  // key must be at least 2*hasher->RawHashSizeInBytes() in length.
  // Takes ownership of mutex, but not of hasher.
  HashedNonceGenerator(
      const Hasher* hasher, StringPiece key, AbstractMutex* mutex);
  virtual ~HashedNonceGenerator();

 protected:
  virtual uint64 NewNonceImpl();

 private:
  const Hasher* hasher_;
  scoped_array<char> key_;
  int key_size_;
  uint64 counter_;

  DISALLOW_COPY_AND_ASSIGN(HashedNonceGenerator);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_HASHED_NONCE_GENERATOR_H_
