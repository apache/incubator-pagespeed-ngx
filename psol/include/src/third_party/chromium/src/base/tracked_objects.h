// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACKED_OBJECTS_H_
#define BASE_TRACKED_OBJECTS_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/base_api.h"
#include "base/synchronization/lock.h"
#include "base/tracked.h"
#include "base/threading/thread_local_storage.h"

// TrackedObjects provides a database of stats about objects (generally Tasks)
// that are tracked.  Tracking means their birth, death, duration, birth thread,
// death thread, and birth place are recorded.  This data is carefully spread
// across a series of objects so that the counts and times can be rapidly
// updated without (usually) having to lock the data, and hence there is usually
// very little contention caused by the tracking.  The data can be viewed via
// the about:tasks URL, with a variety of sorting and filtering choices.
//
// These classes serve as the basis of a profiler of sorts for the Tasks system.
// As a result, design decisions were made to maximize speed, by minimizing
// recurring allocation/deallocation, lock contention and data copying.  In the
// "stable" state, which is reached relatively quickly, there is no separate
// marginal allocation cost associated with construction or destruction of
// tracked objects, no locks are generally employed, and probably the largest
// computational cost is associated with obtaining start and stop times for
// instances as they are created and destroyed.  The introduction of worker
// threads had a slight impact on this approach, and required use of some locks
// when accessing data from the worker threads.
//
// The following describes the lifecycle of tracking an instance.
//
// First off, when the instance is created, the FROM_HERE macro is expanded
// to specify the birth place (file, line, function) where the instance was
// created.  That data is used to create a transient Location instance
// encapsulating the above triple of information.  The strings (like __FILE__)
// are passed around by reference, with the assumption that they are static, and
// will never go away.  This ensures that the strings can be dealt with as atoms
// with great efficiency (i.e., copying of strings is never needed, and
// comparisons for equality can be based on pointer comparisons).
//
// Next, a Births instance is created for use ONLY on the thread where this
// instance was created.  That Births instance records (in a base class
// BirthOnThread) references to the static data provided in a Location instance,
// as well as a pointer specifying the thread on which the birth takes place.
// Hence there is at most one Births instance for each Location on each thread.
// The derived Births class contains slots for recording statistics about all
// instances born at the same location.  Statistics currently include only the
// count of instances constructed.
// Since the base class BirthOnThread contains only constant data, it can be
// freely accessed by any thread at any time (i.e., only the statistic needs to
// be handled carefully, and it is ONLY read or written by the birth thread).
//
// Having now either constructed or found the Births instance described above, a
// pointer to the Births instance is then embedded in a base class of the
// instance we're tracking (usually a Task). This fact alone is very useful in
// debugging, when there is a question of where an instance came from.  In
// addition, the birth time is also embedded in the base class Tracked (see
// tracked.h), and used to later evaluate the lifetime duration.
// As a result of the above embedding, we can (for any tracked instance) find
// out its location of birth, and thread of birth, without using any locks, as
// all that data is constant across the life of the process.
//
// The amount of memory used in the above data structures depends on how many
// threads there are, and how many Locations of construction there are.
// Fortunately, we don't use memory that is the product of those two counts, but
// rather we only need one Births instance for each thread that constructs an
// instance at a Location. In many cases, instances (such as Tasks) are only
// created on one thread, so the memory utilization is actually fairly
// restrained.
//
// Lastly, when an instance is deleted, the final tallies of statistics are
// carefully accumulated.  That tallying wrties into slots (members) in a
// collection of DeathData instances.  For each birth place Location that is
// destroyed on a thread, there is a DeathData instance to record the additional
// death count, as well as accumulate the lifetime duration of the instance as
// it is destroyed (dies).  By maintaining a single place to aggregate this
// addition *only* for the given thread, we avoid the need to lock such
// DeathData instances.
//
// With the above lifecycle description complete, the major remaining detail is
// explaining how each thread maintains a list of DeathData instances, and of
// Births instances, and is able to avoid additional (redundant/unnecessary)
// allocations.
//
// Each thread maintains a list of data items specific to that thread in a
// ThreadData instance (for that specific thread only).  The two critical items
// are lists of DeathData and Births instances.  These lists are maintained in
// STL maps, which are indexed by Location. As noted earlier, we can compare
// locations very efficiently as we consider the underlying data (file,
// function, line) to be atoms, and hence pointer comparison is used rather than
// (slow) string comparisons.
//
// To provide a mechanism for iterating over all "known threads," which means
// threads that have recorded a birth or a death, we create a singly linked list
// of ThreadData instances. Each such instance maintains a pointer to the next
// one.  A static member of ThreadData provides a pointer to the first_ item on
// this global list, and access to that first_ item requires the use of a lock_.
// When new ThreadData instances is added to the global list, it is pre-pended,
// which ensures that any prior acquisition of the list is valid (i.e., the
// holder can iterate over it without fear of it changing, or the necessity of
// using an additional lock.  Iterations are actually pretty rare (used
// primarilly for cleanup, or snapshotting data for display), so this lock has
// very little global performance impact.
//
// The above description tries to define the high performance (run time)
// portions of these classes.  After gathering statistics, calls instigated
// by visiting about:tasks will assemble and aggregate data for display. The
// following data structures are used for producing such displays.  They are
// not performance critical, and their only major constraint is that they should
// be able to run concurrently with ongoing augmentation of the birth and death
// data.
//
// For a given birth location, information about births are spread across data
// structures that are asynchronously changing on various threads.  For display
// purposes, we need to construct Snapshot instances for each combination of
// birth thread, death thread, and location, along with the count of such
// lifetimes.  We gather such data into a Snapshot instances, so that such
// instances can be sorted and aggregated (and remain frozen during our
// processing).  Snapshot instances use pointers to constant portions of the
// birth and death datastructures, but have local (frozen) copies of the actual
// statistics (birth count, durations, etc. etc.).
//
// A DataCollector is a container object that holds a set of Snapshots.  A
// DataCollector can be passed from thread to thread, and each thread
// contributes to it by adding or updating Snapshot instances.  DataCollector
// instances are thread safe containers which are passed to various threads to
// accumulate all Snapshot instances.
//
// After an array of Snapshots instances are colleted into a DataCollector, they
// need to be sorted, and possibly aggregated (example: how many threads are in
// a specific consecutive set of Snapshots?  What was the total birth count for
// that set? etc.).  Aggregation instances collect running sums of any set of
// snapshot instances, and are used to print sub-totals in an about:tasks page.
//
// TODO(jar): I need to store DataCollections, and provide facilities for taking
// the difference between two gathered DataCollections.  For now, I'm just
// adding a hack that Reset()'s to zero all counts and stats.  This is also
// done in a slighly thread-unsafe fashion, as the reseting is done
// asynchronously relative to ongoing updates, and worse yet, some data fields
// are 64bit quantities, and are not atomicly accessed (reset or incremented
// etc.).  For basic profiling, this will work "most of the time," and should be
// sufficient... but storing away DataCollections is the "right way" to do this.
//
class MessageLoop;


