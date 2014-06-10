/*
 * Copyright 2014 Google Inc.
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

// Author: jcrowell@google.com (Jeffrey Crowell)
//
// Interface for a signature function.

#ifndef PAGESPEED_KERNEL_BASE_SIGNATURE_H_
#define PAGESPEED_KERNEL_BASE_SIGNATURE_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

// To enable URL signing with HMAC-SHA1, we must link against OpenSSL,
// which is a a large library with licensing restrictions not known to
// be wholly inline with the Apache license.  To disable URL signing:
//   1. Set SIGN_URL to 0 here
//   2. Comment out the references to openssl.gyp in kernel.gyp.
//   3. Comment out all references to openssl in src/DEPS.
//

#ifndef ENABLE_URL_SIGNATURES
#define ENABLE_URL_SIGNATURES 1
#endif

namespace net_instaweb {

class Signature {
 public:
  // The passed in max_chars will be used to limit the length of Sign() and
  // SignatureSizeInChars().
  explicit Signature();
  virtual ~Signature();

  // Computes a web64-encoded signature of data under a given key.  Takes a
  // StringPiece for signing key, which is used for a pointer to the key and the
  // length of the signing key, and a StringPiece for the data, which is used
  // for a pointer to the data to sign, and the length of the data.
  GoogleString Sign(StringPiece key, StringPiece data) const;

  // Returns the string length of the signatures produced by the Sign() method.
  virtual int SignatureSizeInChars() const = 0;

 protected:
  // Computes a binary signature of a given data under key.
  virtual GoogleString RawSign(StringPiece key, StringPiece data) const = 0;
  // The number of bytes RawSign will produce.
  virtual int RawSignatureSizeInBytes() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Signature);
};

} // namespace net_instaweb


#endif  // PAGESPEED_KERNEL_BASE_SIGNATURE_H_
