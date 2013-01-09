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

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SPLIT_WRITER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SPLIT_WRITER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class MessageHandler;

// SplitWriter writes to two sub-writers.
class SplitWriter : public Writer {
 public:
  SplitWriter(Writer* sub_writer1, Writer* sub_writer2)
      : writer1_(sub_writer1), writer2_(sub_writer2) {
  }

  virtual ~SplitWriter();

  virtual bool Write(const StringPiece& str, MessageHandler* handler) {
    bool ret = writer1_->Write(str, handler);
    ret &= writer2_->Write(str, handler);
    return ret;
  }

  virtual bool Flush(MessageHandler* handler) {
    bool ret = writer1_->Flush(handler);
    ret &= writer2_->Flush(handler);
    return ret;
  }

 private:
  Writer* writer1_;
  Writer* writer2_;

  DISALLOW_COPY_AND_ASSIGN(SplitWriter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SPLIT_WRITER_H_
