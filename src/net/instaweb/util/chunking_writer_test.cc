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

#include "net/instaweb/util/public/chunking_writer.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {
class MessageHandler;

namespace {

// Records a traces of writes and flushes performed into recorded()
// as follows:
// 1) Write of "text" will append: W:text|
// 2) A flush will append: F|
//
// Also makes sure the passed in handler is correct, and let's one
// trigger failures on a given operation.
class TracingWriter : public Writer {
 public:
  TracingWriter(MessageHandler* expected_handler) :
      expected_handler_(expected_handler), ops_(0), fail_on_op_(-1) {
  }

  virtual bool Write(const StringPiece& str, MessageHandler* handler) {
    EXPECT_EQ(expected_handler_, handler);
    if (ops_ == fail_on_op_) {
      // Still advance, so that we know we don't get called again
      ++ops_;
      return false;
    }
    ++ops_;

    recorded_.append("W:");
    recorded_.append(str.data(), str.size());
    recorded_.append("|");
    return true;
  }

  virtual bool Flush(MessageHandler* handler) {
    EXPECT_EQ(expected_handler_, handler);
    if (ops_ == fail_on_op_) {
      // Still advance, so that we know we don't get called again
      ++ops_;
      return false;
    }
    ++ops_;

    recorded_.append("F|");
    return true;
  }

  const GoogleString& recorded() const { return recorded_; }

  // Tells this filter to report a failure on n'th invocation exactly.
  // (starting from 0)
  void set_fail_on_op(int n) { fail_on_op_ = n; }

 private:
  MessageHandler* expected_handler_;
  GoogleString recorded_;
  int ops_;
  int fail_on_op_;
};


class ChunkingWriterTest : public testing::Test {
 public:
  virtual void SetUp() {
    tracer_.reset(new TracingWriter(&message_handler_));
    SetUpWithLimit(0);
  }

  void SetUpWithLimit(int limit) {
    chunker_.reset(new ChunkingWriter(tracer_.get(), limit));
  }

 protected:
  MockMessageHandler message_handler_;
  scoped_ptr<TracingWriter> tracer_;
  scoped_ptr<ChunkingWriter> chunker_;
};

TEST_F(ChunkingWriterTest, UnchunkedBasic) {
  EXPECT_TRUE(chunker_->Write("abc", &message_handler_));
  EXPECT_TRUE(chunker_->Write("def", &message_handler_));
  EXPECT_TRUE(chunker_->Flush(&message_handler_));
  EXPECT_EQ("W:abc|W:def|F|", tracer_->recorded());
}

TEST_F(ChunkingWriterTest, ChunkedBasic) {
  SetUpWithLimit(2);
  EXPECT_TRUE(chunker_->Write("abc", &message_handler_));
  EXPECT_TRUE(chunker_->Write("def", &message_handler_));
  EXPECT_TRUE(chunker_->Flush(&message_handler_));
  EXPECT_EQ("W:ab|F|W:c|W:d|F|W:ef|F|F|", tracer_->recorded());
}

TEST_F(ChunkingWriterTest, ChunkedBasicLong) {
  SetUpWithLimit(4);
  EXPECT_TRUE(chunker_->Write("abcdefghijklmnopqrs", &message_handler_));
  EXPECT_TRUE(chunker_->Flush(&message_handler_));
  EXPECT_EQ("W:abcd|F|W:efgh|F|W:ijkl|F|W:mnop|F|W:qrs|F|", tracer_->recorded());
}

TEST_F(ChunkingWriterTest, ChunkedManualFlush) {
  SetUpWithLimit(4);
  EXPECT_TRUE(chunker_->Write("abc", &message_handler_));
  EXPECT_TRUE(chunker_->Flush(&message_handler_));
  EXPECT_TRUE(chunker_->Write("defgh", &message_handler_));
  EXPECT_EQ("W:abc|F|W:defg|F|W:h|", tracer_->recorded());
}

TEST_F(ChunkingWriterTest, UnchunkedFailureProp1) {
  tracer_->set_fail_on_op(1);
  EXPECT_TRUE(chunker_->Write("abc", &message_handler_));
  EXPECT_FALSE(chunker_->Write("def", &message_handler_));
  EXPECT_EQ("W:abc|", tracer_->recorded());
}

TEST_F(ChunkingWriterTest, UnchunkedFailureProp2) {
  tracer_->set_fail_on_op(2);
  EXPECT_TRUE(chunker_->Write("abc", &message_handler_));
  EXPECT_TRUE(chunker_->Write("def", &message_handler_));
  EXPECT_FALSE(chunker_->Flush(&message_handler_));
  EXPECT_EQ("W:abc|W:def|", tracer_->recorded());
}

TEST_F(ChunkingWriterTest, ChunkedFailureProp1) {
  tracer_->set_fail_on_op(1);
  SetUpWithLimit(4);
  EXPECT_FALSE(chunker_->Write("abcdefgh", &message_handler_));
  EXPECT_EQ("W:abcd|", tracer_->recorded());
}

TEST_F(ChunkingWriterTest, ChunkedFailureProp2) {
  tracer_->set_fail_on_op(2);
  SetUpWithLimit(4);
  EXPECT_FALSE(chunker_->Write("abcdefgh", &message_handler_));
  EXPECT_EQ("W:abcd|F|", tracer_->recorded());
}

TEST_F(ChunkingWriterTest, ChunkedFailureProp3) {
  tracer_->set_fail_on_op(3);
  SetUpWithLimit(4);
  EXPECT_FALSE(chunker_->Write("abcdefgh", &message_handler_));
  EXPECT_EQ("W:abcd|F|W:efgh|", tracer_->recorded());
}

TEST_F(ChunkingWriterTest, ChunkedFailureProp4) {
  tracer_->set_fail_on_op(4);
  SetUpWithLimit(4);
  EXPECT_TRUE(chunker_->Write("abcdefgh", &message_handler_));
  EXPECT_EQ("W:abcd|F|W:efgh|F|", tracer_->recorded());
}

}  // namespace

}  // namespace net_instaweb
