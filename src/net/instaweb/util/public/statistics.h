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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_H_
#define NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_H_

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class Writer;

class Histogram {
 public:
  virtual ~Histogram();

  virtual double Average() = 0;
  // Return estimated value that is greater than perc% of all data.
  // e.g. Percentile(20) returns the value which is greater than
  // 20% of data.
  virtual double Percentile(const double perc) = 0;
  virtual double StandardDeviation() = 0;
  virtual double Count() = 0;
  virtual double Maximum() = 0;
  virtual double Minimum() = 0;

  // Record a value in its bucket.
  virtual void Add(const double value) = 0;
  // Throw away all data.
  virtual void Clear() = 0;
  // True if the histogram is empty.
  virtual bool Empty() = 0;
  // Write script and function to web page. Note that this function should be
  // called only once for one page, should not be used for each histogram.
  virtual void RenderHeader(Writer* writer,
                            MessageHandler* handler) = 0;
  // Export data for histogram graph.
  virtual void Render(const StringPiece& title, Writer* writer,
                      MessageHandler* handler) = 0;
};

class Variable {
 public:
  virtual ~Variable();
  // TODO(sligocki): int -> int64
  virtual int Get() const = 0;
  virtual void Set(int delta) = 0;
  virtual int64 Get64() const = 0;

  virtual void Add(int delta) { Set(delta + Get()); }
  void Clear() { Set(0); }
};

// Helps build a statistics that can be exported as a CSV file.
class Statistics {
 public:
  virtual ~Statistics();

  // Add a new variable, or returns an existing one of that name.
  // The Variable* is owned by the Statistics class -- it should
  // not be deleted by the caller.
  virtual Variable* AddVariable(const StringPiece& name) = 0;

  // Find a variable from a name, returning NULL if not found.
  virtual Variable* FindVariable(const StringPiece& name) const = 0;

  // Find a variable from a name, aborting if not found.
  virtual Variable* GetVariable(const StringPiece& name) const {
    Variable* var = FindVariable(name);
    CHECK(var != NULL) << "Variable not found: " << name;
    return var;
  }

  // Add a new histogram, or returns an existing one of that name.
  // The Histogram* is owned by the Statistics class -- it should not
  // be deleted by the caller.
  virtual Histogram* AddHistogram(const StringPiece& name) = 0;
  // Find a histogram from a name, returning NULL if not found.
  virtual Histogram* FindHistogram(const StringPiece& name) const = 0;
  // Find a histogram from a name, aborting if not found.
  virtual Histogram* GetHistogram(const StringPiece& name) const {
    Histogram* hist = FindHistogram(name);
    CHECK(hist != NULL) << "Histogram not found: " << name;
    return hist;
  }
  // Dump the variable-values to a writer.
  virtual void Dump(Writer* writer, MessageHandler* handler) = 0;

  // Set all variables to 0.
  // Throw away all data in histograms.
  virtual void Clear() = 0;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_H_
