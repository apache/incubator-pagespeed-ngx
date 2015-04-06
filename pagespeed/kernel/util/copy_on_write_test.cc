/*
 * Copyright 2013 Google Inc.
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

#include "pagespeed/kernel/util/copy_on_write.h"

#include <vector>

#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {
namespace {

class IntVector : public std::vector<int> {
 public:
  void Merge(const IntVector& src) {
    for (int i = 0, n = src.size(); i < n; ++i) {
      push_back(src[i]);
    }
  }

  // Copy and assign OK.
};

class CopyOnWriteTest : public testing::Test {
 protected:
  virtual void SetUp() {
    IntVector* one_two = cow_int_vector_a_.MakeWriteable();
    one_two->push_back(1);
    one_two->push_back(2);
  }
  CopyOnWrite<IntVector> cow_int_vector_a_;
};

class ObjectWithNoDefaultCtor {
 public:
  explicit ObjectWithNoDefaultCtor(int x) {}
  ObjectWithNoDefaultCtor(const ObjectWithNoDefaultCtor& src) {}
  ObjectWithNoDefaultCtor& operator=(const ObjectWithNoDefaultCtor& src) {
    return *this;
  }
};

TEST_F(CopyOnWriteTest, CopyConstructorShares) {
  CopyOnWrite<IntVector> b(cow_int_vector_a_);
  EXPECT_EQ(cow_int_vector_a_.get(), b.get()) << "same storage";
  EXPECT_EQ(*cow_int_vector_a_, *b) << "same values";
  EXPECT_EQ(cow_int_vector_a_->size(), b->size()) << "same size";
}

TEST_F(CopyOnWriteTest, AssignmentOperatorShares) {
  CopyOnWrite<IntVector> b = cow_int_vector_a_;
  EXPECT_EQ(cow_int_vector_a_.get(), b.get()) << "same storage";
  EXPECT_EQ(*cow_int_vector_a_, *b) << "same values";
  EXPECT_EQ(cow_int_vector_a_->size(), b->size()) << "same size";
}

TEST_F(CopyOnWriteTest, UniquifyOnWrite) {
  CopyOnWrite<IntVector> b(cow_int_vector_a_);
  EXPECT_EQ(cow_int_vector_a_.get(), b.get()) << "shared before MakeWriteable";
  IntVector* b_ptr = b.MakeWriteable();

  // Right away the pointers are now different even though the content is
  // unchanged.
  EXPECT_NE(cow_int_vector_a_.get(), b.get()) << "unique storage";
  EXPECT_EQ(*cow_int_vector_a_, *b) << "same values -- not modified yet";

  (*b_ptr)[1] = 3;
  EXPECT_EQ(cow_int_vector_a_->size(), b->size()) << "still same size";
  EXPECT_NE(*cow_int_vector_a_, *b) << "but now different content";

  // Now make 'c' share with 'cow_int_vector_a_', and 'd' share with 'b'.
  CopyOnWrite<IntVector> c = cow_int_vector_a_;
  CopyOnWrite<IntVector> d = b;
  EXPECT_EQ(cow_int_vector_a_.get(), c.get());
  EXPECT_EQ(b.get(), d.get());
  EXPECT_NE(cow_int_vector_a_.get(), b.get());
}

TEST_F(CopyOnWriteTest, EmptyObjects) {
  CopyOnWrite<IntVector> empty1;
  CopyOnWrite<IntVector> empty2;
  EXPECT_NE(empty1.get(), empty2.get());
  EXPECT_EQ(*empty1.get(), *empty2.get());
  EXPECT_EQ(0, empty1->size());
  EXPECT_EQ(0, empty2->size());
}

TEST_F(CopyOnWriteTest, NoDefaultCtor) {
  // CopyOnWrite<ObjectWithNoDefaultCtor> fails_to_compile_if_uncommented;
  ObjectWithNoDefaultCtor obj(0);
  CopyOnWrite<ObjectWithNoDefaultCtor> cow_obj(obj);
  CopyOnWrite<ObjectWithNoDefaultCtor> cow_obj_copy(cow_obj);
  CopyOnWrite<ObjectWithNoDefaultCtor> cow_obj_assigned = cow_obj;
}

TEST_F(CopyOnWriteTest, MergeOrShareEmptySrc) {
  CopyOnWrite<IntVector> share(cow_int_vector_a_);
  CopyOnWrite<IntVector> empty;
  cow_int_vector_a_.MergeOrShare(empty);
  EXPECT_EQ(cow_int_vector_a_.get(), share.get()) << "same storage";
}

TEST_F(CopyOnWriteTest, MergeOrShareEmptyThis) {
  CopyOnWrite<IntVector> share(cow_int_vector_a_);
  CopyOnWrite<IntVector> empty;
  empty.MergeOrShare(cow_int_vector_a_);
  EXPECT_EQ(cow_int_vector_a_.get(), share.get()) << "same storage";
  EXPECT_EQ(cow_int_vector_a_.get(), empty.get()) << "same storage";
}

TEST_F(CopyOnWriteTest, MergeOrShareRequiringClassMerge) {
  CopyOnWrite<IntVector> share(cow_int_vector_a_);
  CopyOnWrite<IntVector> three;
  three.MakeWriteable()->push_back(3);
  cow_int_vector_a_.MergeOrShare(three);
  EXPECT_NE(cow_int_vector_a_.get(), share.get()) << "different storage";
  EXPECT_NE(cow_int_vector_a_.get(), three.get()) << "different storage";
  EXPECT_EQ(3, cow_int_vector_a_->size());
}

}  // namespace
}  // namespace net_instaweb
