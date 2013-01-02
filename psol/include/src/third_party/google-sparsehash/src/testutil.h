// Copyright (c) 2010, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---
// Author: Craig Silverstein

// This macro mimics a unittest framework, but is a bit less flexible
// than most.  It requires a superclass to derive from, and does all
// work in global constructors.  The tricky part is implementing
// TYPED_TEST.

#ifndef SPARSEHASH_TEST_UTIL_H_
#define SPARSEHASH_TEST_UTIL_H_

#include "config.h"
#include <stdio.h>

_START_GOOGLE_NAMESPACE_

namespace testing {

#define EXPECT_TRUE(cond)  do {                         \
  if (!(cond)) {                                        \
    ::fputs("Test failed: " #cond "\n", stderr);        \
    ::exit(1);                                          \
  }                                                     \
} while (0)

#define EXPECT_FALSE(a)  EXPECT_TRUE(!(a))
#define EXPECT_EQ(a, b)  EXPECT_TRUE((a) == (b))
#define EXPECT_LT(a, b)  EXPECT_TRUE((a) < (b))
#define EXPECT_GT(a, b)  EXPECT_TRUE((a) > (b))
#define EXPECT_LE(a, b)  EXPECT_TRUE((a) <= (b))
#define EXPECT_GE(a, b)  EXPECT_TRUE((a) >= (b))

#define TEST(suitename, testname)                                       \
  class TEST_##suitename##_##testname {                                 \
   public:                                                              \
    TEST_##suitename##_##testname() {                                   \
      ::fputs("Running " #suitename "." #testname "\n", stderr);        \
      Run();                                                            \
    }                                                                   \
    void Run();                                                         \
  };                                                                    \
  static TEST_##suitename##_##testname                                  \
      test_instance_##suitename##_##testname;                           \
  void TEST_##suitename##_##testname::Run()


template<typename C1, typename C2, typename C3, typename C4, typename C5,
         typename C6> struct TypeList6 {
  typedef C1 type1;
  typedef C2 type2;
  typedef C3 type3;
  typedef C4 type4;
  typedef C5 type5;
  typedef C6 type6;
};

// I need to list 18 types here, for code below to compile, though
// only the first 6 are ever used.
#define TYPED_TEST_CASE_6(classname, typelist)  \
  typedef typelist::type1 classname##_type1;    \
  typedef typelist::type2 classname##_type2;    \
  typedef typelist::type3 classname##_type3;    \
  typedef typelist::type4 classname##_type4;    \
  typedef typelist::type5 classname##_type5;    \
  typedef typelist::type6 classname##_type6;    \
  static const int classname##_numtypes = 6;    \
  typedef typelist::type1 classname##_type7;    \
  typedef typelist::type1 classname##_type8;    \
  typedef typelist::type1 classname##_type9;    \
  typedef typelist::type1 classname##_type10;   \
  typedef typelist::type1 classname##_type11;   \
  typedef typelist::type1 classname##_type12;   \
  typedef typelist::type1 classname##_type13;   \
  typedef typelist::type1 classname##_type14;   \
  typedef typelist::type1 classname##_type15;   \
  typedef typelist::type1 classname##_type16;   \
  typedef typelist::type1 classname##_type17;   \
  typedef typelist::type1 classname##_type18;

template<typename C1, typename C2, typename C3, typename C4, typename C5,
         typename C6, typename C7, typename C8, typename C9, typename C10,
         typename C11, typename C12, typename C13, typename C14, typename C15,
         typename C16, typename C17, typename C18> struct TypeList18 {
  typedef C1 type1;
  typedef C2 type2;
  typedef C3 type3;
  typedef C4 type4;
  typedef C5 type5;
  typedef C6 type6;
  typedef C7 type7;
  typedef C8 type8;
  typedef C9 type9;
  typedef C10 type10;
  typedef C11 type11;
  typedef C12 type12;
  typedef C13 type13;
  typedef C14 type14;
  typedef C15 type15;
  typedef C16 type16;
  typedef C17 type17;
  typedef C18 type18;
};

#define TYPED_TEST_CASE_18(classname, typelist)  \
  typedef typelist::type1 classname##_type1;    \
  typedef typelist::type2 classname##_type2;    \
  typedef typelist::type3 classname##_type3;    \
  typedef typelist::type4 classname##_type4;    \
  typedef typelist::type5 classname##_type5;    \
  typedef typelist::type6 classname##_type6;    \
  typedef typelist::type7 classname##_type7;    \
  typedef typelist::type8 classname##_type8;    \
  typedef typelist::type9 classname##_type9;    \
  typedef typelist::type10 classname##_type10;    \
  typedef typelist::type11 classname##_type11;    \
  typedef typelist::type12 classname##_type12;    \
  typedef typelist::type13 classname##_type13;    \
  typedef typelist::type14 classname##_type14;    \
  typedef typelist::type15 classname##_type15;    \
  typedef typelist::type16 classname##_type16;    \
  typedef typelist::type17 classname##_type17;    \
  typedef typelist::type18 classname##_type18;    \
  static const int classname##_numtypes = 18;

