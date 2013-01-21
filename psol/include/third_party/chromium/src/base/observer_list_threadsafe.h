// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OBSERVER_LIST_THREADSAFE_H_
#define BASE_OBSERVER_LIST_THREADSAFE_H_
#pragma once

#include <algorithm>
#include <map>

#include "base/basictypes.h"
#include "base/callback_old.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "base/task.h"

///////////////////////////////////////////////////////////////////////////////
//
// OVERVIEW:
//
//   A thread-safe container for a list of observers.
//   This is similar to the observer_list (see observer_list.h), but it
//   is more robust for multi-threaded situations.
//
//   The following use cases are supported:
//    * Observers can register for notifications from any thread.
//      Callbacks to the observer will occur on the same thread where
//      the observer initially called AddObserver() from.
//    * Any thread may trigger a notification via Notify().
//    * Observers can remove themselves from the observer list inside
//      of a callback.
//    * If one thread is notifying observers concurrently with an observer
//      removing itself from the observer list, the notifications will
//      be silently dropped.
//
//   The drawback of the threadsafe observer list is that notifications
//   are not as real-time as the non-threadsafe version of this class.
//   Notifications will always be done via PostTask() to another thread,
//   whereas with the non-thread-safe observer_list, notifications happen
//   synchronously and immediately.
//
//   IMPLEMENTATION NOTES
//   The ObserverListThreadSafe maintains an ObserverList for each thread
//   which uses the ThreadSafeObserver.  When Notifying the observers,
//   we simply call PostTask to each registered thread, and then each thread
//   will notify its regular ObserverList.
//
///////////////////////////////////////////////////////////////////////////////

// Forward declaration for ObserverListThreadSafeTraits.
template <class ObserverType>
class ObserverListThreadSafe;

// This class is used to work around VS2005 not accepting:
//
// friend class
//     base::RefCountedThreadSafe<ObserverListThreadSafe<ObserverType> >;
//
// Instead of friending the class, we could friend the actual function
// which calls delete.  However, this ends up being
// RefCountedThreadSafe::DeleteInternal(), which is private.  So we
// define our own templated traits class so we can friend it.
template <class T>
struct ObserverListThreadSafeTraits {
  static void Destruct(const ObserverListThreadSafe<T>* x) {
    delete x;
  }
};

