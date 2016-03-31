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

// Author: sligocki@google.com (Shawn Ligocki)
//
//
// CPU: Intel Nehalem with HyperThreading (4 cores) dL1:32KB dL2:256KB
// Benchmark                         Time(ns)    CPU(ns) Iterations
// ----------------------------------------------------------------
// BM_MinifyCss/64                        747        749    1000000
// BM_MinifyCss/512                      1292       1297     520318
// BM_MinifyCss/4k                     114173     114521       6107
// BM_MinifyCss/32k                    983916     987433        709
// BM_MinifyCss/256k                  8443277    8479080        100
// BM_EscapeStringNormal/1                 33         33   21614011
// BM_EscapeStringNormal/8                108        109    6519694
// BM_EscapeStringNormal/64               566        568    1000000
// BM_EscapeStringNormal/512             3572       3583     196299
// BM_EscapeStringNormal/4k             28471      28582      23328
// BM_EscapeStringSpecial/1                41         41   16962254
// BM_EscapeStringSpecial/8               265        265    2593371
// BM_EscapeStringSpecial/64             1287       1292     554701
// BM_EscapeStringSpecial/512            9719       9756      71318
// BM_EscapeStringSpecial/4k            75572      75791       9101
// BM_EscapeStringSuperSpecial/1           47         47   15656068
// BM_EscapeStringSuperSpecial/8          308        309    2304694
// BM_EscapeStringSuperSpecial/64        1941       1947     361238
// BM_EscapeStringSuperSpecial/512      13333      13375      51935
// BM_EscapeStringSuperSpecial/4k      105527     105909       6768
//
// Disclaimer: comparing runs over time and across different machines
// can be misleading.  When contemplating an algorithm change, always do
// interleaved runs with the old & new algorithm.

#include "net/instaweb/rewriter/public/css_minify.h"
#include "pagespeed/kernel/base/benchmark.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "webutil/css/parser.h"
#include "webutil/css/tostring.h"

namespace net_instaweb {

extern const char* CSS_console_css;

namespace {

static void BM_MinifyCss(int iters, int size) {
  GoogleString in_text;
  for (int i = 0; i < size; i += strlen(CSS_console_css)) {
    in_text += CSS_console_css;
  }
  in_text.resize(size);

  NullMessageHandler handler;
  for (int i = 0; i < iters; ++i) {
    Css::Parser parser(in_text);
    parser.set_preservation_mode(true);
    parser.set_quirks_mode(false);
    scoped_ptr<Css::Stylesheet> stylesheet(parser.ParseRawStylesheet());

    GoogleString result;
    StringWriter writer(&result);
    CssMinify::Stylesheet(*stylesheet, &writer, &handler);
  }
}
BENCHMARK_RANGE(BM_MinifyCss, 1<<6, 1<<18);

// Common-case, all chars are normal alpha-num that don't need to be escaped.
static void BM_EscapeStringNormal(int iters, int size) {
  GoogleString ident(size, 'A');
  for (int i = 0; i < iters; ++i) {
    Css::EscapeUrl(ident);
    Css::EscapeString(ident);
  }
}
BENCHMARK_RANGE(BM_EscapeStringNormal, 1, 1<<12);

// Worst-case for chars we actually expect to find in identifiers.
static void BM_EscapeStringSpecial(int iters, int size) {
  GoogleString ident(size, '(');
  for (int i = 0; i < iters; ++i) {
    Css::EscapeUrl(ident);
    Css::EscapeString(ident);
  }
}
BENCHMARK_RANGE(BM_EscapeStringSpecial, 1, 1<<12);

// Worst-case for exotic chars like newlines and tabs in identifiers.
static void BM_EscapeStringSuperSpecial(int iters, int size) {
  GoogleString ident(size, '\t');
  for (int i = 0; i < iters; ++i) {
    Css::EscapeUrl(ident);
    Css::EscapeString(ident);
  }
}
BENCHMARK_RANGE(BM_EscapeStringSuperSpecial, 1, 1<<12);

}  // namespace

}  // namespace net_instaweb