namespace tracked_objects {

//------------------------------------------------------------------------------
// For a specific thread, and a specific birth place, the collection of all
// death info (with tallies for each death thread, to prevent access conflicts).
class ThreadData;
class BASE_API BirthOnThread {
 public:
  explicit BirthOnThread(const Location& location);

  const Location location() const { return location_; }
  const ThreadData* birth_thread() const { return birth_thread_; }

 private:
  // File/lineno of birth.  This defines the essence of the type, as the context
  // of the birth (construction) often tell what the item is for.  This field
  // is const, and hence safe to access from any thread.
  const Location location_;

  // The thread that records births into this object.  Only this thread is
  // allowed to access birth_count_ (which changes over time).
  const ThreadData* birth_thread_;  // The thread this birth took place on.

  DISALLOW_COPY_AND_ASSIGN(BirthOnThread);
};

//------------------------------------------------------------------------------
// A class for accumulating counts of births (without bothering with a map<>).

class BASE_API Births: public BirthOnThread {
 public:
  explicit Births(const Location& location);

  int birth_count() const { return birth_count_; }

  // When we have a birth we update the count for this BirhPLace.
  void RecordBirth() { ++birth_count_; }

  // When a birthplace is changed (updated), we need to decrement the counter
  // for the old instance.
  void ForgetBirth() { --birth_count_; }  // We corrected a birth place.

