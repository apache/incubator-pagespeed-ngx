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

// Unit-test RefCountedOwner.

#include "net/instaweb/util/public/ref_counted_owner.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

class NoteDeleteClass {
 public:
  explicit NoteDeleteClass(bool* mark_destroy) : mark_destroy_(mark_destroy) {}
  ~NoteDeleteClass() { *mark_destroy_ = true; }

 private:
  bool* mark_destroy_;
  DISALLOW_COPY_AND_ASSIGN(NoteDeleteClass);
};

class RefCountedOwnerTest : public testing::Test {
 protected:
  // Note: we pass in a non-const reference here simply because we also want
  // to cover the non-const versions of the getters.
  void CheckPointerOps(NoteDeleteClass* instance,
                       RefCountedOwner<NoteDeleteClass>& owner) {
    // First we ensure that pointer got fetches
    EXPECT_TRUE(owner.Attach());
    EXPECT_EQ(instance, owner.Get());
    const RefCountedOwner<NoteDeleteClass>& const_owner = owner;
    EXPECT_EQ(instance, const_owner.Get());
  }
};

TEST_F(RefCountedOwnerTest, Simple) {
  bool destroyed = false;
  {
    RefCountedOwner<NoteDeleteClass>::Family f1;
    RefCountedOwner<NoteDeleteClass> o1(&f1);
    RefCountedOwner<NoteDeleteClass> o2(&f1);
    EXPECT_FALSE(o1.Attach());
    EXPECT_FALSE(o2.Attach());
    EXPECT_FALSE(o1.Attach());  // didn't initialize yet ->
                                // nothing changed.
    EXPECT_FALSE(o2.Attach());

    NoteDeleteClass* instance = new NoteDeleteClass(&destroyed);
    o1.Initialize(instance);
    CheckPointerOps(instance, o1);
    CheckPointerOps(instance, o2);

    {
      RefCountedOwner<NoteDeleteClass> o3(&f1);
      CheckPointerOps(instance, o1);
    }

    EXPECT_FALSE(destroyed);
  }
  EXPECT_TRUE(destroyed);
}

// Test with more than one family.
TEST_F(RefCountedOwnerTest, MultipleFamilies) {
  bool destroyed1 = false;
  bool destroyed2 = false;
  {
    RefCountedOwner<NoteDeleteClass>::Family f1;
    RefCountedOwner<NoteDeleteClass>::Family f2;

    RefCountedOwner<NoteDeleteClass> o1(&f1);
    RefCountedOwner<NoteDeleteClass> o2(&f1);
    EXPECT_FALSE(o1.Attach());
    EXPECT_FALSE(o2.Attach());

    NoteDeleteClass* instance1 = new NoteDeleteClass(&destroyed1);
    o1.Initialize(instance1);
    CheckPointerOps(instance1, o1);
    CheckPointerOps(instance1, o2);

    {
      RefCountedOwner<NoteDeleteClass> o3(&f2);
      EXPECT_FALSE(o3.Attach());
      NoteDeleteClass* instance2 = new NoteDeleteClass(&destroyed2);
      o3.Initialize(instance2);
      CheckPointerOps(instance1, o1);
      CheckPointerOps(instance1, o2);
      CheckPointerOps(instance2, o3);
    }

    EXPECT_FALSE(destroyed1);
    EXPECT_TRUE(destroyed2);
  }
  EXPECT_TRUE(destroyed1);
  EXPECT_TRUE(destroyed2);
}

}  // namespace

}  // namespace net_instaweb
