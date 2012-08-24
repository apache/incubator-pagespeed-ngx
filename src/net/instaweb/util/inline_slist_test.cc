// Copyright 2012 Google Inc.
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
// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/util/public/inline_slist.h"

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

class IntElement : public InlineSListElement<IntElement> {
 public:
  explicit IntElement(int n) : num_(n) { }

  const int num() const { return num_; }
  void set_num(int num) { num_ = num; }

 private:
  int num_;

  DISALLOW_COPY_AND_ASSIGN(IntElement);
};

typedef InlineSList<IntElement> IntList;

class InlineSListTest : public testing::Test {
 protected:
  // Dump the list value. Assumes the list only contains small numbers,
  // but is thorough in checking various iteration interfaces, as well
  // as ,->, Get() and *.
  GoogleString Dump() {
    GoogleString dump1, dump2, dump3, dump4, dump5, dump6;

    for (IntList::iterator i = ints_.begin(); i != ints_.end(); ++i) {
      dump1.push_back(i->num() + '0');
    }

    for (IntList::iterator i = ints_.begin(); i != ints_.end(); ++i) {
      dump2.push_back((*i).num() + '0');
    }

    for (IntList::iterator i = ints_.begin(); i != ints_.end(); ++i) {
      dump3.push_back(i.Get()->num() + '0');
    }

    const IntList& cints = const_cast<const IntList&>(ints_);
    for (IntList::const_iterator i = cints.begin(); i != cints.end(); ++i) {
      dump4.push_back(i->num() + '0');
    }

    for (IntList::const_iterator i = cints.begin(); i != cints.end(); ++i) {
      dump5.push_back((*i).num() + '0');
    }

    for (IntList::const_iterator i = cints.begin(); i != cints.end(); ++i) {
      dump6.push_back(i.Get()->num() + '0');
    }

    EXPECT_EQ(dump1, dump2);
    EXPECT_EQ(dump1, dump3);
    EXPECT_EQ(dump1, dump4);
    EXPECT_EQ(dump1, dump5);
    EXPECT_EQ(dump1, dump6);
    return dump1;
  }

  IntList ints_;
};

TEST_F(InlineSListTest, BasicOperation) {
  EXPECT_TRUE(ints_.IsEmpty());
  EXPECT_STREQ("", Dump());

  ints_.Append(new IntElement(0));
  EXPECT_FALSE(ints_.IsEmpty());
  EXPECT_STREQ("0", Dump());

  ints_.Append(new IntElement(1));
  EXPECT_FALSE(ints_.IsEmpty());
  EXPECT_STREQ("01", Dump());

  ints_.Append(new IntElement(2));
  EXPECT_FALSE(ints_.IsEmpty());
  EXPECT_STREQ("012", Dump());
}

TEST_F(InlineSListTest, DestructEmpty) {
  // Make sure ~IntList works with no elements element.
}

TEST_F(InlineSListTest, Destruct1) {
  // Make sure ~IntList works with 1 element.
  ints_.Append(new IntElement(0));
}

TEST_F(InlineSListTest, Remove1) {
  // Remove the sole item in 1-entry list.
  ints_.Append(new IntElement(0));
  EXPECT_EQ(0, ints_.Last()->num());
  IntList::iterator iter(ints_.begin());
  EXPECT_NE(ints_.end(), iter);
  ints_.Erase(&iter);
  EXPECT_EQ(ints_.end(), iter);
  EXPECT_TRUE(ints_.IsEmpty());
  EXPECT_STREQ("", Dump());
}

TEST_F(InlineSListTest, RemoveLast) {
  // Remove last item of 0,1 list.
  ints_.Append(new IntElement(0));
  ints_.Append(new IntElement(1));
  EXPECT_EQ(1, ints_.Last()->num());

  IntList::iterator iter(ints_.begin());
  EXPECT_NE(ints_.end(), iter);
  EXPECT_EQ(0, iter->num());

  ++iter;
  EXPECT_NE(ints_.end(), iter);
  EXPECT_EQ(1, iter->num());

  ints_.Erase(&iter);
  EXPECT_EQ(ints_.end(), iter);
  EXPECT_STREQ("0", Dump());
  EXPECT_EQ(0, ints_.Last()->num());
}

TEST_F(InlineSListTest, RemoveFirst) {
  // Remove first item of 0,1 list.
  ints_.Append(new IntElement(0));
  ints_.Append(new IntElement(1));

  IntList::iterator iter(ints_.begin());
  EXPECT_NE(ints_.end(), iter);
  EXPECT_EQ(0, iter->num());

  ints_.Erase(&iter);
  EXPECT_NE(ints_.end(), iter);
  EXPECT_EQ(1, iter->num());

  ++iter;
  EXPECT_EQ(ints_.end(), iter);
  EXPECT_STREQ("1", Dump());
}

TEST_F(InlineSListTest, RemoveOdd) {
  for (int i = 0; i < 10; ++i) {
    ints_.Append(new IntElement(i));
  }
  EXPECT_STREQ("0123456789", Dump());

  IntList::iterator iter(ints_.begin());
  while (iter != ints_.end()) {
    if ((iter->num() % 2) == 1) {
      ints_.Erase(&iter);
    } else {
      ++iter;
    }
  }
  EXPECT_STREQ("02468", Dump());
}

}  // namespace

}  // namespace net_instaweb