  // Hack to quickly reset all counts to zero.
  void Clear() { birth_count_ = 0; }

 private:
  // The number of births on this thread for our location_.
  int birth_count_;

  DISALLOW_COPY_AND_ASSIGN(Births);
};

//------------------------------------------------------------------------------
// Basic info summarizing multiple destructions of an object with a single
// birthplace (fixed Location).  Used both on specific threads, and also used
// in snapshots when integrating assembled data.

class BASE_API DeathData {
 public:
  // Default initializer.
  DeathData() : count_(0), square_duration_(0) {}

  // When deaths have not yet taken place, and we gather data from all the
  // threads, we create DeathData stats that tally the number of births without
  // a corrosponding death.
  explicit DeathData(int count) : count_(count), square_duration_(0) {}

  void RecordDeath(const base::TimeDelta& duration);

  // Metrics accessors.
  int count() const { return count_; }
  base::TimeDelta life_duration() const { return life_duration_; }
  int64 square_duration() const { return square_duration_; }
  int AverageMsDuration() const;
  double StandardDeviation() const;

  // Accumulate metrics from other into this.
  void AddDeathData(const DeathData& other);

  // Simple print of internal state.
  void Write(std::string* output) const;

  // Reset all tallies to zero.
  void Clear();

 private:
  int count_;                // Number of destructions.
  base::TimeDelta life_duration_;    // Sum of all lifetime durations.
  int64 square_duration_;  // Sum of squares in milliseconds.
};

//------------------------------------------------------------------------------
// A temporary collection of data that can be sorted and summarized.  It is
// gathered (carefully) from many threads.  Instances are held in arrays and
// processed, filtered, and rendered.
// The source of this data was collected on many threads, and is asynchronously
// changing.  The data in this instance is not asynchronously changing.

class BASE_API Snapshot {
 public:
  // When snapshotting a full life cycle set (birth-to-death), use this:
  Snapshot(const BirthOnThread& birth_on_thread, const ThreadData& death_thread,
           const DeathData& death_data);

  // When snapshotting a birth, with no death yet, use this:
  Snapshot(const BirthOnThread& birth_on_thread, int count);


  const ThreadData* birth_thread() const { return birth_->birth_thread(); }
  const Location location() const { return birth_->location(); }
  const BirthOnThread& birth() const { return *birth_; }
  const ThreadData* death_thread() const {return death_thread_; }
  const DeathData& death_data() const { return death_data_; }
  const std::string DeathThreadName() const;

  int count() const { return death_data_.count(); }
  base::TimeDelta life_duration() const { return death_data_.life_duration(); }
  int64 square_duration() const { return death_data_.square_duration(); }
  int AverageMsDuration() const { return death_data_.AverageMsDuration(); }

  void Write(std::string* output) const;

  void Add(const Snapshot& other);

 private:
  const BirthOnThread* birth_;  // Includes Location and birth_thread.
  const ThreadData* death_thread_;
  DeathData death_data_;
};
//------------------------------------------------------------------------------
// DataCollector is a container class for Snapshot and BirthOnThread count
// items.  It protects the gathering under locks, so that it could be called via
// Posttask on any threads, or passed to all the target threads in parallel.

class BASE_API DataCollector {
 public:
  typedef std::vector<Snapshot> Collection;

  // Construct with a list of how many threads should contribute.  This helps us
  // determine (in the async case) when we are done with all contributions.
  DataCollector();
  ~DataCollector();

  // Add all stats from the indicated thread into our arrays.  This function is
  // mutex protected, and *could* be called from any threads (although current
  // implementation serialized calls to Append).
  void Append(const ThreadData& thread_data);

  // After the accumulation phase, the following accessor is used to process the
  // data.
  Collection* collection();

  // After collection of death data is complete, we can add entries for all the
  // remaining living objects.
  void AddListOfLivingObjects();

 private:
  typedef std::map<const BirthOnThread*, int> BirthCount;

  // This instance may be provided to several threads to contribute data.  The
  // following counter tracks how many more threads will contribute.  When it is
  // zero, then all asynchronous contributions are complete, and locked access
  // is no longer needed.
  int count_of_contributing_threads_;

