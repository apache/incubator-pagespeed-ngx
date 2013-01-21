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
//

#ifndef NET_INSTAWEB_UTIL_PUBLIC_BENCHMARK_H_
#define NET_INSTAWEB_UTIL_PUBLIC_BENCHMARK_H_


#include "third_party/re2/src/util/benchmark.h"

#undef BENCHMARK
#define BENCHMARK(f) \
    ::testing::Benchmark* _benchmark_##f = (new ::testing::Benchmark(#f, f))->\
        ThreadRange(1, 1)

#undef BENCHMARK_RANGE
#define BENCHMARK_RANGE(f, lo, hi) \
    ::testing::Benchmark* _benchmark_##f = \
        (new ::testing::Benchmark(#f, f, lo, hi))->ThreadRange(1, 1)



#endif  // NET_INSTAWEB_UTIL_PUBLIC_BENCHMARK_H_