#define TYPED_TEST(superclass, testname)                                \
  template<typename TypeParam>                                          \
  class TEST_onetype_##superclass##_##testname :                        \
      public superclass<TypeParam> {                                    \
   public:                                                              \
    TEST_onetype_##superclass##_##testname() {                          \
      Run();                                                            \
    }                                                                   \
   private:                                                             \
    void Run();                                                         \
  };                                                                    \
  class TEST_typed_##superclass##_##testname {                          \
   public:                                                              \
    explicit TEST_typed_##superclass##_##testname() {                   \
      if (superclass##_numtypes >= 1) {                                 \
        ::fputs("Running " #superclass "." #testname ".1\n", stderr);   \
        TEST_onetype_##superclass##_##testname<superclass##_type1> t;   \
      }                                                                 \
      if (superclass##_numtypes >= 2) {                                 \
        ::fputs("Running " #superclass "." #testname ".2\n", stderr);   \
        TEST_onetype_##superclass##_##testname<superclass##_type2> t;   \
      }                                                                 \
      if (superclass##_numtypes >= 3) {                                 \
        ::fputs("Running " #superclass "." #testname ".3\n", stderr);   \
        TEST_onetype_##superclass##_##testname<superclass##_type3> t;   \
      }                                                                 \
      if (superclass##_numtypes >= 4) {                                 \
        ::fputs("Running " #superclass "." #testname ".4\n", stderr);   \
        TEST_onetype_##superclass##_##testname<superclass##_type4> t;   \
      }                                                                 \
      if (superclass##_numtypes >= 5) {                                 \
        ::fputs("Running " #superclass "." #testname ".5\n", stderr);   \
        TEST_onetype_##superclass##_##testname<superclass##_type5> t;   \
      }                                                                 \
      if (superclass##_numtypes >= 6) {                                 \
        ::fputs("Running " #superclass "." #testname ".6\n", stderr);   \
        TEST_onetype_##superclass##_##testname<superclass##_type6> t;   \
      }                                                                 \
      if (superclass##_numtypes >= 7) {                                 \
        ::fputs("Running " #superclass "." #testname ".7\n", stderr);   \
        TEST_onetype_##superclass##_##testname<superclass##_type7> t;   \
      }                                                                 \
      if (superclass##_numtypes >= 8) {                                 \
        ::fputs("Running " #superclass "." #testname ".8\n", stderr);   \
        TEST_onetype_##superclass##_##testname<superclass##_type8> t;   \
      }                                                                 \
      if (superclass##_numtypes >= 9) {                                 \
        ::fputs("Running " #superclass "." #testname ".9\n", stderr);   \
        TEST_onetype_##superclass##_##testname<superclass##_type9> t;   \
      }                                                                 \
      if (superclass##_numtypes >= 10) {                                \
        ::fputs("Running " #superclass "." #testname ".10\n", stderr);  \
        TEST_onetype_##superclass##_##testname<superclass##_type10> t;  \
      }                                                                 \
      if (superclass##_numtypes >= 11) {                                \
        ::fputs("Running " #superclass "." #testname ".11\n", stderr);  \
        TEST_onetype_##superclass##_##testname<superclass##_type11> t;  \
      }                                                                 \
      if (superclass##_numtypes >= 12) {                                \
        ::fputs("Running " #superclass "." #testname ".12\n", stderr);  \
        TEST_onetype_##superclass##_##testname<superclass##_type12> t;  \
      }                                                                 \
      if (superclass##_numtypes >= 13) {                                \
        ::fputs("Running " #superclass "." #testname ".13\n", stderr);  \
        TEST_onetype_##superclass##_##testname<superclass##_type13> t;  \
      }                                                                 \
      if (superclass##_numtypes >= 14) {                                \
        ::fputs("Running " #superclass "." #testname ".14\n", stderr);  \
        TEST_onetype_##superclass##_##testname<superclass##_type14> t;  \
      }                                                                 \
      if (superclass##_numtypes >= 15) {                                \
        ::fputs("Running " #superclass "." #testname ".15\n", stderr);  \
        TEST_onetype_##superclass##_##testname<superclass##_type15> t;  \
      }                                                                 \
      if (superclass##_numtypes >= 16) {                                \
        ::fputs("Running " #superclass "." #testname ".16\n", stderr);  \
        TEST_onetype_##superclass##_##testname<superclass##_type16> t;  \
      }                                                                 \
      if (superclass##_numtypes >= 17) {                                \
        ::fputs("Running " #superclass "." #testname ".17\n", stderr);  \
        TEST_onetype_##superclass##_##testname<superclass##_type17> t;  \
      }                                                                 \
      if (superclass##_numtypes >= 18) {                                \
        ::fputs("Running " #superclass "." #testname ".18\n", stderr);  \
        TEST_onetype_##superclass##_##testname<superclass##_type18> t;  \
      }                                                                 \
    }                                                                   \
  };                                                                    \
  static TEST_typed_##superclass##_##testname                           \
      test_instance_typed_##superclass##_##testname;                    \
  template<class TypeParam>                                             \
  void TEST_onetype_##superclass##_##testname<TypeParam>::Run()

} // namespace testing

_END_GOOGLE_NAMESPACE_

#endif  // SPARSEHASH_TEST_UTIL_H_
