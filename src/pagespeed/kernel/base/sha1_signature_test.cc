// Copyright 2014 Google Inc.
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
// Author: jcrowell@google.com (Jeffrey Crowell)

#include "pagespeed/kernel/base/sha1_signature.h"

#include "testing/base/public/gunit.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

namespace {

class Sha1SignatureTest : public ::testing::Test {};

TEST_F(Sha1SignatureTest, CorrectSignatureSize) {
  // This test verifies that Sha1Signature outputs signatures of the proper
  // size.
  // SHA1 is 160 bit, which is 27 6-bit chars (there are 2 additional bits for
  // padding).
  GoogleString key(5000, 'A');
  GoogleString data(5000, 'Z');
  const int kMaxSignatureSize =
      SHA1Signature::ComputeSizeFromNumberOfBytes(SHA1Signature::kSHA1NumBytes);
  for (int i = 0; i <= kMaxSignatureSize; ++i) {
    SHA1Signature signature(i);
    EXPECT_EQ(i, signature.Sign("myKey", "myData").size());
    // Large key and data.
    EXPECT_EQ(i, signature.Sign(key, data).size());
  }
}

TEST_F(Sha1SignatureTest, SignaturesDiffer) {
#if ENABLE_URL_SIGNATURES
  SHA1Signature signature;
  // Basic sanity tests.
  // Different data under different keys.
  EXPECT_NE(signature.Sign("key1", "data1"), signature.Sign("key2", "data2"));
  // Same data under different keys.
  EXPECT_NE(signature.Sign("key1", "data2"), signature.Sign("key2", "data2"));
  // Same keys signing different data.
  EXPECT_NE(signature.Sign("key1", "data1"), signature.Sign("key1", "data2"));
  // Same keys signing the same data.
  EXPECT_EQ(signature.Sign("key1", "data1"), signature.Sign("key1", "data1"));
  // Compare against known signature.
  SHA1Signature signature_length_10(10);
  EXPECT_EQ("ijqEvNDQBl", signature_length_10.Sign("hello", "world"));
  // Compare against Python's implementation
  // python -c 'import base64, hmac;from hashlib import
  // sha1;key="key";data="data";hashed=hmac.new(key,data,sha1);print
  // base64.urlsafe_b64encode(hashed.digest()).rstrip("\n")[0:25]'
  SHA1Signature signature_length_25(25);
  EXPECT_EQ("EEFSxb_coHvGM-69RhmfAlXJ9",
            signature_length_25.Sign("key", "data"));
  EXPECT_EQ("RrTsWGEXFU2s1J1mTl1j_ciO-",
            signature_length_25.Sign("foo", "bar"));
#endif
}

}  // namespace

}  // namespace net_instaweb