  // The array that we collect data into.
  Collection collection_;

  // The total number of births recorded at each location for which we have not
  // seen a death count.
  BirthCount global_birth_count_;

  base::Lock accumulation_lock_;  // Protects access during accumulation phase.

  DISALLOW_COPY_AND_ASSIGN(DataCollector);
};

//------------------------------------------------------------------------------
// Aggregation contains summaries (totals and subtotals) of groups of Snapshot
// instances to provide printing of these collections on a single line.

class BASE_API Aggregation: public DeathData {
 public:
  Aggregation();
  ~Aggregation();

  void AddDeathSnapshot(const Snapshot& snapshot);
  void AddBirths(const Births& births);
  void AddBirth(const BirthOnThread& birth);
  void AddBirthPlace(const Location& location);
  void Write(std::string* output) const;
  void Clear();

 private:
  int birth_count_;
  std::map<std::string, int> birth_files_;
  std::map<Location, int> locations_;
  std::map<const ThreadData*, int> birth_threads_;
  DeathData death_data_;
  std::map<const ThreadData*, int> death_threads_;

  DISALLOW_COPY_AND_ASSIGN(Aggregation);
};

//------------------------------------------------------------------------------
// Comparator is a class that supports the comparison of Snapshot instances.
// An instance is actually a list of chained Comparitors, that can provide for
// arbitrary ordering.  The path portion of an about:tasks URL is translated
// into such a chain, which is then used to order Snapshot instances in a
// vector.  It orders them into groups (for aggregation), and can also order
// instances within the groups (for detailed rendering of the instances in an
// aggregation).

class BASE_API Comparator {
 public:
  // Selector enum is the token identifier for each parsed keyword, most of
  // which specify a sort order.
  // Since it is not meaningful to sort more than once on a specific key, we
  // use bitfields to accumulate what we have sorted on so far.
  enum Selector {
    // Sort orders.
    NIL = 0,
    BIRTH_THREAD = 1,
    DEATH_THREAD = 2,
    BIRTH_FILE = 4,
    BIRTH_FUNCTION = 8,
    BIRTH_LINE = 16,
    COUNT = 32,
    AVERAGE_DURATION = 64,
    TOTAL_DURATION = 128,

    // Imediate action keywords.
    RESET_ALL_DATA = -1,
  };

  explicit Comparator();

  // Reset the comparator to a NIL selector.  Clear() and recursively delete any
  // tiebreaker_ entries.  NOTE: We can't use a standard destructor, because
  // the sort algorithm makes copies of this object, and then deletes them,
  // which would cause problems (either we'd make expensive deep copies, or we'd
  // do more thna one delete on a tiebreaker_.
  void Clear();

  // The less() operator for sorting the array via std::sort().
  bool operator()(const Snapshot& left, const Snapshot& right) const;

  void Sort(DataCollector::Collection* collection) const;

  // Check to see if the items are sort equivalents (should be aggregated).
  bool Equivalent(const Snapshot& left, const Snapshot& right) const;

  // Check to see if all required fields are present in the given sample.
  bool Acceptable(const Snapshot& sample) const;

  // A comparator can be refined by specifying what to do if the selected basis
  // for comparison is insufficient to establish an ordering.  This call adds
  // the indicated attribute as the new "least significant" basis of comparison.
  void SetTiebreaker(Selector selector, const std::string& required);

  // Indicate if this instance is set up to sort by the given Selector, thereby
  // putting that information in the SortGrouping, so it is not needed in each
  // printed line.
  bool IsGroupedBy(Selector selector) const;

  // Using the tiebreakers as set above, we mostly get an ordering, which
  // equivalent groups.  If those groups are displayed (rather than just being
  // aggregated, then the following is used to order them (within the group).
  void SetSubgroupTiebreaker(Selector selector);

  // Translate a keyword and restriction in URL path to a selector for sorting.
  void ParseKeyphrase(const std::string& key_phrase);

  // Parse a query in an about:tasks URL to decide on sort ordering.
  bool ParseQuery(const std::string& query);

