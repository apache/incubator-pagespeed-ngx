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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_NULL_STATISTICS_H_
#define NET_INSTAWEB_UTIL_PUBLIC_NULL_STATISTICS_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/statistics_template.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class Writer;

class NullStatisticsVariable : public Variable {
 public:
  NullStatisticsVariable() {}
  virtual ~NullStatisticsVariable();
  virtual int Get() const { return 0; }
  virtual void Set(int value) { }
  virtual int64 Get64() const { return 0; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullStatisticsVariable);
};

class NullStatisticsHistogram : public Histogram {
 public:
  NullStatisticsHistogram() {}
  virtual ~NullStatisticsHistogram();
  virtual double Average() { return 0.0; }
  virtual double Percentile(const double perc) { return 0.0; }
  virtual double StandardDeviation() { return 0.0; }
  virtual double Count() { return 0.0; }
  virtual double Maximum() { return 0.0; }
  virtual double Minimum() { return 0.0; }

  virtual void Add(const double value) { }
  virtual void Clear() { }
  virtual bool Empty() { return true; }
  virtual void RenderHeader(Writer* writer, MessageHandler* handler) { }
  virtual void Render(const StringPiece& title, Writer* writer,
                      MessageHandler* handler) { }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullStatisticsHistogram);
};

// Simple name/value pair statistics implementation.
class NullStatistics : public StatisticsTemplate<NullStatisticsVariable,
                                                 NullStatisticsHistogram,
                                                 FakeTimedVariable> {
 public:
  static const int kNotFound;

  NullStatistics() { }
  virtual ~NullStatistics();

 protected:
  virtual NullStatisticsVariable* NewVariable(const StringPiece& name,
                                              int index);
  virtual NullStatisticsHistogram* NewHistogram();
  virtual FakeTimedVariable* NewTimedVariable(const StringPiece& name,
                                              int index);

 private:
  DISALLOW_COPY_AND_ASSIGN(NullStatistics);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_NULL_STATISTICS_H_
