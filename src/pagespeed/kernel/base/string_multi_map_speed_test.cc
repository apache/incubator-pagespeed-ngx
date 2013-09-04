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

#include "pagespeed/kernel/base/string_multi_map.h"

#include <set>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/benchmark.h"
#include "pagespeed/kernel/base/string_util.h"

//
// .../src/out/Release/mod_pagespeed_speed_test "BM_Sanitize*
// BM_SanitizeByArray      50000             30782 ns/op
// BM_SanitizeBySet        10000            222213 ns/op


namespace {

static StringPiece kNamesToSanitize[] = {
  // http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html
  "Connection",
  "KeepAlive",
  "Proxy-Authenticate",
  "Proxy-Authorization",
  "SetCookie",
  "SetCookie2",
  "TE",
  "Trailers",
  "Transfer-Encoding",
  "Upgrade",
};

void AddHeaders(net_instaweb::StringMultiMapInsensitive* multi_map) {
  multi_map->Add("Transfer-Encoding", "chunked");
  multi_map->Add("Date", "Fri, 22 Apr 2011 19:34:33 GMT");
  multi_map->Add("Set-Cookie", "CG=US:CA:Mountain+View");
  multi_map->Add("Set-Cookie", "UA=chrome");
  multi_map->Add("Cache-Control", "max-age=100");
  multi_map->Add("Set-Cookie", "path=/");
  multi_map->Add("Vary", "User-Agent");
  multi_map->Add("Set-Cookie", "LA=1275937193");
  multi_map->Add("Vary", "Accept-Encoding");
  multi_map->Add("Connection", "close");
}

void BM_SanitizeByArray(int iters) {
  for (int i = 0; i < iters; ++i) {
    net_instaweb::StringMultiMapInsensitive multi_map;
    AddHeaders(&multi_map);

    CHECK(multi_map.RemoveAllFromSortedArray(kNamesToSanitize,
                                             arraysize(kNamesToSanitize)));

    // Most of the time we'll find nothing (or little) to remove, so bias the
    // test toward that case.
    for (int j = 1; j < 100; ++j) {
      CHECK(!multi_map.RemoveAllFromSortedArray(kNamesToSanitize,
                                                arraysize(kNamesToSanitize)));
    }
  }
}

void BM_SanitizeBySet(int iters) {
  for (int i = 0; i < iters; ++i) {
    net_instaweb::StringMultiMapInsensitive multi_map;
    AddHeaders(&multi_map);
    net_instaweb::StringSetInsensitive remove_set;

    // Most of the time we'll find nothing (or little) to remove, so bias the
    // test toward that case.
    bool expect_remove = true;
    for (int repeat = 0; repeat < 100; ++repeat) {
      for (int j = 0, n = arraysize(kNamesToSanitize); j < n; ++j) {
        remove_set.insert(kNamesToSanitize[j].as_string());
      }
      bool removed_anything = false;
      for (net_instaweb::StringSetInsensitive::const_iterator iter =
               remove_set.begin(); iter != remove_set.end(); ++iter) {
        if (multi_map.RemoveAll(*iter)) {
          removed_anything = true;
        }
      }
      CHECK_EQ(expect_remove, removed_anything);
      expect_remove = false;
    }
  }
}

}  // namespace

BENCHMARK(BM_SanitizeByArray);
BENCHMARK(BM_SanitizeBySet);
