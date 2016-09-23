// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: cheesy@google.com (Steve Hill)

#ifndef PAGESPEED_CONTROLLER_CONTEXT_REGISTRY_H_
#define PAGESPEED_CONTROLLER_CONTEXT_REGISTRY_H_

#include <unistd.h>
#include <memory>
#include <unordered_set>

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

// To cleanly shut down gRPC, either client or server side, you need to call
// TryCancel on all outstanding {Client,Server}Contexts and then wait for the
// cancellations to process. ContextRegistry holds a list of active Contexts and
// provides the blocking CancelAllActive() method which cancels all contained
// contexts and then waits for them to be removed from the Registry.

template <typename ContextT>
class ContextRegistry {
 public:
  ContextRegistry(ThreadSystem* thread_system);
  virtual ~ContextRegistry();

  // Returns whether the ContextT was registered or not. Will only fail once
  // CancelAllActive has been called.
  bool TryRegisterContext(ContextT* ctx)
      LOCKS_EXCLUDED(mutex_) WARN_UNUSED_RESULT;
  void RemoveContext(ContextT* ctx) LOCKS_EXCLUDED(mutex_);

  // Calls TryCancel on all contained Contexts and then blocks until all have
  // been Removed. Some other thread(s) must process the cancellations or this
  // will block forever. Note that mutex_ is held while TryCancel is called, so
  // TryCancel must not call back into this registry. gRPC delivers
  // cancellations asynchronously afer TryCancel() has returned, so that is not
  // a problem for the intended use.
  void CancelAllActiveAndWait() LOCKS_EXCLUDED(mutex_);

  // Calls TryCancel on all Contained Contexts and then returns immediately.
  // As above, mutex_ is held while TryCancel is called, so TryCancel must
  // not call back into the registry.
  void CancelAllActive() LOCKS_EXCLUDED(mutex_);

  // Whether CancelAllActive has been called yet. Once this starts returning
  // true, it will never again return false. Because of this, a true return
  // can safely be used to skip work on the assumption that TryRegisterContext
  // will fail. However the converse is NOT true: You must use
  // TryRegisterContext to check if it is safe to to work.
  bool IsShutdown() const LOCKS_EXCLUDED(mutex_);

  // Number of contained contexts.
  int Size() const LOCKS_EXCLUDED(mutex_);

  // For use in tests.
  bool Empty() const LOCKS_EXCLUDED(mutex_);

 private:
  typedef std::unordered_set<ContextT*> ContextSet;

  std::unique_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  std::unique_ptr<ThreadSystem::Condvar> condvar_ GUARDED_BY(mutex_);
  bool shutdown_ GUARDED_BY(mutex_);
  ContextSet contexts_ GUARDED_BY(mutex_);
};

template <typename ContextT>
ContextRegistry<ContextT>::ContextRegistry(ThreadSystem* thread_system)
    : mutex_(thread_system->NewMutex()),
      condvar_(mutex_->NewCondvar()),
      shutdown_(false) {
}

template <typename ContextT>
ContextRegistry<ContextT>::~ContextRegistry() {
  DCHECK_EQ(contexts_.size(), 0);
}

template <typename ContextT>
bool ContextRegistry<ContextT>::Empty() const {
  ScopedMutex lock(mutex_.get());
  return contexts_.empty();
}

template <typename ContextT>
int ContextRegistry<ContextT>::Size() const {
  ScopedMutex lock(mutex_.get());
  return static_cast<int>(contexts_.size());
}

template <typename ContextT>
bool ContextRegistry<ContextT>::IsShutdown() const {
  ScopedMutex lock(mutex_.get());
  return shutdown_;
}

template <typename ContextT>
bool ContextRegistry<ContextT>::TryRegisterContext(ContextT* context) {
  CHECK(context != nullptr);

  ScopedMutex lock(mutex_.get());
  bool inserted = false;
  if (!shutdown_) {
    inserted = contexts_.insert(context).second;
    DCHECK(inserted);
  }
  return inserted;
}

template <typename ContextT>
void ContextRegistry<ContextT>::RemoveContext(ContextT* context) {
  ScopedMutex lock(mutex_.get());
  int num_erased = contexts_.erase(context);
  DCHECK_EQ(num_erased, 1);
  if (num_erased > 0 && contexts_.empty() && shutdown_) {
    condvar_->Broadcast();
  }
}

template <typename ContextT>
void ContextRegistry<ContextT>::CancelAllActive() {
  ContextSet old_contexts;
  {
    ScopedMutex lock(mutex_.get());
    shutdown_ = true;
    // Copy clients so we can iterate over it without the mutex held.
    // shutdown_ will prevent any additional contexts from being added.
    // This cannot use the usual "swap" trick because we want to wait for the
    // contexts to be removed via calls to RemoveContext on another thread.
    old_contexts = contexts_;
  }

  // If there is nothing to do, we might as well avoid taking the lock a second
  // time, below.
  if (old_contexts.empty()) {
    return;
  }

  for (ContextT* ctx : old_contexts) {
    // There might be a few hundred entries in old_contexts, so we prefer not to
    // hold the lock while we iterate through all of them. However, as soon as
    // we release the lock, contexts that finish naturally can call back into
    // RemoveContext(), after which they are deleted. Thus, we must check every
    // pointer is still in contexts_ (ie: alive) before we call TryCancel() on
    // it.
    mutex_->Lock();
    if (contexts_.find(ctx) != contexts_.end()) {
      // Per gRPC documentation, it's safe to call this no matter where in the
      // lifecycle we are.
      ctx->TryCancel();
    }
    mutex_->Unlock();
    usleep(1);  // yield to other threads.
  }
}

template <typename ContextT>
void ContextRegistry<ContextT>::CancelAllActiveAndWait() {
  CancelAllActive();

  // Now wait for contexts_ to drain as the Cancel events are processed.
  {
    ScopedMutex lock(mutex_.get());
    while (!contexts_.empty()) {
      condvar_->Wait();
    }
  }
}

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_CONTEXT_REGISTRY_H_
