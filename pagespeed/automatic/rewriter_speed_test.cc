/*
 * Copyright 2012 Google Inc.
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
//
//
// TODO(jmarantz): As it stands now the use of WgetUrlFetcher makes
// any speed-tests with resource rewriting meaningless, as it's not
// really async.  This test still makes sense for pure DOM-rewriting
// filters.  Later we can switch to the Serf fetcher and a real async
// flow.
//
// with --rewrite_level=PassThrough --rewriters=trim_urls I get:
//
// CPU: Intel Westmere with HyperThreading (3 cores) dL1:32KB dL2:256KB
// Benchmark                               Time(ns)    CPU(ns) Iterations
// ----------------------------------------------------------------------
// BM_ParseAndSerializeReuseParserX50   40979557   40900000        100
//
// Disclaimer: comparing runs over time and across different machines
// can be misleading.  When contemplating an algorithm change, always do
// interleaved runs with the old & new algorithm.

#include <algorithm>
#include <cstdlib>  // for exit
#include <memory>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/rewriter/public/process_context.h"
#include "strings/stringpiece_utils.h"
#include "pagespeed/automatic/static_rewriter.h"
#include "pagespeed/kernel/base/benchmark.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/null_writer.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

namespace {

// Lazily grab all the HTML text from testdata.  Note that we will
// never free this string but that's not considered a memory leak
// in Google because it's reachable from a static.
//
// TODO(jmarantz): this function is duplicated from
// net/instaweb/htmlparse/html_parse_speed_test.cc and should possibly
// be factored out.
GoogleString* sHtmlText = NULL;
ProcessContext* process_context = NULL;
const StringPiece GetHtmlText() {
  if (sHtmlText == NULL) {
    sHtmlText = new GoogleString;
    process_context = new ProcessContext;
    StdioFileSystem file_system;
    StringVector files;
    GoogleMessageHandler handler;
    static const char kDir[] = "net/instaweb/htmlparse/testdata";
    if (!file_system.ListContents(kDir, &files, &handler)) {
      LOG(ERROR) << "Unable to find test data for HTML benchmark, skipping";
      return StringPiece();
    }
    std::sort(files.begin(), files.end());
    for (int i = 0, n = files.size(); i < n; ++i) {
      GoogleString buffer;
      // Note that we do not want to include xmp_tag.html here as it
      // includes an unterminated <xmp> tag, so anything afterwards
      // will just get accumulated into that --- which was especially
      // noticeable in the X100 test.
      if (strings::EndsWith(StringPiece(files[i]), "xmp_tag.html")) {
        continue;
      }

      if (strings::EndsWith(StringPiece(files[i]), ".html")) {
        if (!file_system.ReadFile(files[i].c_str(), &buffer, &handler)) {
          LOG(ERROR) << "Unable to open:" << files[i];
          exit(1);
        }
      }
      StrAppend(sHtmlText, buffer);
    }
  }
  return *sHtmlText;
}

static void BM_ParseAndSerializeReuseParserX50(int iters) {
  StopBenchmarkTiming();
  StringPiece orig = GetHtmlText();
  if (orig.empty()) {
    return;
  }
  GoogleString text;
  text.reserve(50 * orig.size());
  // Repeat the text 50 times to get a ~1.5M file.
  for (int i = 0; i < 50; ++i) {
    StrAppend(&text, orig);
  }

  StaticRewriter rewriter(*process_context);
  StartBenchmarkTiming();
  for (int i = 0; i < iters; ++i) {
    NullWriter writer;
    rewriter.ParseText("http://example.com/benchmark", "benchmark", text,
                       "/tmp", &writer);
  }
}
BENCHMARK(BM_ParseAndSerializeReuseParserX50);

}  // namespace

}  // namespace net_instaweb
