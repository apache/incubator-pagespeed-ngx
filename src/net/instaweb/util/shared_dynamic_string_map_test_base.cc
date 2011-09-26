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
// Author: jhoch@google.com (Jason Hoch)

#include "net/instaweb/util/public/shared_dynamic_string_map_test_base.h"

#include <cmath>
#include <cstdlib>
#include "base/logging.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/shared_dynamic_string_map.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

const int kIntSize = sizeof(int); // NOLINT
// Should be a multiple of 4
const int kTableSize = 1024;
// +1 for string that causes overflow
const int kNumberOfStrings = kTableSize + 1;
const int kStringSize = 64;
const char kPrefix[] = "/prefix/";
const char kSuffix[] = "suffix";
const char kExampleString1[] = "http://www.example1.com";
const char kExampleString2[] = "http://www.example2.com";

}  // namespace

SharedDynamicStringMapTestBase::SharedDynamicStringMapTestBase(
    SharedMemTestEnv* test_env)
    : test_env_(test_env),
      shmem_runtime_(test_env->CreateSharedMemRuntime()) {
  // We must be able to fit a unique int in our string
  CHECK(2 * kIntSize < kStringSize - 1);
  // 255 because we can't use null char
  CHECK(kNumberOfStrings < pow(16, 2 * kIntSize));
  // After the int at the front we add random chars
  for (int i = 0; i < kNumberOfStrings; i++) {
    // We pad the beginning with the hex representation of i, a unique string
    // or non-null characters
    GoogleString string = StringPrintf("%0*x", 2 * kIntSize, i);
    // We fill the rest of the string with random lower-case letters
    // -1 so there's room for the terminating null character
    while (string.length() < kStringSize - 1) {
      string.push_back(random() % 26 + 'a');
    }
    strings_.push_back(string);
  }
}

bool SharedDynamicStringMapTestBase::CreateChild(TestMethod0 method) {
  Function* callback =
      new MemberFunction0<SharedDynamicStringMapTestBase>(method, this);
  return test_env_->CreateChild(callback);
}

bool SharedDynamicStringMapTestBase::CreateFillChild(TestMethod2 method,
                                                     int start,
                                                     int number_of_strings) {
  Function* callback =
      new MemberFunction2<SharedDynamicStringMapTestBase, int, int>(
          method, this, start, number_of_strings);
  return test_env_->CreateChild(callback);
}

SharedDynamicStringMap* SharedDynamicStringMapTestBase::ChildInit() {
  SharedDynamicStringMap* map =
      new SharedDynamicStringMap(kTableSize,
                                 kStringSize,
                                 shmem_runtime_.get(),
                                 kPrefix,
                                 kSuffix);
  map->InitSegment(false, &handler_);
  return map;
}

SharedDynamicStringMap* SharedDynamicStringMapTestBase::ParentInit() {
  SharedDynamicStringMap* map =
      new SharedDynamicStringMap(kTableSize,
                                 kStringSize,
                                 shmem_runtime_.get(),
                                 kPrefix,
                                 kSuffix);
  map->InitSegment(true, &handler_);
  return map;
}

