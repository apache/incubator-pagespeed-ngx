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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_TEMPLATE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_TEMPLATE_H_

#include <algorithm>
#include <cstddef>
#include <map>
#include <vector>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {
class MessageHandler;

// This class makes it easier to define new Statistics implementations
// by providing a templatized implementation of variable registration and
// management.
template<class Var, class Hist, class TimedVar> class StatisticsTemplate
    : public Statistics {
 public:
  StatisticsTemplate() {}
  virtual ~StatisticsTemplate() {
    STLDeleteContainerPointers(variables_.begin(), variables_.end());
    STLDeleteContainerPointers(histograms_.begin(), histograms_.end());
    STLDeleteContainerPointers(timed_vars_.begin(), timed_vars_.end());
  }

  // Implementations of Statistics API --- see base class docs for
  // description.
  virtual Var* AddVariable(const StringPiece& name) {
    Var* var = FindVariable(name);
    if (var == NULL) {
      var = NewVariable(name, variables_.size());
      variables_.push_back(var);
      variable_names_.push_back(name.as_string());
      variable_map_[name.as_string()] = var;
    }
    return var;
  }

  virtual Var* AddGlobalVariable(const StringPiece& name) {
    Var* var = FindVariable(name);
    if (var == NULL) {
      var = NewGlobalVariable(name, variables_.size());
      variables_.push_back(var);
      variable_names_.push_back(name.as_string());
      variable_map_[name.as_string()] = var;
    }
    return var;
  }

  virtual Var* FindVariable(const StringPiece& name) const {
    typename VarMap::const_iterator p = variable_map_.find(name.as_string());
    Var* var = NULL;
    if (p != variable_map_.end()) {
      var = p->second;
    }
    return var;
  }

  virtual Hist* AddHistogram(const StringPiece& name) {
    Hist* hist = FindHistogram(name);
    if (hist == NULL) {
      hist = NewHistogram(name);
      histograms_.push_back(hist);
      histogram_names_.push_back(name.as_string());
      histogram_map_[name.as_string()] = hist;
    }
    return hist;
  }

  virtual Hist* FindHistogram(const StringPiece& name) const {
    typename HistMap::const_iterator p = histogram_map_.find(name.as_string());
    Hist* hist = NULL;
    if (p != histogram_map_.end()) {
      hist = p->second;
    }
    return hist;
  }

  virtual TimedVar* AddTimedVariable(const StringPiece& name,
                                     const StringPiece& group) {
    TimedVar* timedvar = FindTimedVariable(name);
    if (timedvar == NULL) {
      timedvar = NewTimedVariable(name, timed_vars_.size());
      timed_vars_.push_back(timedvar);
      timed_var_map_[name.as_string()] = timedvar;
      timed_var_group_map_[group.as_string()].push_back(name.as_string());
    }
    return timedvar;
  }

  virtual TimedVar* FindTimedVariable(const StringPiece& name) const {
    typename TimedVarMap::const_iterator p =
        timed_var_map_.find(name.as_string());
    TimedVar* timedvar = NULL;
    if (p != timed_var_map_.end()) {
      timedvar = p->second;
    }
    return timedvar;
  }

  virtual const StringVector& HistogramNames() {
    return histogram_names_;
  }

  virtual const std::map<GoogleString, StringVector>& TimedVariableMap() {
    return timed_var_group_map_;
  }

  virtual void Dump(Writer* writer, MessageHandler* message_handler) {
    int longest_string = 0;
    for (int i = 0, n = variables_.size(); i < n; ++i) {
      const GoogleString& var_name = variable_names_[i];
      int length_number = Integer64ToString(variables_[i]->Get()).size();
      int length_name = var_name.size();
      longest_string = std::max(longest_string, length_name + length_number);
    }

    GoogleString spaces_buffer = GoogleString(longest_string, ' ');
    StringPiece spaces(spaces_buffer);
    for (int i = 0, n = variables_.size(); i < n; ++i) {
      const GoogleString& var_name = variable_names_[i];
      GoogleString var_as_str = Integer64ToString(variables_[i]->Get());
      writer->Write(var_name, message_handler);
      writer->Write(": ", message_handler);
      int num_spaces = longest_string - var_name.size() - var_as_str.size();
      writer->Write(spaces.substr(0, num_spaces), message_handler);
      writer->Write(var_as_str, message_handler);
      writer->Write("\n", message_handler);
    }
  }

  virtual void Clear() {
    for (int i = 0, n = variables_.size(); i < n; ++i) {
      Variable* var = variables_[i];
      var->Clear();
    }
    for (int i = 0, n = histograms_.size(); i < n; ++i) {
      Histogram* hist = histograms_[i];
      hist->Clear();
    }
    for (int i = 0, n = timed_vars_.size(); i < n; ++i) {
      TimedVariable* timedvar = timed_vars_[i];
      timedvar->Clear();
    }
  }

 protected:
  // Interface to subclass.
  virtual Var* NewVariable(const StringPiece& name, int index) = 0;

  // Default implementation just calls NewVariable
  virtual Var* NewGlobalVariable(const StringPiece& name, int index) {
    return NewVariable(name, index);
  }

  virtual Hist* NewHistogram(const StringPiece& name) = 0;
  virtual TimedVar* NewTimedVariable(const StringPiece& name, int index) = 0;

  size_t variables_size() const { return variables_.size(); }
  Var* variables(size_t pos) { return variables_.at(pos); }

  size_t histograms_size() const { return histograms_.size(); }
  Hist* histograms(size_t pos) { return histograms_.at(pos); }

  const GoogleString& histogram_names(size_t pos) const {
    return histogram_names_.at(pos);
  }

 private:
  typedef std::vector<Var*> VarVector;
  typedef std::map<GoogleString, Var*> VarMap;
  typedef std::vector<Hist*> HistVector;
  typedef std::map<GoogleString, Hist*> HistMap;

  typedef std::vector<TimedVar*> TimedVarVector;
  typedef std::map<GoogleString, TimedVar*> TimedVarMap;
  VarVector variables_;
  VarMap variable_map_;
  HistVector histograms_;
  HistMap histogram_map_;
  TimedVarVector timed_vars_;
  TimedVarMap timed_var_map_;
  // map between group and names of stats.
  std::map<GoogleString, StringVector> timed_var_group_map_;
  StringVector variable_names_;
  StringVector histogram_names_;

  DISALLOW_COPY_AND_ASSIGN(StatisticsTemplate);
};

// A specialization of StatisticsTemplate for implementations that can only
// do scalar statistics variables.
template<class Var>
class ScalarStatisticsTemplate
    : public StatisticsTemplate<Var, NullHistogram, FakeTimedVariable> {
 public:
  ScalarStatisticsTemplate() {}
  virtual ~ScalarStatisticsTemplate() {}

 protected:
  virtual NullHistogram* NewHistogram(const StringPiece& name) {
    return new NullHistogram;
  }

  virtual FakeTimedVariable* NewTimedVariable(const StringPiece& name,
                                              int index) {
    return this->NewFakeTimedVariable(name, index);
  }
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_TEMPLATE_H_
