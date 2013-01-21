// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FieldTrial is a class for handling details of statistical experiments
// performed by actual users in the field (i.e., in a shipped or beta product).
// All code is called exclusively on the UI thread currently.
//
// The simplest example is an experiment to see whether one of two options
// produces "better" results across our user population.  In that scenario, UMA
// data is uploaded to aggregate the test results, and this FieldTrial class
// manages the state of each such experiment (state == which option was
// pseudo-randomly selected).
//
// States are typically generated randomly, either based on a one time
// randomization (which will yield the same results, in terms of selecting
// the client for a field trial or not, for every run of the program on a
// given machine), or by a startup randomization (generated each time the
// application starts up, but held constant during the duration of the
// process), or by continuous randomization across a run (where the state
// can be recalculated again and again, many times during a process).
// Continuous randomization is not yet implemented.

//------------------------------------------------------------------------------
// Example:  Suppose we have an experiment involving memory, such as determining
// the impact of some pruning algorithm.
// We assume that we already have a histogram of memory usage, such as:

//   HISTOGRAM_COUNTS("Memory.RendererTotal", count);

// Somewhere in main thread initialization code, we'd probably define an
// instance of a FieldTrial, with code such as:

// // FieldTrials are reference counted, and persist automagically until
// // process teardown, courtesy of their automatic registration in
// // FieldTrialList.
// // Note: This field trial will run in Chrome instances compiled through
// //       8 July, 2015, and after that all instances will be in "StandardMem".
// scoped_refptr<FieldTrial> trial = new FieldTrial("MemoryExperiment", 1000,
//                                                  "StandardMem", 2015, 7, 8);
// const int kHighMemGroup =
//     trial->AppendGroup("HighMem", 20);  // 2% in HighMem group.
// const int kLowMemGroup =
//     trial->AppendGroup("LowMem", 20);   // 2% in LowMem group.
// // Take action depending of which group we randomly land in.
// if (trial->group() == kHighMemGroup)
//   SetPruningAlgorithm(kType1);  // Sample setting of browser state.
// else if (trial->group() == kLowMemGroup)
//   SetPruningAlgorithm(kType2);  // Sample alternate setting.

// We then, in addition to our original histogram, output histograms which have
// slightly different names depending on what group the trial instance happened
// to randomly be assigned:

// HISTOGRAM_COUNTS("Memory.RendererTotal", count);  // The original histogram.
// static const bool memory_renderer_total_trial_exists =
//     FieldTrialList::TrialExists("Memory.RendererTotal");
// if (memory_renderer_total_trial_exists) {
//   HISTOGRAM_COUNTS(FieldTrial::MakeName("Memory.RendererTotal",
//                                         "MemoryExperiment"), count);
// }

// The above code will create four distinct histograms, with each run of the
// application being assigned to of of the three groups, and for each group, the
// correspondingly named histogram will be populated:

// Memory.RendererTotal              // 100% of users still fill this histogram.
// Memory.RendererTotal_HighMem      // 2% of users will fill this histogram.
// Memory.RendererTotal_LowMem       // 2% of users will fill this histogram.
// Memory.RendererTotal_StandardMem  // 96% of users will fill this histogram.

//------------------------------------------------------------------------------

#ifndef BASE_METRICS_FIELD_TRIAL_H_
#define BASE_METRICS_FIELD_TRIAL_H_
#pragma once

#include <map>
#include <string>

#include "base/base_api.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/time.h"

namespace base {

class FieldTrialList;

class BASE_API FieldTrial : public RefCounted<FieldTrial> {
 public:
  typedef int Probability;  // Probability type for being selected in a trial.

  // A return value to indicate that a given instance has not yet had a group
  // assignment (and hence is not yet participating in the trial).
  static const int kNotFinalized;

  // This is the group number of the 'default' group. This provides an easy way
  // to assign all the remaining probability to a group ('default').
  static const int kDefaultGroupNumber;

  // The name is used to register the instance with the FieldTrialList class,
  // and can be used to find the trial (only one trial can be present for each
  // name).  |name| and |default_group_name| may not be empty.
  //
  // Group probabilities that are later supplied must sum to less than or equal
  // to the total_probability. Arguments year, month and day_of_month specify
  // the expiration time. If the build time is after the expiration time then
  // the field trial reverts to the 'default' group.
  //
  // Using this constructor creates a startup-randomized FieldTrial. If you
  // want a one-time randomized trial, call UseOneTimeRandomization() right
  // after construction.
  FieldTrial(const std::string& name, Probability total_probability,
             const std::string& default_group_name, const int year,
             const int month, const int day_of_month);

  // Changes the field trial to use one-time randomization, i.e. produce the
  // same result for the current trial on every run of this client. Must be
  // called right after construction.
  //
  // Before using this method, |FieldTrialList::EnableOneTimeRandomization()|
  // must be called exactly once.
  void UseOneTimeRandomization();