void SharedDynamicStringMapTestBase::TestSimple() {
  scoped_ptr<SharedDynamicStringMap> map(ParentInit());
  GoogleString output;
  StringWriter writer(&output);
  map->Dump(&writer, &handler_);
  EXPECT_EQ(output, "");
  EXPECT_EQ(0, map->GetNumberInserted());
  map->IncrementElement(kExampleString1);
  EXPECT_EQ(1, map->LookupElement(kExampleString1));
  output.clear();
  map->Dump(&writer, &handler_);
  EXPECT_EQ(output, "http://www.example1.com: 1\n");
  EXPECT_EQ(1, map->GetNumberInserted());
  map->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedDynamicStringMapTestBase::TestCreate() {
  scoped_ptr<SharedDynamicStringMap> map(ParentInit());
  EXPECT_EQ(0, map->LookupElement(kExampleString1));
  EXPECT_EQ(0, map->LookupElement(kExampleString2));
  EXPECT_EQ(0, map->GetNumberInserted());
  map->IncrementElement(kExampleString1);
  map->IncrementElement(kExampleString2);
  EXPECT_EQ(1, map->LookupElement(kExampleString1));
  EXPECT_EQ(1, map->LookupElement(kExampleString2));
  EXPECT_EQ(2, map->GetNumberInserted());
  ASSERT_TRUE(CreateChild(&SharedDynamicStringMapTestBase::AddChild));
  test_env_->WaitForChildren();
  EXPECT_EQ(2, map->LookupElement(kExampleString1));
  EXPECT_EQ(2, map->LookupElement(kExampleString2));
  EXPECT_EQ(2, map->GetNumberInserted());
  map->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedDynamicStringMapTestBase::AddChild() {
  scoped_ptr<SharedDynamicStringMap> map(ChildInit());
  if ((map->IncrementElement(kExampleString1) == 0) ||
      (map->IncrementElement(kExampleString2) == 0)) {
    test_env_->ChildFailed();
  }
}

void SharedDynamicStringMapTestBase::TestAdd() {
  scoped_ptr<SharedDynamicStringMap> map(ParentInit());
  for (int i = 0; i < 2; i++)
    ASSERT_TRUE(CreateChild(&SharedDynamicStringMapTestBase::AddChild));
  test_env_->WaitForChildren();
  EXPECT_EQ(2, map->LookupElement(kExampleString1));
  EXPECT_EQ(2, map->LookupElement(kExampleString2));
  EXPECT_EQ(2, map->GetNumberInserted());
  map->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedDynamicStringMapTestBase::TestQuarterFull() {
  scoped_ptr<SharedDynamicStringMap> map(ParentInit());
  ASSERT_TRUE(CreateFillChild(&SharedDynamicStringMapTestBase::AddFillChild,
                              0,
                              kTableSize / 4));
  test_env_->WaitForChildren();
  EXPECT_EQ(kTableSize / 4, map->GetNumberInserted());
  GoogleString output;
  StringWriter writer(&output);
  map->Dump(&writer, &handler_);
  // Dump outputs the table data in the form
  // "<string1>: <value1>\n<string2>: <value2>\n<string3>: <value3>\n..."
  // In this case all values should be 1 so for each of the (kTableSize / 4)
  // strings there should be kStringSize characters plus a ":", " ", "1", and
  // "\n" and minus a null character; hence (kTablsize / 4) * (kStringSize + 3)
  EXPECT_EQ((kTableSize / 4) * (kStringSize + 3), output.length());
  map->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedDynamicStringMapTestBase::TestFillSingleThread() {
  scoped_ptr<SharedDynamicStringMap> map(ParentInit());
  EXPECT_EQ(0, map->GetNumberInserted());
  // One child fills the entire table.
  ASSERT_TRUE(CreateFillChild(&SharedDynamicStringMapTestBase::AddFillChild,
                              0,
                              kTableSize));
  test_env_->WaitForChildren();
  // Each entry should have been incremented once.
  for (int i = 0; i < kTableSize; i++)
    EXPECT_EQ(1, map->LookupElement(strings_[i]));
  EXPECT_EQ(kTableSize, map->GetNumberInserted());
  // One child increments the entire table.
  ASSERT_TRUE(CreateFillChild(&SharedDynamicStringMapTestBase::AddFillChild,
                              0,
                              kTableSize));
  test_env_->WaitForChildren();
  // Each entry should have been incremented twice.
  for (int i = 0; i < kTableSize; i++)
    EXPECT_EQ(2, map->LookupElement(strings_[i]));
  EXPECT_EQ(kTableSize, map->GetNumberInserted());
  // Once the table is full it should not accept additional strings.
  ASSERT_TRUE(CreateChild(&SharedDynamicStringMapTestBase::AddToFullTable));
  test_env_->WaitForChildren();
  EXPECT_EQ(kTableSize, map->GetNumberInserted());
  map->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedDynamicStringMapTestBase::TestFillMultipleNonOverlappingThreads() {
  scoped_ptr<SharedDynamicStringMap> map(ParentInit());
  CHECK_EQ(kTableSize % 4, 0);
  // Each child will fill up 1/4 of the table.
  for (int i = 0; i < 4; i++)
    ASSERT_TRUE(CreateFillChild(&SharedDynamicStringMapTestBase::AddFillChild,
                                i * kTableSize / 4,
                                kTableSize / 4));
  test_env_->WaitForChildren();
  for (int i = 0; i < kTableSize; i++)
    EXPECT_EQ(1, map->LookupElement(strings_[i]));
  EXPECT_EQ(kTableSize, map->GetNumberInserted());
  // Once the table is full it should not accept additional strings.
  ASSERT_TRUE(CreateChild(&SharedDynamicStringMapTestBase::AddToFullTable));
  EXPECT_EQ(kTableSize, map->GetNumberInserted());
  test_env_->WaitForChildren();
  map->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedDynamicStringMapTestBase::TestFillMultipleOverlappingThreads() {
  scoped_ptr<SharedDynamicStringMap> map(ParentInit());
  // Ensure that kTableSize is a multiple of 4.
  CHECK_EQ(kTableSize & 3, 0);
  // Each child will fill up 1/2 of the table - the table will get covered
  // twice.
  for (int i = 0; i < 4; i++)
    ASSERT_TRUE(CreateFillChild(&SharedDynamicStringMapTestBase::AddFillChild,
                                i * kTableSize / 4,
                                kTableSize / 2));
  // In addition, the parent is going to fill up the entire table.
  for (int i = 0; i < kTableSize; i++)
    ASSERT_NE(0, map->IncrementElement(strings_[i]));
  test_env_->WaitForChildren();
  EXPECT_EQ(kTableSize, map->GetNumberInserted());
  // Hence, we check that the values are equal to 3.
  for (int i = 0; i < kTableSize; i++)
    EXPECT_EQ(3, map->LookupElement(strings_[i]));
  // Once the table is full it should not accept additional strings.
  ASSERT_TRUE(CreateChild(&SharedDynamicStringMapTestBase::AddToFullTable));
  test_env_->WaitForChildren();
  EXPECT_EQ(kTableSize, map->GetNumberInserted());
  map->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedDynamicStringMapTestBase::AddFillChild(int start,
                                                  int number_of_strings) {
  scoped_ptr<SharedDynamicStringMap> map(ChildInit());
  for (int i = 0; i < number_of_strings; i++) {
    if (0 == map->IncrementElement(strings_[(i + start) % kTableSize]))
      test_env_->ChildFailed();
  }
}

void SharedDynamicStringMapTestBase::AddToFullTable() {
  scoped_ptr<SharedDynamicStringMap> map(ChildInit());
  const char* string = strings_[kTableSize].c_str();
  EXPECT_EQ(0, map->IncrementElement(string));
}

}  // namespace net_instaweb
