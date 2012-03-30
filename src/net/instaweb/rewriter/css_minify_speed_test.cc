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
// CPU: Intel Nehalem with HyperThreading (2 cores) dL1:32KB dL2:256KB
// Benchmark                         Time(ns)    CPU(ns) Iterations
// ----------------------------------------------------------------
// BM_EscapeStringNormal/1                 26         26   26923077
// BM_EscapeStringNormal/8                 82         82    8750000
// BM_EscapeStringNormal/64               482        480    1458333
// BM_EscapeStringNormal/512             3107       3109     218750
// BM_EscapeStringNormal/4k             25049      25000      28000
// BM_EscapeStringSpecial/1                31         31   22580645
// BM_EscapeStringSpecial/8               198        201    3888889
// BM_EscapeStringSpecial/64              796        789     875000
// BM_EscapeStringSpecial/512            5609       5600     100000
// BM_EscapeStringSpecial/4k            44625      44356      15556
// BM_EscapeStringSuperSpecial/1           43         43   16666667
// BM_EscapeStringSuperSpecial/8          257        256    2692308
// BM_EscapeStringSuperSpecial/64        1631       1623     437500
// BM_EscapeStringSuperSpecial/512      11478      11629      63636
// BM_EscapeStringSuperSpecial/4k       90466      91283       7778

#include "net/instaweb/rewriter/public/css_minify.h"

#include "net/instaweb/util/public/benchmark.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

namespace {

// Common-case, all chars are normal alpha-num that don't need to be escaped.
static void BM_EscapeStringNormal(int iters, int size) {
  GoogleString ident(size, 'A');
  for (int i = 0; i < iters; ++i) {
    CssMinify::EscapeString(ident, true);
    CssMinify::EscapeString(ident, false);
  }
}
BENCHMARK_RANGE(BM_EscapeStringNormal, 1, 1<<12);

// Worst-case for chars we actually expect to find in identifiers.
static void BM_EscapeStringSpecial(int iters, int size) {
  GoogleString ident(size, '(');
  for (int i = 0; i < iters; ++i) {
    CssMinify::EscapeString(ident, true);
    CssMinify::EscapeString(ident, false);
  }
}
BENCHMARK_RANGE(BM_EscapeStringSpecial, 1, 1<<12);

// Worst-case for exotic chars like newlines and tabs in identifiers.
static void BM_EscapeStringSuperSpecial(int iters, int size) {
  GoogleString ident(size, '\t');
  for (int i = 0; i < iters; ++i) {
    CssMinify::EscapeString(ident, true);
    CssMinify::EscapeString(ident, false);
  }
}
BENCHMARK_RANGE(BM_EscapeStringSuperSpecial, 1, 1<<12);

}  // namespace

}  // namespace net_instaweb
