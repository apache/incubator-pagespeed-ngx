/*
 * Copyright 2011 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)

// Unit-test the arena

#include "net/instaweb/util/public/arena.h"

#include <cstddef>
#include <set>

#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class ArenaTest : public testing::Test {
 public:
  ArenaTest() {
    ClearStats();
  }

 protected:
  friend class KidA;
  friend class KidB;

  class Base {
   public:
    explicit Base(ArenaTest* owner) : owner_(owner) {}
    virtual ~Base() {
      // Watch out for double-delete
      EXPECT_TRUE(owner_ != NULL);
      owner_ = NULL;
    }

    // When testing creation, we invoke this method.
    virtual void Made() = 0;

    void* operator new(size_t size, Arena<Base>* arena) {
      return arena->Allocate(size);
    }

   protected:
    ArenaTest* owner_;
  };

  // We expect KidA to have size of 2 pointers: the vtable and owner,
  // (just like base). On 32-bit this will be 8-bytes, so with 8-byte
  // alignment area it will divide the block size
  class KidA : public Base {
   public:
    explicit KidA(ArenaTest* o) : Base(o) {}

    ~KidA() {
      ++owner_->destroyed_a_;
    }

    virtual void Made() {
      ++owner_->made_a_;
    }
  };

  // KidB is 3 pointers long, so on 64-bit along with the next pointer
  // we will be using 32 bytes per allocation and it will divide
  // the block size.
  //
  // The difference in size between A and B lets us test mixed combinations of
  // different sizes.
  class KidB : public Base {
   public:
    explicit KidB(ArenaTest* o) : Base(o) {}

    ~KidB() {
      ++owner_->destroyed_b_;
    }

    virtual void Made() {
      ++owner_->made_b_;
    }

   private:
    void* different_size;
  };

  // Tests a given mixture of allocations of KidA and KidB --
  // making sure we get sane pointers and delete things.
  void TestCombo(int num_a, int num_b) {
    for (int a = 0; a < num_a; ++a) {
      CheckPtr(new (&arena_) KidA(this));
    }

    for (int b = 0; b < num_b; ++b) {
      CheckPtr(new (&arena_) KidB(this));
    }

    arena_.DestroyObjects();

    EXPECT_EQ(num_a, made_a_);
    EXPECT_EQ(num_b, made_b_);
    EXPECT_EQ(num_a, destroyed_a_);
    EXPECT_EQ(num_b, destroyed_b_);
  }

  // Checks to make sure the pointer is sane, and calls Made on it.
  void CheckPtr(Base* p) {
    EXPECT_TRUE(seen_ptrs_.find(p) == seen_ptrs_.end());
    seen_ptrs_.insert(p);
    p->Made();
  }

  void ClearStats() {
    made_a_ = 0;
    made_b_ = 0;
    destroyed_a_ = 0;
    destroyed_b_ = 0;
    seen_ptrs_.clear();
  }

  int made_a_;
  int made_b_;
  int destroyed_a_;
  int destroyed_b_;
  Arena<Base> arena_;
  std::set<void*> seen_ptrs_;
};

// Empty arena should be OK without a Destroy
TEST_F(ArenaTest, TestEmpty) {
}

// calling Destroy on empty is fine.
TEST_F(ArenaTest, TestEmptyDestroy) {
  arena_.DestroyObjects();
}

TEST_F(ArenaTest, TestJustA) {
  TestCombo(10000, 0);
}

TEST_F(ArenaTest, TestJustA2) {
  // On 32-bit this should perfectly fill all the blocks it uses
  TestCombo(2048, 0);
}

TEST_F(ArenaTest, TestJustB) {
  TestCombo(0, 10000);
}

TEST_F(ArenaTest, TestJustB2) {
  // On 64-bit this should perfectly fill all the blocks it uses
  TestCombo(0, 2048);
}

TEST_F(ArenaTest, TestMix) {
  TestCombo(10000, 20000);
}

// Make sure we work again after a clear
TEST_F(ArenaTest, TestReuse) {
  TestCombo(10000, 20000);
  ClearStats();
  TestCombo(20000, 10000);
}

// Tests for alignment helper.
TEST_F(ArenaTest, TestAlign) {
  // A few that work regardless of arch, to sanity-check
  // the more through loop below
  EXPECT_EQ(8, arena_.ExpandToAlign(8));
  EXPECT_EQ(16, arena_.ExpandToAlign(15));
  EXPECT_EQ(16, arena_.ExpandToAlign(14));
  EXPECT_EQ(16, arena_.ExpandToAlign(13));

  for (size_t t = 0; t < 1000u; ++t) {
    size_t expanded = arena_.ExpandToAlign(t);
    if ((t % arena_.kAlign) == 0) {
      // Well-aligned case.
      EXPECT_EQ(t, expanded);
    } else {
      // Otherwise should be next multiple
      EXPECT_EQ(((t / arena_.kAlign) + 1) * arena_.kAlign, expanded);
    }
  }
}

}  // namespace net_instaweb