  // Output a header line that can be used to indicated what items will be
  // collected in the group.  It lists all (potentially) tested attributes and
  // their values (in the sample item).
  bool WriteSortGrouping(const Snapshot& sample, std::string* output) const;

  // Output a sample, with SortGroup details not displayed.
  void WriteSnapshot(const Snapshot& sample, std::string* output) const;

 private:
  // The selector directs this instance to compare based on the specified
  // members of the tested elements.
  enum Selector selector_;

  // For filtering into acceptable and unacceptable snapshot instance, the
  // following is required to be a substring of the selector_ field.
  std::string required_;

  // If this instance can't decide on an ordering, we can consult a tie-breaker
  // which may have a different basis of comparison.
  Comparator* tiebreaker_;

  // We or together all the selectors we sort on (not counting sub-group
  // selectors), so that we can tell if we've decided to group on any given
  // criteria.
  int combined_selectors_;

  // Some tiebreakrs are for subgroup ordering, and not for basic ordering (in
  // preparation for aggregation).  The subgroup tiebreakers are not consulted
  // when deciding if two items are in equivalent groups.  This flag tells us
  // to ignore the tiebreaker when doing Equivalent() testing.
  bool use_tiebreaker_for_sort_only_;
};


//------------------------------------------------------------------------------
// For each thread, we have a ThreadData that stores all tracking info generated
// on this thread.  This prevents the need for locking as data accumulates.

class BASE_API ThreadData {
 public:
  typedef std::map<Location, Births*> BirthMap;
  typedef std::map<const Births*, DeathData> DeathMap;

  ThreadData();
  ~ThreadData();

  // Using Thread Local Store, find the current instance for collecting data.
  // If an instance does not exist, construct one (and remember it for use on
  // this thread.
  // If shutdown has already started, and we don't yet have an instance, then
  // return null.
  static ThreadData* current();

  // For a given about:tasks URL, develop resulting HTML, and append to output.
  static void WriteHTML(const std::string& query, std::string* output);

  // For a given accumulated array of results, use the comparator to sort and
  // subtotal, writing the results to the output.
  static void WriteHTMLTotalAndSubtotals(
      const DataCollector::Collection& match_array,
      const Comparator& comparator, std::string* output);

  // In this thread's data, record a new birth.
  Births* TallyABirth(const Location& location);

  // Find a place to record a death on this thread.
  void TallyADeath(const Births& lifetimes, const base::TimeDelta& duration);

  // (Thread safe) Get start of list of instances.
  static ThreadData* first();
  // Iterate through the null terminated list of instances.
  ThreadData* next() const { return next_; }

  MessageLoop* message_loop() const { return message_loop_; }
  const std::string ThreadName() const;

  // Using our lock, make a copy of the specified maps.  These calls may arrive
  // from non-local threads, and are used to quickly scan data from all threads
  // in order to build an HTML page for about:tasks.
  void SnapshotBirthMap(BirthMap *output) const;
  void SnapshotDeathMap(DeathMap *output) const;

  // Hack: asynchronously clear all birth counts and death tallies data values
  // in all ThreadData instances.  The numerical (zeroing) part is done without
  // use of a locks or atomics exchanges, and may (for int64 values) produce
  // bogus counts VERY rarely.
  static void ResetAllThreadData();

  // Using our lock to protect the iteration, Clear all birth and death data.
  void Reset();

  // Using the "known list of threads" gathered during births and deaths, the
  // following attempts to run the given function once all all such threads.
  // Note that the function can only be run on threads which have a message
  // loop!
  static void RunOnAllThreads(void (*Func)());

  // Set internal status_ to either become ACTIVE, or later, to be SHUTDOWN,
  // based on argument being true or false respectively.
  // IF tracking is not compiled in, this function will return false.
  static bool StartTracking(bool status);
  static bool IsActive();

#ifdef OS_WIN
  // WARNING: ONLY call this function when all MessageLoops are still intact for
  // all registered threads.  IF you call it later, you will crash.
  // Note: You don't need to call it at all, and you can wait till you are
  // single threaded (again) to do the cleanup via
  // ShutdownSingleThreadedCleanup().
  // Start the teardown (shutdown) process in a multi-thread mode by disabling
  // further additions to thread database on all threads.  First it makes a
  // local (locked) change to prevent any more threads from registering.  Then
  // it Posts a Task to all registered threads to be sure they are aware that no
  // more accumulation can take place.
  static void ShutdownMultiThreadTracking();
#endif

