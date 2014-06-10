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

#include "pagespeed/kernel/base/sha1_signature.h"

#include <algorithm>

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#if ENABLE_URL_SIGNATURES
#include "third_party/openssl/openssl/crypto/hmac/hmac.h"
#endif

namespace net_instaweb {

namespace {

int CeilDivide(int numerator, int denominator) {
  int num = numerator + denominator - 1;
  return num / denominator;
}

}  // namespace

SHA1Signature::SHA1Signature()
    : Signature(), max_chars_(kDefaultSignatureSize) {}

SHA1Signature::SHA1Signature(int signature_size)
    : Signature(), max_chars_(signature_size) {}

SHA1Signature::~SHA1Signature() {}

GoogleString SHA1Signature::RawSign(StringPiece key, StringPiece data) const {
  unsigned int signature_length;
#if ENABLE_URL_SIGNATURES
  unsigned char* md = HMAC(EVP_sha1(), key.data(), key.size(),
                           reinterpret_cast<const unsigned char*>(data.data()),
                           data.size(), NULL, &signature_length);
  const unsigned char* result = const_cast<const unsigned char*>(md);
#else
  const unsigned char result[kSHA1NumBytes] = {0};
  signature_length = kSHA1NumBytes;
#endif
  GoogleString signature(reinterpret_cast<const char*>(result),
                         signature_length);
  return signature;
}

int SHA1Signature::RawSignatureSizeInBytes() const {
  return kSHA1NumBytes;
}

int SHA1Signature::SignatureSizeInChars() const {
  int max_length = ComputeSizeFromNumberOfBytes(RawSignatureSizeInBytes());
  return std::min(max_length, max_chars_);
}


int SHA1Signature::ComputeSizeFromNumberOfBytes(int num_bytes) {
  return CeilDivide(num_bytes * 4, 3);
}

}  // namespace net_instaweb
