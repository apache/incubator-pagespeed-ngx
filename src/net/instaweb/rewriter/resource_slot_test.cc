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

// Unit-test the resource slot comparator.

#include "net/instaweb/rewriter/public/resource_slot.h"

#include <set>
#include <utility>  // for std::pair
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"

namespace net_instaweb {

class ResourceSlotTest : public HtmlParseTestBase {
 protected:
  typedef std::set<HtmlResourceSlotPtr, HtmlResourceSlotComparator> SlotSet;

  virtual bool AddBody() const { return false; }

  virtual void SetUp() {
    HtmlParseTestBase::SetUp();

    // Set up 4 slots for testing.
    elements_[0] = html_parse_.NewElement(NULL, HtmlName::kLink);
    html_parse_.AddAttribute(elements_[0], HtmlName::kHref, "v1");
    html_parse_.AddAttribute(elements_[0], HtmlName::kSrc, "v2");
    elements_[1] = html_parse_.NewElement(NULL, HtmlName::kLink);
    html_parse_.AddAttribute(element(1), HtmlName::kHref, "v1");
    html_parse_.AddAttribute(element(1), HtmlName::kSrc, "v2");

    slots_[0] = MakeSlot(0, 0);
    slots_[1] = MakeSlot(0, 1);
    slots_[2] = MakeSlot(1, 0);
    slots_[3] = MakeSlot(1, 1);
  }

  HtmlResourceSlotPtr MakeSlot(int element_index, int attribute_index) {
    ResourcePtr empty;
    HtmlResourceSlot* slot = new HtmlResourceSlot(
        empty, element(element_index),
        attribute(element_index, attribute_index));
    return HtmlResourceSlotPtr(slot);
  }

  bool InsertAndReturnTrueIfAdded(const HtmlResourceSlotPtr& slot) {
    std::pair<HtmlResourceSlotSet::iterator, bool> p = slot_set_.insert(slot);
    return p.second;
  }

  int num_slots() const { return slot_set_.size(); }
  const HtmlResourceSlotPtr slot(int index) const { return slots_[index]; }
  HtmlElement* element(int index) { return elements_[index]; }
  HtmlElement::Attribute* attribute(int element_index, int attribute_index) {
    return &element(element_index)->attribute(attribute_index);
  }

 private:
  HtmlResourceSlotSet slot_set_;
  HtmlResourceSlotPtr slots_[4];
  HtmlElement* elements_[2];
};

TEST_F(ResourceSlotTest, Accessors) {
  EXPECT_EQ(element(0), slot(0)->element());
  EXPECT_EQ(&element(0)->attribute(0), slot(0)->attribute());
  EXPECT_EQ(element(0), slot(1)->element());
  EXPECT_EQ(&element(0)->attribute(1), slot(1)->attribute());
  EXPECT_EQ(element(1), slot(2)->element());
  EXPECT_EQ(&element(1)->attribute(0), slot(2)->attribute());
  EXPECT_EQ(element(1), slot(3)->element());
  EXPECT_EQ(&element(1)->attribute(1), slot(3)->attribute());
}

TEST_F(ResourceSlotTest, Comparator) {
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(InsertAndReturnTrueIfAdded(slot(i)));
  }
  EXPECT_EQ(4, num_slots());

  // Adding an equivalent slot should fail and leave the number of remembered
  // slots unchanged.
  ResourcePtr empty;
  HtmlResourceSlotPtr s4_dup(MakeSlot(1, 1));
  EXPECT_FALSE(InsertAndReturnTrueIfAdded(s4_dup))
      << "s4_dup is equivalent to slots_[3] so it should not add to the set";
  EXPECT_EQ(4, num_slots());
}

}  // namespace net_instaweb
