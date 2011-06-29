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
// Author: fangfei@google.com (Fangfei Zhou)

#include <cstdlib>
#include "net/instaweb/util/public/circular_buffer.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class CircularBufferTest : public testing::Test {
 protected:
  MockMessageHandler handler_;
};

// Instantiate CircularBuffer with malloc.
TEST_F(CircularBufferTest, InstantiateWithMalloc) {
  CircularBuffer* cb = CircularBuffer::Create(10);
  cb->Write("012345");
  EXPECT_EQ("012345", cb->ToString(&handler_));
  free(cb);
}

// Instantiate CircularBuffer with pre-allocated block of right size.
TEST_F(CircularBufferTest, InstantiateWithPreAllocatedBlock) {
  const int capacity = 10;
  const int segment_size = CircularBuffer::Sizeof(capacity);
  void* segment = static_cast<void*>(malloc(segment_size));
  CircularBuffer* cb = CircularBuffer::Init(true, segment,
                                            segment_size, capacity);
  if (cb != NULL) {
    cb->Write("0123456789");
  }
  EXPECT_EQ("0123456789", cb->ToString(&handler_));
  free(segment);
}

// Test circular buffer overwritten result.
TEST_F(CircularBufferTest, CircularWritten) {
  const int capacity = 10;
  CircularBuffer* cb = CircularBuffer::Create(capacity);
  cb->Write("012345");
  EXPECT_EQ("012345", cb->ToString(&handler_));
  cb->Write("67");
  EXPECT_EQ("01234567", cb->ToString(&handler_));
  // Buffer size is 10, it should be filled exactly so far.
  cb->Write("89");
  EXPECT_EQ("0123456789", cb->ToString(&handler_));
  // Lose the first char.
  cb->Write("a");
  EXPECT_EQ("123456789a", cb->ToString(&handler_));
  // Message size is larger than buffer size.
  cb->Write("bcdefghijkl");
  EXPECT_EQ("cdefghijkl", cb->ToString(&handler_));
  free(cb);
}

// Test the content after clear().
TEST_F(CircularBufferTest, OverWrittenAfterClear) {
  const int capacity = 10;
  CircularBuffer* cb = CircularBuffer::Create(capacity);
  cb->Write("0123456789");
  EXPECT_EQ("0123456789", cb->ToString(&handler_));
  cb->Clear();
  cb->Write("abc");
  EXPECT_EQ("abc", cb->ToString(&handler_));
  free(cb);
}

// Test corner case where buffer size is 1.
TEST_F(CircularBufferTest, SmallSize) {
  // CircularBuffer instantiated with malloc.
  const int capacity = 1;
  CircularBuffer* cb = CircularBuffer::Create(capacity);
  cb->Write("0");
  EXPECT_EQ("0", cb->ToString(&handler_));
  cb->Write("1");
  EXPECT_EQ("1", cb->ToString(&handler_));
  cb->Write("234");
  EXPECT_EQ("4", cb->ToString(&handler_));
  free(cb);
  // CircularBuffer instantiated with pre-allocated buffer.
  int segment_size = CircularBuffer::Sizeof(capacity);
  void* segment = static_cast<void*>(malloc(segment_size));
  CircularBuffer* temp = CircularBuffer::Init(true, segment,
                                              segment_size, capacity);
  temp->Write("0");
  EXPECT_EQ("0", temp->ToString(&handler_));
  temp->Write("1");
  EXPECT_EQ("1", temp->ToString(&handler_));
  temp->Write("234");
  EXPECT_EQ("4", temp->ToString(&handler_));
  free(temp);
}

}  // namespace net_instaweb
