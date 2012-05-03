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
// Tests a few different alternatives to implementing an ordered container
// supporting push_back and pop_front.
//
//
// Benchmark                   Time(ns)    CPU(ns) Iterations
// ----------------------------------------------------------
// BM_List4                        4846       4830     142857
// BM_Deque4                        747        750    1000000
// BM_DequeVector4                  468        470    1489362
// BM_DequeUsingStdVector4         1874       1873     368421
// BM_List100                    118003     118292       5833
// BM_Deque100                    16389      16457      43750
// BM_DequeVector100              10296      10214      63636
// BM_DequeUsingStdVector100      75617      74286       8750

#include <deque>
#include <list>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/util/public/benchmark.h"
#include "net/instaweb/util/public/deque_vector.h"


// Implementation of deque subset interface using vector, with O(N)
// mutations at front and no extra memory.  This is for benchmarking
// comparison.  Surprisingly it beats List even @ 100 elements.
template<class T>
class DequeUsingStdVector : public std::vector<T> {
 public:
  void push_front(const T& value) { this->insert(this->begin(), value); }
  void pop_front() { this->erase(this->begin()); }
};

template<class Deque> static void FourElementWorkout(int iters,
                                                     int num_elements) {
  for (int iter = 0; iter < iters; ++iter) {
    Deque deque;

    // Simple usage as pure stack or queue, but not at the same time.
    for (int i = 0; i < num_elements; ++i) { deque.push_back(i); }
    for (int i = 0; i < num_elements; ++i) {
      CHECK_EQ(i, deque.front());
      deque.pop_front();
    }
    for (int i = 0; i < num_elements; ++i) { deque.push_front(i); }
    for (int i = num_elements - 1; i >= 0; --i) {
      CHECK_EQ(i, deque.front());
      deque.pop_front();
    }
    for (int i = 0; i < num_elements; ++i) { deque.push_front(i); }
    for (int i = 0; i < num_elements; ++i) {
      CHECK_EQ(i, deque.back());
      deque.pop_back();
    }
    for (int i = 0; i < num_elements; ++i) { deque.push_back(i); }
    for (int i = num_elements - 1; i >= 0; --i) {
      CHECK_EQ(i, deque.back());
      deque.pop_back();
    }

    // Comingled pushes to front or back of queue.
    for (int i = 0; i < num_elements / 2; ++i) {
      deque.push_back(i);
      deque.push_front(i);
    }
    for (int i = 0; i < num_elements; ++i) { deque.pop_back(); }
    for (int i = 0; i < num_elements / 2; ++i) {
      deque.push_back(i);
      deque.push_front(i);
    }
    for (int i = 0; i < num_elements; ++i) {
      deque.pop_front();
    }
    for (int i = 0; i < num_elements / 2; ++i) {
      deque.push_front(i);
      deque.push_back(i);
    }
    for (int i = 0; i < num_elements; ++i) { deque.pop_back(); }
    for (int i = 0; i < num_elements / 2; ++i) {
      deque.push_front(i);
      deque.push_back(i);
    }
    for (int i = 0; i < num_elements; ++i) { deque.pop_front(); }

    // Chasing 1 value pushed onto the back and popped from front.
    for (int i = 0; i < 10 * num_elements; ++i) {
      deque.push_back(i);
      CHECK_EQ(i, deque.front());
      deque.pop_front();
    }

    // Chasing 2 values pushed onto the back and popped from front.
    deque.push_back(-1);
    for (int i = 0; i < 10 * num_elements; ++i) {
      deque.push_back(i);
      CHECK_EQ(i - 1, deque.front());
      deque.pop_front();
    }
    deque.pop_front();

    // Chasing 1 value pushed onto the front and popped from back.
    for (int i = 0; i < 10 * num_elements; ++i) {
      deque.push_front(i);
      CHECK_EQ(i, deque.back());
      deque.pop_back();
    }

    // Chasing 2 values pushed onto the front and popped from back.
    deque.push_front(-1);
    for (int i = 0; i < 10 * num_elements; ++i) {
      deque.push_front(i);
      CHECK_EQ(i - 1, deque.back());
      deque.pop_back();
    }
    deque.pop_back();
  }
}

static void BM_List4(int iters) {
  FourElementWorkout<std::list<int> >(iters, 4);
}

static void BM_Deque4(int iters) {
  FourElementWorkout<std::deque<int> >(iters, 4);
}

static void BM_DequeVector4(int iters) {
  FourElementWorkout<DequeVector<int> >(iters, 4);
}

static void BM_DequeUsingStdVector4(int iters) {
  FourElementWorkout<DequeUsingStdVector<int> >(iters, 4);
}

static void BM_List100(int iters) {
  FourElementWorkout<std::list<int> >(iters, 100);
}

static void BM_Deque100(int iters) {
  FourElementWorkout<std::deque<int> >(iters, 100);
}

static void BM_DequeVector100(int iters) {
  FourElementWorkout<DequeVector<int> >(iters, 100);
}

static void BM_DequeUsingStdVector100(int iters) {
  FourElementWorkout<DequeUsingStdVector<int> >(iters, 100);
}

BENCHMARK(BM_List4);
BENCHMARK(BM_Deque4);
BENCHMARK(BM_DequeVector4);
BENCHMARK(BM_DequeUsingStdVector4);

BENCHMARK(BM_List100);
BENCHMARK(BM_Deque100);
BENCHMARK(BM_DequeVector100);
BENCHMARK(BM_DequeUsingStdVector100);
