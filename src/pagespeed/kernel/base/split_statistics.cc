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

// Author: morlovich@google.com (Maksim Orlovich)
// See the header for a description.

#include "pagespeed/kernel/base/split_statistics.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

SplitUpDownCounter::SplitUpDownCounter(UpDownCounter* rw, UpDownCounter* w)
    : rw_(rw), w_(w) {
}

SplitUpDownCounter::~SplitUpDownCounter() {
}

void SplitUpDownCounter::Set(int64 value) {
  w_->Set(value);
  rw_->Set(value);
}

int64 SplitUpDownCounter::SetReturningPreviousValue(int64 value) {
  w_->Set(value);
  return rw_->SetReturningPreviousValue(value);
}

int64 SplitUpDownCounter::Get() const {
  return rw_->Get();
}

StringPiece SplitUpDownCounter::GetName() const {
  return rw_->GetName();
}

int64 SplitUpDownCounter::AddHelper(int delta) {
  w_->Add(delta);
  return rw_->Add(delta);
}

SplitVariable::SplitVariable(Variable* rw, Variable* w)
    : rw_(rw), w_(w) {
}

SplitVariable::~SplitVariable() {
}

int64 SplitVariable::Get() const {
  return rw_->Get();
}

void SplitVariable::Clear() {
  w_->Clear();
  rw_->Clear();
}

StringPiece SplitVariable::GetName() const {
  return rw_->GetName();
}

int64 SplitVariable::AddHelper(int delta) {
  w_->Add(delta);
  return rw_->Add(delta);
}

SplitHistogram::SplitHistogram(
    ThreadSystem* threads, Histogram* rw, Histogram* w)
    : lock_(threads->NewMutex()), rw_(rw), w_(w) {
}

SplitHistogram::~SplitHistogram() {
}

void SplitHistogram::Add(double value) {
  w_->Add(value);
  rw_->Add(value);
}

void SplitHistogram::Clear() {
  // Clear only resets local on purpose, in case it's tied to a clear button
  // in a UI.
  rw_->Clear();
}

void SplitHistogram::Render(int index, Writer* writer,
                            MessageHandler* handler) {
  rw_->Render(index, writer, handler);
}

int SplitHistogram::NumBuckets() {
  return rw_->NumBuckets();
}

void SplitHistogram::EnableNegativeBuckets() {
  w_->EnableNegativeBuckets();
  rw_->EnableNegativeBuckets();
}

void SplitHistogram::SetMinValue(double value) {
  w_->SetMinValue(value);
  rw_->SetMinValue(value);
}

void SplitHistogram::SetMaxValue(double value) {
  w_->SetMaxValue(value);
  rw_->SetMaxValue(value);
}

void SplitHistogram::SetSuggestedNumBuckets(int i) {
  w_->SetSuggestedNumBuckets(i);
  rw_->SetSuggestedNumBuckets(i);
}

double SplitHistogram::BucketStart(int index) {
  return rw_->BucketStart(index);
}

double SplitHistogram::BucketLimit(int index) {
  return rw_->BucketLimit(index);
}

double SplitHistogram::BucketCount(int index) {
  return rw_->BucketCount(index);
}

double SplitHistogram::AverageInternal() {
  return rw_->Average();
}

double SplitHistogram::PercentileInternal(const double perc) {
  return rw_->Percentile(perc);
}

double SplitHistogram::StandardDeviationInternal() {
  return rw_->StandardDeviation();
}

double SplitHistogram::CountInternal() {
  return rw_->Count();
}

double SplitHistogram::MaximumInternal() {
  return rw_->Maximum();
}

double SplitHistogram::MinimumInternal() {
  return rw_->Minimum();
}

AbstractMutex* SplitHistogram::lock() {
  return lock_.get();
}

SplitTimedVariable::SplitTimedVariable(TimedVariable* rw, TimedVariable* w)
    : rw_(rw), w_(w) {
}

SplitTimedVariable::~SplitTimedVariable() {
}

void SplitTimedVariable::IncBy(int64 delta) {
  w_->IncBy(delta);
  rw_->IncBy(delta);
}

int64 SplitTimedVariable::Get(int level) {
  return rw_->Get(level);
}

void SplitTimedVariable::Clear() {
  // Clear only resets local on purpose, in case it's tied to a clear button
  // in a UI.
  rw_->Clear();
}

SplitStatistics::SplitStatistics(
    ThreadSystem* thread_system, Statistics* local, Statistics* global)
    : thread_system_(thread_system),
      local_(local),
      global_(global) {
}

SplitStatistics::~SplitStatistics() {
}

SplitUpDownCounter* SplitStatistics::NewUpDownCounter(StringPiece name) {
  UpDownCounter* local_var = local_->FindUpDownCounter(name);
  CHECK(local_var != NULL);

  UpDownCounter* global_var = global_->FindUpDownCounter(name);
  CHECK(global_var != NULL);

  return new SplitUpDownCounter(local_var /* read/write */,
                                global_var /* write only */);
}

SplitVariable* SplitStatistics::NewVariable(StringPiece name) {
  Variable* local_var = local_->FindVariable(name);
  CHECK(local_var != NULL);

  Variable* global_var = global_->FindVariable(name);
  CHECK(global_var != NULL);

  return new SplitVariable(local_var /* read/write */,
                           global_var /* write only */);
}

SplitUpDownCounter* SplitStatistics::NewGlobalUpDownCounter(StringPiece name) {
  UpDownCounter* local_var = local_->FindUpDownCounter(name);
  CHECK(local_var != NULL);

  UpDownCounter* global_var = global_->FindUpDownCounter(name);
  CHECK(global_var != NULL);

  // For NewGlobalUpDownCounter we reverse global and local from their usual
  // behavior in NewVariable, doing reads from the global/aggregate.
  return new SplitUpDownCounter(global_var /* read/write */,
                                local_var /* write only */);
}

SplitHistogram* SplitStatistics::NewHistogram(StringPiece name) {
  Histogram* local_histo = local_->FindHistogram(name);
  CHECK(local_histo != NULL);

  Histogram* global_histo = global_->FindHistogram(name);
  CHECK(global_histo != NULL);

  return new SplitHistogram(thread_system_,
                            local_histo /* read/write */,
                            global_histo /* write only */);
}

SplitTimedVariable* SplitStatistics::NewTimedVariable(StringPiece name) {
  TimedVariable* local_timed_var = local_->FindTimedVariable(name);
  CHECK(local_timed_var != NULL);

  TimedVariable* global_timed_var = global_->FindTimedVariable(name);
  CHECK(global_timed_var != NULL);

  return new SplitTimedVariable(local_timed_var /* read/write */,
                                global_timed_var /* write only */);
}

}  // namespace net_instaweb
