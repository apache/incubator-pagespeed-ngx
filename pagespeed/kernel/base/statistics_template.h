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

#ifndef PAGESPEED_KERNEL_BASE_STATISTICS_TEMPLATE_H_
#define PAGESPEED_KERNEL_BASE_STATISTICS_TEMPLATE_H_

#include <algorithm>
#include <cstddef>
#include <map>
#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/writer.h"

namespace net_instaweb {
class MessageHandler;

// This class makes it easier to define new Statistics implementations
// by providing a templatized implementation of variable registration and
// management.
template<class Var, class UpDown, class Hist,
         class TimedVar> class StatisticsTemplate
    : public Statistics {
 public:
  StatisticsTemplate() {}
  virtual ~StatisticsTemplate() {
    STLDeleteContainerPointers(variables_.begin(), variables_.end());
    STLDeleteContainerPointers(up_downs_.begin(), up_downs_.end());
    STLDeleteContainerPointers(histograms_.begin(), histograms_.end());
    STLDeleteContainerPointers(timed_vars_.begin(), timed_vars_.end());
  }

  // Implementations of Statistics API --- see base class docs for
  // description.
  virtual Var* AddVariable(const StringPiece& name) {
    Var* var = FindVariable(name);
    if (var == NULL) {
      var = NewVariable(name);
      variables_.push_back(var);
      variable_names_.push_back(name.as_string());
      variable_map_[name.as_string()] = var;
    }
    return var;
  }

  virtual UpDown* AddUpDownCounter(const StringPiece& name) {
    UpDown* var = FindUpDownCounter(name);
    if (var == NULL) {
      var = NewUpDownCounter(name);
      up_downs_.push_back(var);
      up_down_names_.push_back(name.as_string());
      up_down_map_[name.as_string()] = var;
    }
    return var;
  }

  virtual UpDown* AddGlobalUpDownCounter(const StringPiece& name) {
    UpDown* var = FindUpDownCounter(name);
    if (var == NULL) {
      var = NewGlobalUpDownCounter(name);
      up_downs_.push_back(var);
      up_down_names_.push_back(name.as_string());
      up_down_map_[name.as_string()] = var;
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

  virtual UpDown* FindUpDownCounter(const StringPiece& name) const {
    typename UpDownMap::const_iterator p = up_down_map_.find(name.as_string());
    UpDown* var = NULL;
    if (p != up_down_map_.end()) {
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
      timedvar = NewTimedVariable(name);
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
    for (int i = 0, n = up_downs_.size(); i < n; ++i) {
      const GoogleString& up_down_name = up_down_names_[i];
      int length_number = Integer64ToString(up_downs_[i]->Get()).size();
      int length_name = up_down_name.size();
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
    for (int i = 0, n = up_downs_.size(); i < n; ++i) {
      const GoogleString& up_down_name = up_down_names_[i];
      GoogleString up_down_as_str = Integer64ToString(up_downs_[i]->Get());
      writer->Write(up_down_name, message_handler);
      writer->Write(": ", message_handler);
      int num_spaces = longest_string - up_down_name.size() -
          up_down_as_str.size();
      writer->Write(spaces.substr(0, num_spaces), message_handler);
      writer->Write(up_down_as_str, message_handler);
      writer->Write("\n", message_handler);
    }
  }

  // The string written to the writer will be like this:
  // {"variables": {"cache_hits": 10,"cache_misses": 5,...}, "maxlength": 50}
  virtual void DumpJson(Writer* writer, MessageHandler* message_handler) {
    int longest_string = 0;
    writer->Write("{\"variables\": {", message_handler);
    for (int i = 0, n = variables_.size(); i < n; ++i) {
      const GoogleString& var_name = variable_names_[i];
      GoogleString var_as_str = Integer64ToString(variables_[i]->Get());
      int length_name = var_name.size();
      int length_number = var_as_str.size();
      longest_string = std::max(longest_string, length_name + length_number);
      writer->Write(StrCat("\"", var_name, "\": ", var_as_str),
                    message_handler);
      if (i != n - 1) {
        writer->Write(",", message_handler);
      }
    }
    for (int i = 0, n = up_downs_.size(); i < n; ++i) {
      const GoogleString& up_down_name = up_down_names_[i];
      GoogleString up_down_as_str = Integer64ToString(up_downs_[i]->Get());
      int length_name = up_down_name.size();
      int length_number = up_down_as_str.size();
      longest_string = std::max(longest_string, length_name + length_number);
      writer->Write(StrCat(",\"", up_down_name, "\": ", up_down_as_str),
                    message_handler);
    }
    writer->Write("}, \"maxlength\": ", message_handler);
    writer->Write(Integer64ToString(longest_string), message_handler);
    writer->Write("}", message_handler);
  }

  virtual void Clear() {
    for (int i = 0, n = variables_.size(); i < n; ++i) {
      Variable* var = variables_[i];
      var->Clear();
    }
    for (int i = 0, n = up_downs_.size(); i < n; ++i) {
      UpDownCounter* var = up_downs_[i];
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
  virtual Var* NewVariable(StringPiece name) = 0;

  // Interface to subclass.
  virtual UpDown* NewUpDownCounter(StringPiece name) = 0;

  // Default implementation just calls NewUpDownCounter
  virtual UpDown* NewGlobalUpDownCounter(StringPiece name) {
    return NewUpDownCounter(name);
  }

  virtual Hist* NewHistogram(StringPiece name) = 0;
  virtual TimedVar* NewTimedVariable(StringPiece name) = 0;

  size_t variables_size() const { return variables_.size(); }
  Var* variables(size_t pos) { return variables_.at(pos); }

  size_t up_down_size() const { return up_downs_.size(); }
  UpDown* up_downs(size_t pos) { return up_downs_.at(pos); }

  size_t histograms_size() const { return histograms_.size(); }
  Hist* histograms(size_t pos) { return histograms_.at(pos); }

  const GoogleString& histogram_names(size_t pos) const {
    return histogram_names_.at(pos);
  }

 private:
  typedef std::vector<Var*> VarVector;
  typedef std::map<GoogleString, Var*> VarMap;
  typedef std::vector<UpDown*> UpDownVector;
  typedef std::map<GoogleString, UpDown*> UpDownMap;
  typedef std::vector<Hist*> HistVector;
  typedef std::map<GoogleString, Hist*> HistMap;

  typedef std::vector<TimedVar*> TimedVarVector;
  typedef std::map<GoogleString, TimedVar*> TimedVarMap;
  VarVector variables_;
  VarMap variable_map_;
  UpDownVector up_downs_;
  UpDownMap up_down_map_;
  HistVector histograms_;
  HistMap histogram_map_;
  TimedVarVector timed_vars_;
  TimedVarMap timed_var_map_;
  // map between group and names of stats.
  std::map<GoogleString, StringVector> timed_var_group_map_;
  StringVector variable_names_;
  StringVector up_down_names_;
  StringVector histogram_names_;

  DISALLOW_COPY_AND_ASSIGN(StatisticsTemplate);
};

// Helper class to create Variable interface implementations given a
// helper implementation class Impl.  Note that the same Impl class
// can be used for UpDownTemplate, but Variable will not provide a
// Set method, and will DCHECK-fail on negative increments.
//
// class Impl must define methods:
//      Impl(StringPiece name, Statistics* stats);
//      int64 Get();
//      StringPiece GetName();
//      int64 AddHelper(int64 delta);
//      void Clear();
// See ../util/simple_stats.h, class SimpleStatsVariable, for an example
// of an Impl class.
template<class Impl> class VarTemplate : public Variable {
 public:
  VarTemplate(StringPiece name, Statistics* stats) : impl_(name, stats) {}
  virtual ~VarTemplate() {}
  virtual int64 Get() const { return impl_.Get(); }
  virtual StringPiece GetName() const { return impl_.GetName(); }
  virtual int64 AddHelper(int64 delta) { return impl_.AddHelper(delta); }
  virtual void Clear() { impl_.Set(0); }

  Impl* impl() { return &impl_; }

 private:
  Impl impl_;

  DISALLOW_COPY_AND_ASSIGN(VarTemplate);
};

// Helper class to create UpDownCounter interface implementations given a
// helper implementation class Impl.  Note that the same Impl class
// can be used for VarTemplate, but UpDownCounter provides a
// Set method, and will not DCHECK-fail on negative increments.
template<class Impl> class UpDownTemplate : public UpDownCounter {
 public:
  UpDownTemplate(StringPiece name, Statistics* stats)
      : impl_(name, stats) {}
  virtual ~UpDownTemplate() {}
  virtual int64 Get() const { return impl_.Get(); }
  virtual StringPiece GetName() const { return impl_.GetName(); }
  virtual void Set(int64 value) { impl_.Set(value); }
  virtual int64 AddHelper(int64 delta) { return impl_.AddHelper(delta); }
  virtual void Clear() { impl_.Set(0); }

  Impl* impl() { return &impl_; }

 private:
  Impl impl_;

  DISALLOW_COPY_AND_ASSIGN(UpDownTemplate);
};

// A specialization of StatisticsTemplate for implementations where the
// Variable and UpDownCounter implementations can share a common Impl.
template<class Impl,                       // See example in VarTemplate
         class HistC = CountHistogram,     // Histogram
         class TVarC = FakeTimedVariable>  // TimeDVariable
class ScalarStatisticsTemplate
    : public StatisticsTemplate<VarTemplate<Impl>, UpDownTemplate<Impl>,
                                HistC, TVarC> {
 public:
  // Add typedefs for template class args to make them visible to subclasses.
  typedef VarTemplate<Impl> Var;
  typedef UpDownTemplate<Impl> UpDown;
  typedef HistC Hist;
  typedef TVarC TVar;

  ScalarStatisticsTemplate() {}
  virtual ~ScalarStatisticsTemplate() {}

 protected:
  virtual Var* NewVariable(StringPiece name) {
    return new Var(name, this);
  }

  virtual UpDown* NewUpDownCounter(StringPiece name) {
    return new UpDown(name, this);
  }

  virtual TVar* NewTimedVariable(StringPiece name) {
    return new TVar(name, this);
  }
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_STATISTICS_TEMPLATE_H_