  // WARNING: ONLY call this function when you are running single threaded
  // (again) and all message loops and threads have terminated.  Until that
  // point some threads may still attempt to write into our data structures.
  // Delete recursively all data structures, starting with the list of
  // ThreadData instances.
  static void ShutdownSingleThreadedCleanup();

 private:
  // Current allowable states of the tracking system.  The states always
  // proceed towards SHUTDOWN, and never go backwards.
  enum Status {
    UNINITIALIZED,
    ACTIVE,
    SHUTDOWN,
  };

#if defined(OS_WIN)
  class ThreadSafeDownCounter;
  class RunTheStatic;
#endif

  // Each registered thread is called to set status_ to SHUTDOWN.
  // This is done redundantly on every registered thread because it is not
  // protected by a mutex.  Running on all threads guarantees we get the
  // notification into the memory cache of all possible threads.
  static void ShutdownDisablingFurtherTracking();

  // We use thread local store to identify which ThreadData to interact with.
  static base::ThreadLocalStorage::Slot tls_index_;

  // Link to the most recently created instance (starts a null terminated list).
  static ThreadData* first_;
  // Protection for access to first_.
  static base::Lock list_lock_;

  // We set status_ to SHUTDOWN when we shut down the tracking service. This
  // setting is redundantly established by all participating threads so that we
  // are *guaranteed* (without locking) that all threads can "see" the status
  // and avoid additional calls into the  service.
  static Status status_;

  // Link to next instance (null terminated list). Used to globally track all
  // registered instances (corresponds to all registered threads where we keep
  // data).
  ThreadData* next_;

  // The message loop where tasks needing to access this instance's private data
  // should be directed.  Since some threads have no message loop, some
  // instances have data that can't be (safely) modified externally.
  MessageLoop* message_loop_;

  // A map used on each thread to keep track of Births on this thread.
  // This map should only be accessed on the thread it was constructed on.
  // When a snapshot is needed, this structure can be locked in place for the
  // duration of the snapshotting activity.
  BirthMap birth_map_;

  // Similar to birth_map_, this records informations about death of tracked
  // instances (i.e., when a tracked instance was destroyed on this thread).
  // It is locked before changing, and hence other threads may access it by
  // locking before reading it.
  DeathMap death_map_;

  // Lock to protect *some* access to BirthMap and DeathMap.  The maps are
  // regularly read and written on this thread, but may only be read from other
  // threads.  To support this, we acquire this lock if we are writing from this
  // thread, or reading from another thread.  For reading from this thread we
  // don't need a lock, as there is no potential for a conflict since the
  // writing is only done from this thread.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ThreadData);
};


//------------------------------------------------------------------------------
// Provide simple way to to start global tracking, and to tear down tracking
// when done.  Note that construction and destruction of this object must be
// done when running in  threaded mode (before spawning a lot of threads
// for construction, and after shutting down all the threads for destruction).

// To prevent grabbing thread local store resources time and again if someone
// chooses to try to re-run the browser many times, we maintain global state and
// only allow the tracking system to be started up at most once, and shutdown
// at most once.  See bug 31344 for an example.

class BASE_API AutoTracking {
 public:
  AutoTracking() {
    if (state_ != kNeverBeenRun)
      return;
    ThreadData::StartTracking(true);
    state_ = kRunning;
  }

  ~AutoTracking() {
#ifndef NDEBUG
    if (state_ != kRunning)
      return;
    // We don't do cleanup of any sort in Release build because it is a
    // complete waste of time.  Since Chromium doesn't join all its thread and
    // guarantee we're in a single threaded mode, we don't even do cleanup in
    // debug mode, as it will generate race-checker warnings.
#endif
  }

 private:
  enum State {
    kNeverBeenRun,
    kRunning,
    kTornDownAndStopped,
  };
  static State state_;

  DISALLOW_COPY_AND_ASSIGN(AutoTracking);
};


}  // namespace tracked_objects

#endif  // BASE_TRACKED_OBJECTS_H_
