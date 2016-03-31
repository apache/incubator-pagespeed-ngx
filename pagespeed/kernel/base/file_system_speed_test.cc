/*
 * Copyright 2015 Google Inc.
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

#include "base/logging.h"
#include "pagespeed/kernel/base/benchmark.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"

// Running the speed test:
//   src/out/Release/mod_pagespeed_speed_test .File
//   BM_100kWholeFile             100000             18845 ns/op
//   BM_100kStreamingFile          50000             70181 ns/op
//   BM_1MWholeFile                10000            122070 ns/op
//   BM_1MStreamingFile             2000            760416 ns/op
//
//
// Disclaimer: comparing runs over time and across different machines
// can be misleading.  When contemplating an algorithm change, always do
// interleaved runs with the old & new algorithm.

namespace net_instaweb {

namespace {

class FSTester {
 public:
  explicit FSTester(int size) {
    StopBenchmarkTiming();
    GoogleString str;
    str.append(size, 'a');
    filename_ = StrCat(GTestTempDir(), "large_file.txt");
    file_system_.WriteFile(filename_.c_str(), str, &handler_);
  }

  ~FSTester() {
    file_system_.RemoveFile(filename_.c_str(), &handler_);
    StartBenchmarkTiming();
  }

  void ReadWholeFile(int iters) {
    StartBenchmarkTiming();
    for (int i = 0; i < iters; ++i) {
      GoogleString buf;
      CHECK(file_system_.ReadFile(filename_.c_str(), &buf, &handler_));
    }
    StopBenchmarkTiming();
  }

  void StreamingReadFile(int iters) {
    StartBenchmarkTiming();
    for (int i = 0; i < iters; ++i) {
      GoogleString buf;
      StringWriter writer(&buf);
      CHECK(file_system_.ReadFile(filename_.c_str(), &writer, &handler_));
    }
    StopBenchmarkTiming();
  }

 private:
  StdioFileSystem file_system_;
  GoogleString filename_;
  GoogleMessageHandler handler_;
};

static void BM_100kWholeFile(int iters) {
  FSTester fs_tester(100000);
  fs_tester.ReadWholeFile(iters);
}
BENCHMARK(BM_100kWholeFile);

static void BM_100kStreamingFile(int iters) {
  FSTester fs_tester(100000);
  fs_tester.StreamingReadFile(iters);
}
BENCHMARK(BM_100kStreamingFile);

static void BM_1MWholeFile(int iters) {
  FSTester fs_tester(1000000);
  fs_tester.ReadWholeFile(iters);
}
BENCHMARK(BM_1MWholeFile);

static void BM_1MStreamingFile(int iters) {
  FSTester fs_tester(1000000);
  fs_tester.StreamingReadFile(iters);
}
BENCHMARK(BM_1MStreamingFile);

}  // namespace

}  // namespace net_instaweb
