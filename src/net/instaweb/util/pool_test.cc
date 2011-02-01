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

#include "net/instaweb/util/public/pool.h"

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/pool_element.h"
#include <string>

namespace net_instaweb {

namespace {

// Pool element containing an int, for test purposes.
class IntElement : public PoolElement<IntElement> {
 public:
  IntElement() { }

  const int num() const { return num_; }
  void set_num(int num) { num_ = num; }

 private:
  int num_;

  DISALLOW_COPY_AND_ASSIGN(IntElement);
};

typedef Pool<IntElement> IntPool;
typedef PoolElement<IntElement>::Position PoolPosition;

class PoolTest : public testing::Test {
 protected:
  PoolTest() {
    for (int i = 0; i < arraysize(elements_); ++i) {
      elements_[i].set_num(i);
    }
  }

  ~PoolTest() {
    pool_.Clear();
  }

  // Add just the ith element to pool_
  void Add(int i) {
    int sz = pool_.size();
    pool_.Add(&elements_[i]);
    EXPECT_FALSE(pool_.empty());
    EXPECT_EQ(sz + 1, pool_.size());
  }

  // Add the first n elements_ to pool_ for test setup.
  void Adds(int n) {
    for (int i = 0; i < n; ++i) {
      Add(i);
    }
  }

  // Expect that pool_ contains the n elements in expected, in order.
  void ExpectContainsElements(int n, const int* expected) {
    EXPECT_EQ(n, pool_.size());
    PoolPosition iter = pool_.begin();
    for (int i = 0; i < n; ++i) {
      EXPECT_EQ(expected[i], (*iter)->num()) << "Actually " << Dump();
      ++iter;
    }
  }

  // Expect that pool_ contains the numbers in [lo,hi], in order.
  void ExpectContains(int lo, int hi) {
    EXPECT_EQ(hi - lo + 1, pool_.size());
    IntPool::iterator iter = pool_.begin();
    for (int i = lo; i <= hi; ++i) {
      EXPECT_EQ(i, (*iter)->num()) << "Actually " << Dump();
      ++iter;
    }
  }

  // Expect that the next element removed will be i
  void ExpectRemoveOldest(int i) {
    int sz = pool_.size();
    EXPECT_FALSE(pool_.empty());
    EXPECT_EQ(i, pool_.RemoveOldest()->num());
    EXPECT_EQ(sz - 1, pool_.size());
  }

  // Remove the element i from pool_
  void Remove(int i) {
    int sz = pool_.size();
    EXPECT_FALSE(pool_.empty());
    EXPECT_EQ(i, pool_.Remove(&elements_[i])->num());
    EXPECT_EQ(sz - 1, pool_.size());
  }

  // Dump the pool value.  Yields, dirty, and compact string (but isn't robust
  // if we were to use enormous pool sizes during testing; since we're backed by
  // collections this should not be necessary).
  std::string Dump() {
    std::string buf;
    for (IntPool::iterator iter = pool_.begin(); iter != pool_.end(); ++iter) {
      buf.push_back((*iter)->num() + '0');
    }
    return buf;
  }

  IntPool pool_;
  IntElement elements_[4];
 private:
  DISALLOW_COPY_AND_ASSIGN(PoolTest);
};

TEST_F(PoolTest, TestInsertAndOrderedRemoveOldest) {
  EXPECT_TRUE(pool_.empty());
  EXPECT_EQ(0, pool_.size());
  Adds(4);
  ExpectContains(0, 3);
  ExpectRemoveOldest(0);
  ExpectRemoveOldest(1);
  ExpectRemoveOldest(2);
  ExpectRemoveOldest(3);
  EXPECT_TRUE(pool_.empty());
  EXPECT_TRUE(NULL == pool_.RemoveOldest());
}

TEST_F(PoolTest, TestInsertAndRemove) {
  Adds(4);
  ExpectContains(0, 3);
  Remove(0);
  ExpectContains(1, 3);
  Remove(1);
  ExpectContains(2, 3);
  Remove(2);
  ExpectContains(3, 3);
  Remove(3);
  EXPECT_TRUE(pool_.empty());
}

TEST_F(PoolTest, TestRemoveAndReinsertFront) {
  Adds(4);
  ExpectContains(0, 3);
  Remove(3);
  ExpectContains(0, 2);
  Add(3);
  ExpectContains(0, 3);
}

TEST_F(PoolTest, TestRemoveAndReinsertBack) {
  Adds(4);
  ExpectContains(0, 3);
  Remove(0);
  ExpectContains(1, 3);
  Add(0);
  ExpectRemoveOldest(1);
  ExpectRemoveOldest(2);
  ExpectRemoveOldest(3);
  ExpectRemoveOldest(0);
}

TEST_F(PoolTest, TestRemoveAndReinsertMiddle) {
  Adds(4);
  ExpectContains(0, 3);
  Remove(2);
  const int kMiddleExpected[] = { 0, 1, 3 };
  ExpectContainsElements(arraysize(kMiddleExpected), kMiddleExpected);
  Add(2);
  ExpectRemoveOldest(0);
  ExpectRemoveOldest(1);
  ExpectRemoveOldest(3);
  ExpectRemoveOldest(2);
}

TEST_F(PoolTest, TestClear) {
  Adds(4);
  ExpectContains(0, 3);
  pool_.Clear();
  EXPECT_TRUE(pool_.empty());
}

}  // namespace

}  // namespace net_instaweb
