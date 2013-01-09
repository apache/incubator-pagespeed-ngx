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

#ifndef MOD_SPDY_COMMON_SPDY_STREAM_TASK_FACTORY_H_
#define MOD_SPDY_COMMON_SPDY_STREAM_TASK_FACTORY_H_

#include "base/basictypes.h"

namespace net_instaweb { class Function; }

namespace mod_spdy {

class SpdyStream;

// SpdyStreamTaskFactory is a helper interface for the SpdySession class.
// The task factory generates tasks that take care of processing SPDY streams.
// The task factory must not be deleted before all such tasks have been
// disposed of (run or cancelled).
class SpdyStreamTaskFactory {
 public:
  SpdyStreamTaskFactory();
  virtual ~SpdyStreamTaskFactory();

  // Create a new task to process the given stream.  Running the task should
  // process the stream -- that is, pull frames off the stream's input queue
  // and post frames to the stream's output queue -- and the task should not
  // complete until the stream is completely finished.
  //
  // The implementation may assume that the factory will outlive the task.
  virtual net_instaweb::Function* NewStreamTask(SpdyStream* stream) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyStreamTaskFactory);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_SPDY_STREAM_TASK_FACTORY_H_
