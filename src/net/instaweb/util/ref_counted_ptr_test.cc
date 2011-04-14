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

// Unit-test RefCountedPtr.

#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

int counter = 0;

class BaseClass {
 public:
  BaseClass() : index_(counter++) {}
  int index() const { return index_; }

 private:
  int index_;
  DISALLOW_COPY_AND_ASSIGN(BaseClass);
};

struct SimpleClass : public BaseClass {};
struct DerivedA : public BaseClass {};
struct DerivedB : public BaseClass {};

}  // namespace

typedef RefCountedObj<SimpleClass> SimplePtr;
typedef RefCountedPtr<BaseClass> PolymorphicPtr;

class RefCountedPtrTest : public testing::Test {
 protected:
};

TEST_F(RefCountedPtrTest, Simple) {
  SimplePtr simple1;
  EXPECT_TRUE(simple1.unique());
  int index = simple1->index();
  SimplePtr simple2 = simple1;
  EXPECT_FALSE(simple1.unique());
  EXPECT_FALSE(simple2.unique());
  EXPECT_EQ(index, simple2->index());
  SimplePtr simple3(simple1);
  EXPECT_FALSE(simple3.unique());
  EXPECT_EQ(index, simple3->index());
  SimplePtr simple4;
  EXPECT_TRUE(simple4.unique());
  EXPECT_NE(index, simple4->index());
}

TEST_F(RefCountedPtrTest, Polymorphic) {
  PolymorphicPtr poly1(new DerivedA);
  int index = poly1->index();
  EXPECT_TRUE(poly1.unique());
  PolymorphicPtr poly2(poly1);
  EXPECT_FALSE(poly1.unique());
  EXPECT_FALSE(poly2.unique());
  EXPECT_EQ(index, poly2->index());
  PolymorphicPtr poly3 = poly1;
  EXPECT_FALSE(poly3.unique());
  EXPECT_EQ(index, poly3->index());
  PolymorphicPtr poly4(new DerivedB);
  EXPECT_TRUE(poly4.unique());
  EXPECT_NE(index, poly4->index());
  PolymorphicPtr poly5;
  EXPECT_TRUE(poly5.get() == NULL);
  EXPECT_TRUE(poly5.unique());
}

}  // namespace net_instaweb
