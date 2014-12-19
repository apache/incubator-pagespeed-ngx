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

// Author: morlovich@google.com (Maksim Orlovich)

#ifndef PAGESPEED_KERNEL_BASE_MOCK_MESSAGE_HANDLER_H_
#define PAGESPEED_KERNEL_BASE_MOCK_MESSAGE_HANDLER_H_

#include <map>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/fast_wildcard_group.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class Writer;

// A version of GoogleMessageHandler to use in testcases that keeps
// track of number of messages output, to validate diagnostics
class MockMessageHandler : public MessageHandler {
 public:
  // Takes ownership of the mutex.
  explicit MockMessageHandler(AbstractMutex* mutex);

  virtual ~MockMessageHandler();

  // Returns number of messages of given type issued
  int MessagesOfType(MessageType type) const;

  // Returns total number of messages issued
  int TotalMessages() const;

  // Returns number of messages which are not printed
  int SkippedMessagesOfType(MessageType type) const;

  // Returns total number of messages which are not printed
  int TotalSkippedMessages() const;

  // Returns number of messages of severity higher than info
  int SeriousMessages() const;

  // Takes ownership of the mutex.
  void set_mutex(AbstractMutex* mutex);

  // If a message contains any of the added patterns (sub-strings),
  // it will not be printed, but will still be counted.
  void AddPatternToSkipPrinting(const char* pattern);

  // Dumps contents of String Buffer.
  virtual bool Dump(Writer* writer);

 protected:
  virtual void MessageSImpl(MessageType type, const GoogleString& message);

  virtual void FileMessageSImpl(MessageType type, const char* filename,
                                int line, const GoogleString& message);

 private:
  // Returns whether the message should be printed.
  bool ShouldPrintMessage(const StringPiece& msg);

 private:
  typedef std::map<MessageType, int> MessageCountMap;

  // The Impl versions don't grab the lock themselves
  int TotalMessagesImpl(const MessageCountMap& counts) const;
  int MessagesOfTypeImpl(const MessageCountMap& counts,
                         MessageType type) const;

  scoped_ptr<AbstractMutex> mutex_;
  MessageCountMap message_counts_;
  MessageCountMap skipped_message_counts_;
  FastWildcardGroup patterns_to_skip_;
  GoogleString buffer_;
  // This handler is only for internal use in Dump method.
  GoogleMessageHandler internal_handler_;

  DISALLOW_COPY_AND_ASSIGN(MockMessageHandler);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_MOCK_MESSAGE_HANDLER_H_
