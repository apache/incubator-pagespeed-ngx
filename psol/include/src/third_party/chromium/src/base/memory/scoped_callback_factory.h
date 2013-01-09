// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ScopedCallbackFactory helps in cases where you wish to allocate a Callback
// (see base/callback.h), but need to prevent any pending callbacks from
// executing when your object gets destroyed.
//
// EXAMPLE:
//
//  void GatherDataAsynchronously(Callback1<Data>::Type* callback);
//
//  class MyClass {
//   public:
//    MyClass() : factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
//    }
//
//    void Process() {
//      GatherDataAsynchronously(factory_.NewCallback(&MyClass::GotData));
//    }
//
//   private:
//    void GotData(const Data& data) {
//      ...
//    }
//
//    base::ScopedCallbackFactory<MyClass> factory_;
//  };
//
// In the above example, the Process function calls GatherDataAsynchronously to
// kick off some asynchronous processing that upon completion will notify a
// callback.  If in the meantime, the MyClass instance is destroyed, when the
// callback runs, it will notice that the MyClass instance is dead, and it will
// avoid calling the GotData method.

#ifndef BASE_MEMORY_SCOPED_CALLBACK_FACTORY_H_
#define BASE_MEMORY_SCOPED_CALLBACK_FACTORY_H_

#include "base/callback_old.h"
#include "base/memory/weak_ptr.h"

namespace base {

template <class T>
class ScopedCallbackFactory {
 public:
  explicit ScopedCallbackFactory(T* obj) : weak_factory_(obj) {
  }

  typename Callback0::Type* NewCallback(
      void (T::*method)()) {
    return new CallbackImpl<void (T::*)(), Tuple0 >(
        weak_factory_.GetWeakPtr(), method);
  }

  template <typename Arg1>
  typename Callback1<Arg1>::Type* NewCallback(
      void (T::*method)(Arg1)) {
    return new CallbackImpl<void (T::*)(Arg1), Tuple1<Arg1> >(
        weak_factory_.GetWeakPtr(), method);
  }

  template <typename Arg1, typename Arg2>
  typename Callback2<Arg1, Arg2>::Type* NewCallback(
      void (T::*method)(Arg1, Arg2)) {
    return new CallbackImpl<void (T::*)(Arg1, Arg2), Tuple2<Arg1, Arg2> >(
        weak_factory_.GetWeakPtr(), method);
  }

  template <typename Arg1, typename Arg2, typename Arg3>
  typename Callback3<Arg1, Arg2, Arg3>::Type* NewCallback(
      void (T::*method)(Arg1, Arg2, Arg3)) {
    return new CallbackImpl<void (T::*)(Arg1, Arg2, Arg3),
                            Tuple3<Arg1, Arg2, Arg3> >(
        weak_factory_.GetWeakPtr(), method);
  }

  template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
  typename Callback4<Arg1, Arg2, Arg3, Arg4>::Type* NewCallback(
      void (T::*method)(Arg1, Arg2, Arg3, Arg4)) {
    return new CallbackImpl<void (T::*)(Arg1, Arg2, Arg3, Arg4),
                            Tuple4<Arg1, Arg2, Arg3, Arg4> >(
        weak_factory_.GetWeakPtr(), method);
  }

  template <typename Arg1, typename Arg2, typename Arg3, typename Arg4,
            typename Arg5>
  typename Callback5<Arg1, Arg2, Arg3, Arg4, Arg5>::Type* NewCallback(
      void (T::*method)(Arg1, Arg2, Arg3, Arg4, Arg5)) {
    return new CallbackImpl<void (T::*)(Arg1, Arg2, Arg3, Arg4, Arg5),
                            Tuple5<Arg1, Arg2, Arg3, Arg4, Arg5> >(
        weak_factory_.GetWeakPtr(), method);
  }

  void RevokeAll() { weak_factory_.InvalidateWeakPtrs(); }
  bool HasPendingCallbacks() const { return weak_factory_.HasWeakPtrs(); }

 private:
  template <typename Method>
  class CallbackStorage {
   public:
    CallbackStorage(const WeakPtr<T>& obj, Method meth)
        : obj_(obj),
          meth_(meth) {
    }

   protected:
    WeakPtr<T> obj_;
    Method meth_;
  };

  template <typename Method, typename Params>
  class CallbackImpl : public CallbackStorage<Method>,
                       public CallbackRunner<Params> {
   public:
    CallbackImpl(const WeakPtr<T>& obj, Method meth)
        : CallbackStorage<Method>(obj, meth) {
    }
    virtual void RunWithParams(const Params& params) {
      // Use "this->" to force C++ to look inside our templatized base class;
      // see Effective C++, 3rd Ed, item 43, p210 for details.
      if (!this->obj_)
        return;
      DispatchToMethod(this->obj_.get(), this->meth_, params);
    }
  };

  WeakPtrFactory<T> weak_factory_;
};

}  // namespace base

#endif  // BASE_MEMORY_SCOPED_CALLBACK_FACTORY_H_
