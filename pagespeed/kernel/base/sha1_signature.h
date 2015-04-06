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
// Implementation for a signature function.

#ifndef PAGESPEED_KERNEL_BASE_SHA1_SIGNATURE_H_
#define PAGESPEED_KERNEL_BASE_SHA1_SIGNATURE_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/signature.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

// Implementation class of Signature, using HMAC-SHA1 for signing.
class SHA1Signature : public Signature {
 public:
  static const int kDefaultSignatureSize = 10;
  static const int kSHA1NumBytes = 20;

  SHA1Signature();
  explicit SHA1Signature(int signature_size);
  virtual ~SHA1Signature();
  int SignatureSizeInChars() const;
  static int ComputeSizeFromNumberOfBytes(int num_bytes);

 protected:
  virtual GoogleString RawSign(StringPiece key, StringPiece data) const;
  virtual int RawSignatureSizeInBytes() const;

 private:
  int max_chars_;

  DISALLOW_COPY_AND_ASSIGN(SHA1Signature);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_SHA1_SIGNATURE_H_