  // Disables this trial, meaning it always determines the default group
  // has been selected. May be called immediately after construction, or
  // at any time after initialization (should not be interleaved with
  // AppendGroup calls). Once disabled, there is no way to re-enable a
  // trial.
  void Disable();

  // Establish the name and probability of the next group in this trial.
  // Sometimes, based on construction randomization, this call may cause the
  // provided group to be *THE* group selected for use in this instance.
  // The return value is the group number of the new group.
  int AppendGroup(const std::string& name, Probability group_probability);

  // Return the name of the FieldTrial (excluding the group name).
  std::string name() const { return name_; }

  // Return the randomly selected group number that was assigned.
  // Return kDefaultGroupNumber if the instance is in the 'default' group.
  // Note that this will force an instance to participate, and make it illegal
  // to attempt to probabilistically add any other groups to the trial.
  int group();

  // If the group's name is empty, a string version containing the group
  // number is used as the group name.
  std::string group_name();

  // Return the default group name of the FieldTrial.
  std::string default_group_name() const { return default_group_name_; }

  // Helper function for the most common use: as an argument to specify the
  // name of a HISTOGRAM.  Use the original histogram name as the name_prefix.
  static std::string MakeName(const std::string& name_prefix,
                              const std::string& trial_name);

  // Enable benchmarking sets field trials to a common setting.
  static void EnableBenchmarking();

 private:
  // Allow tests to access our innards for testing purposes.
  FRIEND_TEST(FieldTrialTest, Registration);
  FRIEND_TEST(FieldTrialTest, AbsoluteProbabilities);
  FRIEND_TEST(FieldTrialTest, RemainingProbability);
  FRIEND_TEST(FieldTrialTest, FiftyFiftyProbability);
  FRIEND_TEST(FieldTrialTest, MiddleProbabilities);
  FRIEND_TEST(FieldTrialTest, OneWinner);
  FRIEND_TEST(FieldTrialTest, DisableProbability);
  FRIEND_TEST(FieldTrialTest, Save);
  FRIEND_TEST(FieldTrialTest, DuplicateRestore);
  FRIEND_TEST(FieldTrialTest, MakeName);
  FRIEND_TEST(FieldTrialTest, HashClientId);
  FRIEND_TEST(FieldTrialTest, HashClientIdIsUniform);
  FRIEND_TEST(FieldTrialTest, UseOneTimeRandomization);

  friend class base::FieldTrialList;

  friend class RefCounted<FieldTrial>;

  virtual ~FieldTrial();

  // Returns the group_name. A winner need not have been chosen.
  std::string group_name_internal() const { return group_name_; }

  // Get build time.
  static Time GetBuildTime();

  // Calculates a uniformly-distributed double between [0.0, 1.0) given
  // a |client_id| and a |trial_name| (the latter is used as salt to avoid
  // separate one-time randomized trials from all having the same results).
  static double HashClientId(const std::string& client_id,
                             const std::string& trial_name);

  // The name of the field trial, as can be found via the FieldTrialList.
  const std::string name_;

  // The maximum sum of all probabilities supplied, which corresponds to 100%.
  // This is the scaling factor used to adjust supplied probabilities.
  const Probability divisor_;

  // The name of the default group.
  const std::string default_group_name_;

  // The randomly selected probability that is used to select a group (or have
  // the instance not participate).  It is the product of divisor_ and a random
  // number between [0, 1).
  Probability random_;

  // Sum of the probabilities of all appended groups.
  Probability accumulated_group_probability_;

  int next_group_number_;

  // The pseudo-randomly assigned group number.
  // This is kNotFinalized if no group has been assigned.
  int group_;

  // A textual name for the randomly selected group. Valid after |group()|
  // has been called.
  std::string group_name_;

  // When enable_field_trial_ is false, field trial reverts to the 'default'
  // group.
  bool enable_field_trial_;

  // When benchmarking is enabled, field trials all revert to the 'default'
  // group.
  static bool enable_benchmarking_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrial);
};

//------------------------------------------------------------------------------
// Class with a list of all active field trials.  A trial is active if it has
// been registered, which includes evaluating its state based on its probaility.
// Only one instance of this class exists.
class BASE_API FieldTrialList {
 public:
  // Define a separator charactor to use when creating a persistent form of an
  // instance.  This is intended for use as a command line argument, passed to a
  // second process to mimic our state (i.e., provide the same group name).
  static const char kPersistentStringSeparator;  // Currently a slash.

  // Define expiration year in future. It is initialized to two years from Now.
  static int kExpirationYearInFuture;

  // Observer is notified when a FieldTrial's group is selected.
  class Observer {
   public:
    // Notify observers when FieldTrials's group is selected.
    virtual void OnFieldTrialGroupFinalized(const std::string& trial_name,
                                            const std::string& group_name) = 0;

   protected:
    virtual ~Observer() {}
  };

