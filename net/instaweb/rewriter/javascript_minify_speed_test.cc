/*
 * Copyright 2014 Google Inc.
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
//
//
// CPU: Intel Nehalem with HyperThreading (4 cores) dL1:32KB dL2:256KB
// Benchmark                     Time(ns)    CPU(ns) Iterations
// ------------------------------------------------------------
// BM_MinifyJavascriptNew/64         3862       3870     178281
// BM_MinifyJavascriptNew/512       29962      30058      24922
// BM_MinifyJavascriptNew/4k       163436     163944       4218
// BM_MinifyJavascriptNew/32k     1370666    1374490        494
// BM_MinifyJavascriptNew/256k   11499929   11532620        100
// BM_MinifyJavascriptOld/64         1182       1185     571793
// BM_MinifyJavascriptOld/512       10234      10270      65585
// BM_MinifyJavascriptOld/4k        65045      65232      10000
// BM_MinifyJavascriptOld/32k      666505     669240       1000
// BM_MinifyJavascriptOld/256k    4989183    5005530        100
//
// Disclaimer: comparing runs over time and across different machines
// can be misleading.  When contemplating an algorithm change, always do
// interleaved runs with the old & new algorithm.

#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/javascript_library_identification.h"
#include "pagespeed/kernel/base/benchmark.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/null_statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/js/js_tokenizer.h"

namespace net_instaweb {

extern const char* JS_console_js;

namespace {

void TestMinifyJavascript(bool use_experimental_minifier, int iters, int size) {
  GoogleString in_text;
  for (int i = 0; i < size; i += strlen(JS_console_js)) {
    in_text += JS_console_js;
  }
  in_text.resize(size);

  NullStatistics stats;
  JavascriptRewriteConfig::InitStats(&stats);
  pagespeed::js::JsTokenizerPatterns js_tokenizer_patterns;
  JavascriptLibraryIdentification js_lib_id;
  JavascriptRewriteConfig config(&stats, true /* minify */,
                                 use_experimental_minifier,
                                 &js_lib_id, &js_tokenizer_patterns);

  NullMessageHandler handler;
  for (int i = 0; i < iters; ++i) {
    JavascriptCodeBlock block(in_text, &config, "" /* message_id */, &handler);
    block.Rewrite();
  }
}

static void BM_MinifyJavascriptNew(int iters, int size) {
  TestMinifyJavascript(true, iters, size);
}
BENCHMARK_RANGE(BM_MinifyJavascriptNew, 1<<6, 1<<18);

static void BM_MinifyJavascriptOld(int iters, int size) {
  TestMinifyJavascript(false, iters, size);
}
BENCHMARK_RANGE(BM_MinifyJavascriptOld, 1<<6, 1<<18);

}  // namespace

}  // namespace net_instaweb