template <class ObserverType>
class ObserverListThreadSafe
    : public base::RefCountedThreadSafe<
        ObserverListThreadSafe<ObserverType>,
        ObserverListThreadSafeTraits<ObserverType> > {
 public:
  typedef typename ObserverList<ObserverType>::NotificationType
      NotificationType;

  ObserverListThreadSafe()
      : type_(ObserverListBase<ObserverType>::NOTIFY_ALL) {}
  explicit ObserverListThreadSafe(NotificationType type) : type_(type) {}

  // Add an observer to the list.  An observer should not be added to
  // the same list more than once.
  void AddObserver(ObserverType* obs) {
    ObserverList<ObserverType>* list = NULL;
    MessageLoop* loop = MessageLoop::current();
    // TODO(mbelshe): Get rid of this check.  Its needed right now because
    //                Time currently triggers usage of the ObserverList.
    //                And unittests use time without a MessageLoop.
    if (!loop)
      return;  // Some unittests may access this without a message loop.
    {
      base::AutoLock lock(list_lock_);
      if (observer_lists_.find(loop) == observer_lists_.end())
        observer_lists_[loop] = new ObserverList<ObserverType>(type_);
      list = observer_lists_[loop];
    }
    list->AddObserver(obs);
  }

  // Remove an observer from the list if it is in the list.
  // If there are pending notifications in-transit to the observer, they will
  // be aborted.
  // If the observer to be removed is in the list, RemoveObserver MUST
  // be called from the same thread which called AddObserver.
  void RemoveObserver(ObserverType* obs) {
    ObserverList<ObserverType>* list = NULL;
    MessageLoop* loop = MessageLoop::current();
    if (!loop)
      return;  // On shutdown, it is possible that current() is already null.
    {
      base::AutoLock lock(list_lock_);
      typename ObserversListMap::iterator it = observer_lists_.find(loop);
      if (it == observer_lists_.end()) {
        // This may happen if we try to remove an observer on a thread
        // we never added an observer for.
        return;
      }
      list = it->second;

      // If we're about to remove the last observer from the list,
      // then we can remove this observer_list entirely.
      if (list->HasObserver(obs) && list->size() == 1)
        observer_lists_.erase(it);
    }
    list->RemoveObserver(obs);

    // If RemoveObserver is called from a notification, the size will be
    // nonzero.  Instead of deleting here, the NotifyWrapper will delete
    // when it finishes iterating.
    if (list->size() == 0)
      delete list;
  }

  // Notify methods.
  // Make a thread-safe callback to each Observer in the list.
  // Note, these calls are effectively asynchronous.  You cannot assume
  // that at the completion of the Notify call that all Observers have
  // been Notified.  The notification may still be pending delivery.
  template <class Method>
  void Notify(Method m) {
    UnboundMethod<ObserverType, Method, Tuple0> method(m, MakeTuple());
    Notify<Method, Tuple0>(method);
  }

  template <class Method, class A>
  void Notify(Method m, const A& a) {
    UnboundMethod<ObserverType, Method, Tuple1<A> > method(m, MakeTuple(a));
    Notify<Method, Tuple1<A> >(method);
  }

  template <class Method, class A, class B>
  void Notify(Method m, const A& a, const B& b) {
    UnboundMethod<ObserverType, Method, Tuple2<A, B> > method(
        m, MakeTuple(a, b));
    Notify<Method, Tuple2<A, B> >(method);
  }

  template <class Method, class A, class B, class C>
  void Notify(Method m, const A& a, const B& b, const C& c) {
    UnboundMethod<ObserverType, Method, Tuple3<A, B, C> > method(
        m, MakeTuple(a, b, c));
    Notify<Method, Tuple3<A, B, C> >(method);
  }

  template <class Method, class A, class B, class C, class D>
  void Notify(Method m, const A& a, const B& b, const C& c, const D& d) {
    UnboundMethod<ObserverType, Method, Tuple4<A, B, C, D> > method(
        m, MakeTuple(a, b, c, d));
    Notify<Method, Tuple4<A, B, C, D> >(method);
  }

  // TODO(mbelshe):  Add more wrappers for Notify() with more arguments.

 private:
  // See comment above ObserverListThreadSafeTraits' definition.
  friend struct ObserverListThreadSafeTraits<ObserverType>;

  ~ObserverListThreadSafe() {
    typename ObserversListMap::const_iterator it;
    for (it = observer_lists_.begin(); it != observer_lists_.end(); ++it)
      delete (*it).second;
    observer_lists_.clear();
  }

  template <class Method, class Params>
  void Notify(const UnboundMethod<ObserverType, Method, Params>& method) {
    base::AutoLock lock(list_lock_);
    typename ObserversListMap::iterator it;
    for (it = observer_lists_.begin(); it != observer_lists_.end(); ++it) {
      MessageLoop* loop = (*it).first;
      ObserverList<ObserverType>* list = (*it).second;
      loop->PostTask(
          FROM_HERE,
          NewRunnableMethod(this,
              &ObserverListThreadSafe<ObserverType>::
                 template NotifyWrapper<Method, Params>, list, method));
    }
  }

  // Wrapper which is called to fire the notifications for each thread's
  // ObserverList.  This function MUST be called on the thread which owns
  // the unsafe ObserverList.
  template <class Method, class Params>
  void NotifyWrapper(ObserverList<ObserverType>* list,
      const UnboundMethod<ObserverType, Method, Params>& method) {

    // Check that this list still needs notifications.
    {
      base::AutoLock lock(list_lock_);
      typename ObserversListMap::iterator it =
          observer_lists_.find(MessageLoop::current());

      // The ObserverList could have been removed already.  In fact, it could
      // have been removed and then re-added!  If the master list's loop
      // does not match this one, then we do not need to finish this
      // notification.
      if (it == observer_lists_.end() || it->second != list)
        return;
    }

    {
      typename ObserverList<ObserverType>::Iterator it(*list);
      ObserverType* obs;
      while ((obs = it.GetNext()) != NULL)
        method.Run(obs);
    }

    // If there are no more observers on the list, we can now delete it.
    if (list->size() == 0) {
      {
        base::AutoLock lock(list_lock_);
        // Remove |list| if it's not already removed.
        // This can happen if multiple observers got removed in a notification.
        // See http://crbug.com/55725.
        typename ObserversListMap::iterator it =
            observer_lists_.find(MessageLoop::current());
        if (it != observer_lists_.end() && it->second == list)
          observer_lists_.erase(it);
      }
      delete list;
    }
  }

  typedef std::map<MessageLoop*, ObserverList<ObserverType>*> ObserversListMap;

  // These are marked mutable to facilitate having NotifyAll be const.
  base::Lock list_lock_;  // Protects the observer_lists_.
  ObserversListMap observer_lists_;
  const NotificationType type_;

  DISALLOW_COPY_AND_ASSIGN(ObserverListThreadSafe);
};

#endif  // BASE_OBSERVER_LIST_THREADSAFE_H_
