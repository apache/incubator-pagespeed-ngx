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

#include <map>
#include <vector>
#include <utility>                      // for pair
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

  // Add a new variable, or returns an existing one of that name.
  // The Variable* is owned by the Statistics class -- it should
  // not be deleted by the caller.
  virtual Variable* AddVariable(const StringPiece& name) {
    return AddVariableInternal(name);
  }

  // Find a variable from a name, returning NULL if not found.
  virtual Variable* FindVariable(const StringPiece& name) const {
    return FindVariableInternal(name);
  }

  // Add a new histogram, or returns an exisiting one of that name.
  // The Histogram* is owned by the Statistics class -- it should
  // not be deleted by the caller.
  virtual Histogram* AddHistogram(const StringPiece& name) {
    return AddHistogramInternal(name);
  }

  // Find a histogram from a name, returning NULL if not found.
  virtual Histogram* FindHistogram(const StringPiece& name) const {
    return FindHistogramInternal(name);
  }

  virtual TimedVariable* AddTimedVariable(
      const StringPiece& name, const StringPiece& group) {
    return AddTimedVariableInternal(name, group);
  }

  virtual TimedVariable* FindTimedVariable(
      const StringPiece& name) const {
    return FindTimedVariableInternal(name);
  }

  virtual StringVector& HistogramNames() {
    return histogram_names_;
  }

  virtual std::map<GoogleString, StringVector>& TimedVariableMap() {
    return timed_var_group_map_;
  }

  virtual void Dump(Writer* writer, MessageHandler* message_handler) {
    for (int i = 0, n = variables_.size(); i < n; ++i) {
      Var* var = variables_[i];
      writer->Write(variable_names_[i], message_handler);
      writer->Write(": ", message_handler);
      writer->Write(Integer64ToString(var->Get64()), message_handler);
      writer->Write("\n", message_handler);
    }
  }

  virtual void Clear() {
    for (int i = 0, n = variables_.size(); i < n; ++i) {
      Var* var = variables_[i];
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
  // Adds a variable, returning as the template class Var type, rather
  // than its base class, as AddVariable does.
  virtual Var* AddVariableInternal(const StringPiece& name) {
    Var* var = FindVariableInternal(name);
    if (var == NULL) {
      var = NewVariable(name, variables_.size());
      variables_.push_back(var);
      variable_names_.push_back(name.as_string());
      variable_map_[GoogleString(name.data(), name.size())] = var;
    }
    return var;
  }

  // Finds a variable, returning as the template class Var type, rather
  // than its base class, as FindVariable does.
  virtual Var* FindVariableInternal(const StringPiece& name) const {
    typename VarMap::const_iterator p = variable_map_.find(name.as_string());
    Var* var = NULL;
    if (p != variable_map_.end()) {
      var = p->second;
    }
    return var;
  }

  virtual Var* NewVariable(const StringPiece& name, int index) = 0;

  virtual Histogram* AddHistogramInternal(const StringPiece& name) {
    Histogram* hist = FindHistogramInternal(name);
    if (hist == NULL) {
      hist = NewHistogram();
      histograms_.push_back(hist);
      histogram_names_.push_back(name.as_string());
      histogram_map_[GoogleString(name.data(), name.size())] = hist;
    }
    return hist;
  }
  // Finds a histogram, returning as the template class Hist type, rather
  // than its base class, as FindHistogram does.
  virtual Histogram* FindHistogramInternal(const StringPiece& name) const {
    typename HistMap::const_iterator p = histogram_map_.find(name.as_string());
    Histogram* hist = NULL;
    if (p != histogram_map_.end()) {
      hist = p->second;
    }
    return hist;
  }

  virtual TimedVariable* AddTimedVariableInternal(const StringPiece& name,
                                                  const StringPiece& group) {
    TimedVariable* timedvar = FindTimedVariableInternal(name);
    if (timedvar == NULL) {
      timedvar = NewTimedVariable(name, timed_vars_.size());
      timed_vars_.push_back(timedvar);
      timed_var_map_[GoogleString(name.data(), name.size())] = timedvar;
      timed_var_group_map_[GoogleString(group.data(), group.size())].push_back(
          name.as_string());
    }
    return timedvar;
  }

  virtual TimedVariable* FindTimedVariableInternal(
      const StringPiece& name) const {
    typename TimedVarMap::const_iterator p =
        timed_var_map_.find(name.as_string());
    TimedVariable* timedvar = NULL;
    if (p != timed_var_map_.end()) {
      timedvar = p->second;
    }
    return timedvar;
  }

  typedef std::vector<Var*> VarVector;
  typedef std::map<GoogleString, Var*> VarMap;
  typedef std::vector<Histogram*> HistVector;
  typedef std::map<GoogleString, Histogram*> HistMap;

  typedef std::vector<TimedVariable*> TimedVarVector;
  typedef std::map<GoogleString, TimedVariable*> TimedVarMap;
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

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_TEMPLATE_H_