  // This singleton holds the global list of registered FieldTrials.
  //
  // |client_id| should be an opaque, diverse ID for this client that does not
  // change between sessions, to enable one-time randomized trials. The empty
  // string may be provided, in which case one-time randomized trials will
  // not be available.
  explicit FieldTrialList(const std::string& client_id);
  // Destructor Release()'s references to all registered FieldTrial instances.
  ~FieldTrialList();

  // Register() stores a pointer to the given trial in a global map.
  // This method also AddRef's the indicated trial.
  static void Register(FieldTrial* trial);

  // The Find() method can be used to test to see if a named Trial was already
  // registered, or to retrieve a pointer to it from the global map.
  static FieldTrial* Find(const std::string& name);

  // Returns the group number chosen for the named trial, or
  // FieldTrial::kNotFinalized if the trial does not exist.
  static int FindValue(const std::string& name);

  // Returns the group name chosen for the named trial, or the
  // empty string if the trial does not exist.
  static std::string FindFullName(const std::string& name);

  // Returns true if the named trial has been registered.
  static bool TrialExists(const std::string& name);

  // Create a persistent representation of all FieldTrial instances and the
  // |client_id()| state for resurrection in another process.  This allows
  // randomization to be done in one process, and secondary processes can by
  // synchronized on the result. The resulting string contains the
  // |client_id()|, the names, the trial name, and a "/" separator.
  static void StatesToString(std::string* output);

  // Use a previously generated state string (re: StatesToString()) augment the
  // current list of field tests to include the supplied tests, and using a 100%
  // probability for each test, force them to have the same group string.  This
  // is commonly used in a non-browser process, to carry randomly selected state
  // in a browser process into this non-browser process. This method calls
  // CreateFieldTrial to create the FieldTrial in the non-browser process.
  // Currently only the group_name_ and name_ are restored, as well as the
  // |client_id()| state, that could be used for one-time randomized trials
  // set up only in child processes.
  static bool CreateTrialsInChildProcess(const std::string& prior_trials);

  // Create a FieldTrial with the given |name| and using 100% probability for
  // the FieldTrial, force FieldTrial to have the same group string as
  // |group_name|. This is commonly used in a non-browser process, to carry
  // randomly selected state in a browser process into this non-browser process.
  // Currently only the group_name_ and name_ are set in the FieldTrial. It
  // returns NULL if there is a FieldTrial that is already registered with the
  // same |name| but has different finalized group string (|group_name|).
  static FieldTrial* CreateFieldTrial(const std::string& name,
                                      const std::string& group_name);

  // Add an observer to be notified when a field trial is irrevocably committed
  // to being part of some specific field_group (and hence the group_name is
  // also finalized for that field_trial).
  static void AddObserver(Observer* observer);

  // Remove an observer.
  static void RemoveObserver(Observer* observer);

  // Notify all observers that a group is finalized for the named Trial.
  static void NotifyFieldTrialGroupSelection(const std::string& name,
                                             const std::string& group_name);

  // The time of construction of the global map is recorded in a static variable
  // and is commonly used by experiments to identify the time since the start
  // of the application.  In some experiments it may be useful to discount
  // data that is gathered before the application has reached sufficient
  // stability (example: most DLL have loaded, etc.)
  static TimeTicks application_start_time() {
    if (global_)
      return global_->application_start_time_;
    // For testing purposes only, or when we don't yet have a start time.
    return TimeTicks::Now();
  }

  // Return the number of active field trials.
  static size_t GetFieldTrialCount();

  // Returns true if you can call |FieldTrial::UseOneTimeRandomization()|
  // without error, i.e. if a non-empty string was provided as the client_id
  // when constructing the FieldTrialList singleton.
  static bool IsOneTimeRandomizationEnabled();

  // Returns an opaque, diverse ID for this client that does not change
  // between sessions.
  //
  // Returns the empty string if one-time randomization is not enabled.
  static const std::string& client_id();

 private:
  // A map from FieldTrial names to the actual instances.
  typedef std::map<std::string, FieldTrial*> RegistrationList;

  // Helper function should be called only while holding lock_.
  FieldTrial* PreLockedFind(const std::string& name);

  static FieldTrialList* global_;  // The singleton of this class.

  // This will tell us if there is an attempt to register a field
  // trial or check if one-time randomization is enabled without
  // creating the FieldTrialList. This is not an error, unless a
  // FieldTrialList is created after that.
  static bool used_without_global_;

  // A helper value made available to users, that shows when the FieldTrialList
  // was initialized.  Note that this is a singleton instance, and hence is a
  // good approximation to the start of the process.
  TimeTicks application_start_time_;

  // Lock for access to registered_.
  base::Lock lock_;
  RegistrationList registered_;

  // An opaque, diverse ID for this client that does not change
  // between sessions, or the empty string if not initialized.
  std::string client_id_;

  // List of observers to be notified when a group is selected for a FieldTrial.
  ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrialList);
};

}  // namespace base

#endif  // BASE_METRICS_FIELD_TRIAL_H_
