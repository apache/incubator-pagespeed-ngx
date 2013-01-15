// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_H_
#define BASE_TASK_H_
#pragma once

#include "base/base_api.h"
#include "base/debug/alias.h"
#include "base/memory/raw_scoped_refptr_mismatch_checker.h"
#include "base/memory/weak_ptr.h"
#include "base/tracked.h"
#include "base/tuple.h"

namespace base {
const size_t kDeadTask = 0xDEAD7A53;
}

// Task ------------------------------------------------------------------------
//
// A task is a generic runnable thingy, usually used for running code on a
// different thread or for scheduling future tasks off of the message loop.

class BASE_API Task : public tracked_objects::Tracked {
 public:
  Task();
  virtual ~Task();

  // Tasks are automatically deleted after Run is called.
  virtual void Run() = 0;
};

class BASE_API CancelableTask : public Task {
 public:
  CancelableTask();
  virtual ~CancelableTask();

  // Not all tasks support cancellation.
  virtual void Cancel() = 0;
};

// Scoped Factories ------------------------------------------------------------
//
// These scoped factory objects can be used by non-refcounted objects to safely
// place tasks in a message loop.  Each factory guarantees that the tasks it
// produces will not run after the factory is destroyed.  Commonly, factories
// are declared as class members, so the class' tasks will automatically cancel
// when the class instance is destroyed.
//
// Exampe Usage:
//
// class MyClass {
//  private:
//   // This factory will be used to schedule invocations of SomeMethod.
//   ScopedRunnableMethodFactory<MyClass> some_method_factory_;
//
//  public:
//   // It is safe to suppress warning 4355 here.
//   MyClass() : ALLOW_THIS_IN_INITIALIZER_LIST(some_method_factory_(this)) { }
//
//   void SomeMethod() {
//     // If this function might be called directly, you might want to revoke
//     // any outstanding runnable methods scheduled to call it.  If it's not
//     // referenced other than by the factory, this is unnecessary.
//     some_method_factory_.RevokeAll();
//     ...
//   }
//
//   void ScheduleSomeMethod() {
//     // If you'd like to only only have one pending task at a time, test for
//     // |empty| before manufacturing another task.
//     if (!some_method_factory_.empty())
//       return;
//
//     // The factories are not thread safe, so always invoke on
//     // |MessageLoop::current()|.
//     MessageLoop::current()->PostDelayedTask(
//         FROM_HERE,
//         some_method_factory_.NewRunnableMethod(&MyClass::SomeMethod),
//         kSomeMethodDelayMS);
//   }
// };

// A ScopedRunnableMethodFactory creates runnable methods for a specified
// object.  This is particularly useful for generating callbacks for
// non-reference counted objects when the factory is a member of the object.
template<class T>
class ScopedRunnableMethodFactory {
 public:
  explicit ScopedRunnableMethodFactory(T* object) : weak_factory_(object) {
  }

  template <class Method>
  inline CancelableTask* NewRunnableMethod(Method method) {
    return new RunnableMethod<Method, Tuple0>(
        weak_factory_.GetWeakPtr(), method, MakeTuple());
  }

  template <class Method, class A>
  inline CancelableTask* NewRunnableMethod(Method method, const A& a) {
    return new RunnableMethod<Method, Tuple1<A> >(
        weak_factory_.GetWeakPtr(), method, MakeTuple(a));
  }

  template <class Method, class A, class B>
  inline CancelableTask* NewRunnableMethod(Method method, const A& a,
                                           const B& b) {
    return new RunnableMethod<Method, Tuple2<A, B> >(
        weak_factory_.GetWeakPtr(), method, MakeTuple(a, b));
  }

  template <class Method, class A, class B, class C>
  inline CancelableTask* NewRunnableMethod(Method method,
                                           const A& a,
                                           const B& b,
                                           const C& c) {
    return new RunnableMethod<Method, Tuple3<A, B, C> >(
        weak_factory_.GetWeakPtr(), method, MakeTuple(a, b, c));
  }

  template <class Method, class A, class B, class C, class D>
  inline CancelableTask* NewRunnableMethod(Method method,
                                           const A& a,
                                           const B& b,
                                           const C& c,
                                           const D& d) {
    return new RunnableMethod<Method, Tuple4<A, B, C, D> >(
        weak_factory_.GetWeakPtr(), method, MakeTuple(a, b, c, d));
  }

  template <class Method, class A, class B, class C, class D, class E>
  inline CancelableTask* NewRunnableMethod(Method method,
                                           const A& a,
                                           const B& b,
                                           const C& c,
                                           const D& d,
                                           const E& e) {
    return new RunnableMethod<Method, Tuple5<A, B, C, D, E> >(
        weak_factory_.GetWeakPtr(), method, MakeTuple(a, b, c, d, e));
  }

  void RevokeAll() { weak_factory_.InvalidateWeakPtrs(); }

  bool empty() const { return !weak_factory_.HasWeakPtrs(); }

 protected:
  template <class Method, class Params>
  class RunnableMethod : public CancelableTask {
   public:
    RunnableMethod(const base::WeakPtr<T>& obj,
                   Method meth,
                   const Params& params)
        : obj_(obj),
          meth_(meth),
          params_(params) {
      COMPILE_ASSERT(
          (base::internal::ParamsUseScopedRefptrCorrectly<Params>::value),
          badscopedrunnablemethodparams);
    }

    virtual void Run() {
      if (obj_)
        DispatchToMethod(obj_.get(), meth_, params_);
    }

    virtual void Cancel() {
      obj_.reset();
    }

   private:
    base::WeakPtr<T> obj_;
    Method meth_;
    Params params_;

    DISALLOW_COPY_AND_ASSIGN(RunnableMethod);
  };

 private:
  base::WeakPtrFactory<T> weak_factory_;
};

// General task implementations ------------------------------------------------

// Task to delete an object
template<class T>
class DeleteTask : public CancelableTask {
 public:
  explicit DeleteTask(const T* obj) : obj_(obj) {
  }
  virtual void Run() {
    delete obj_;
  }
  virtual void Cancel() {
    obj_ = NULL;
  }

 private:
  const T* obj_;
};

// Task to Release() an object
template<class T>
class ReleaseTask : public CancelableTask {
 public:
  explicit ReleaseTask(const T* obj) : obj_(obj) {
  }
  virtual void Run() {
    if (obj_)
      obj_->Release();
  }
  virtual void Cancel() {
    obj_ = NULL;
  }

 private:
  const T* obj_;
};

// RunnableMethodTraits --------------------------------------------------------
//
// This traits-class is used by RunnableMethod to manage the lifetime of the
// callee object.  By default, it is assumed that the callee supports AddRef
// and Release methods.  A particular class can specialize this template to
// define other lifetime management.  For example, if the callee is known to
// live longer than the RunnableMethod object, then a RunnableMethodTraits
// struct could be defined with empty RetainCallee and ReleaseCallee methods.
//
// The DISABLE_RUNNABLE_METHOD_REFCOUNT macro is provided as a convenient way
// for declaring a RunnableMethodTraits that disables refcounting.

template <class T>
struct RunnableMethodTraits {
  RunnableMethodTraits() {
#ifndef NDEBUG
    origin_thread_id_ = base::PlatformThread::CurrentId();
#endif
  }

  ~RunnableMethodTraits() {
#ifndef NDEBUG
    // If destroyed on a separate thread, then we had better have been using
    // thread-safe reference counting!
    if (origin_thread_id_ != base::PlatformThread::CurrentId())
      DCHECK(T::ImplementsThreadSafeReferenceCounting());
#endif
  }

  void RetainCallee(T* obj) {
#ifndef NDEBUG
    // Catch NewRunnableMethod being called in an object's constructor.  This
    // isn't safe since the method can be invoked before the constructor
    // completes, causing the object to be deleted.
    obj->AddRef();
    obj->Release();
#endif
    obj->AddRef();
  }

  void ReleaseCallee(T* obj) {
    obj->Release();
  }

 private:
#ifndef NDEBUG
  base::PlatformThreadId origin_thread_id_;
#endif
};

// Convenience macro for declaring a RunnableMethodTraits that disables
// refcounting of a class.  This is useful if you know that the callee
// will outlive the RunnableMethod object and thus do not need the ref counts.
//
// The invocation of DISABLE_RUNNABLE_METHOD_REFCOUNT should be done at the
// global namespace scope.  Example:
//
//   namespace foo {
//   class Bar {
//     ...
//   };
//   }  // namespace foo
//
//   DISABLE_RUNNABLE_METHOD_REFCOUNT(foo::Bar);
//
// This is different from DISALLOW_COPY_AND_ASSIGN which is declared inside the
// class.
#define DISABLE_RUNNABLE_METHOD_REFCOUNT(TypeName) \
  template <>                                      \
  struct RunnableMethodTraits<TypeName> {          \
    void RetainCallee(TypeName* manager) {}        \
    void ReleaseCallee(TypeName* manager) {}       \
  }

// RunnableMethod and RunnableFunction -----------------------------------------
//
// Runnable methods are a type of task that call a function on an object when
// they are run. We implement both an object and a set of NewRunnableMethod and
// NewRunnableFunction functions for convenience. These functions are
// overloaded and will infer the template types, simplifying calling code.
//
// The template definitions all use the following names:
// T                - the class type of the object you're supplying
//                    this is not needed for the Static version of the call
// Method/Function  - the signature of a pointer to the method or function you
//                    want to call
// Param            - the parameter(s) to the method, possibly packed as a Tuple
// A                - the first parameter (if any) to the method
// B                - the second parameter (if any) to the method
//
// Put these all together and you get an object that can call a method whose
// signature is:
//   R T::MyFunction([A[, B]])
//
// Usage:
// PostTask(FROM_HERE, NewRunnableMethod(object, &Object::method[, a[, b]])
// PostTask(FROM_HERE, NewRunnableFunction(&function[, a[, b]])

// RunnableMethod and NewRunnableMethod implementation -------------------------

template <class T, class Method, class Params>
class RunnableMethod : public CancelableTask {
 public:
  RunnableMethod(T* obj, Method meth, const Params& params)
      : obj_(obj), meth_(meth), params_(params) {
    traits_.RetainCallee(obj_);
    COMPILE_ASSERT(
        (base::internal::ParamsUseScopedRefptrCorrectly<Params>::value),
        badrunnablemethodparams);
  }

  ~RunnableMethod() {
    ReleaseCallee();
    obj_ = reinterpret_cast<T*>(base::kDeadTask);
  }

  virtual void Run() {
    if (obj_)
      DispatchToMethod(obj_, meth_, params_);
  }

  virtual void Cancel() {
    ReleaseCallee();
  }

 private:
  void ReleaseCallee() {
    T* obj = obj_;
    obj_ = NULL;
    if (obj)
      traits_.ReleaseCallee(obj);
  }

  T* obj_;
  Method meth_;
  Params params_;
  RunnableMethodTraits<T> traits_;
};

template <class T, class Method>
inline CancelableTask* NewRunnableMethod(T* object, Method method) {
  return new RunnableMethod<T, Method, Tuple0>(object, method, MakeTuple());
}

template <class T, class Method, class A>
inline CancelableTask* NewRunnableMethod(T* object, Method method, const A& a) {
  return new RunnableMethod<T, Method, Tuple1<A> >(object,
                                                   method,
                                                   MakeTuple(a));
}

template <class T, class Method, class A, class B>
inline CancelableTask* NewRunnableMethod(T* object, Method method,
const A& a, const B& b) {
  return new RunnableMethod<T, Method, Tuple2<A, B> >(object, method,
                                                      MakeTuple(a, b));
}

template <class T, class Method, class A, class B, class C>
inline CancelableTask* NewRunnableMethod(T* object, Method method,
                                          const A& a, const B& b, const C& c) {
  return new RunnableMethod<T, Method, Tuple3<A, B, C> >(object, method,
                                                         MakeTuple(a, b, c));
}

template <class T, class Method, class A, class B, class C, class D>
inline CancelableTask* NewRunnableMethod(T* object, Method method,
                                          const A& a, const B& b,
                                          const C& c, const D& d) {
  return new RunnableMethod<T, Method, Tuple4<A, B, C, D> >(object, method,
                                                            MakeTuple(a, b,
                                                                      c, d));
}

template <class T, class Method, class A, class B, class C, class D, class E>
inline CancelableTask* NewRunnableMethod(T* object, Method method,
                                          const A& a, const B& b,
                                          const C& c, const D& d, const E& e) {
  return new RunnableMethod<T,
                            Method,
                            Tuple5<A, B, C, D, E> >(object,
                                                    method,
                                                    MakeTuple(a, b, c, d, e));
}

template <class T, class Method, class A, class B, class C, class D, class E,
          class F>
inline CancelableTask* NewRunnableMethod(T* object, Method method,
                                          const A& a, const B& b,
                                          const C& c, const D& d, const E& e,
                                          const F& f) {
  return new RunnableMethod<T,
                            Method,
                            Tuple6<A, B, C, D, E, F> >(object,
                                                       method,
                                                       MakeTuple(a, b, c, d, e,
                                                                 f));
}

template <class T, class Method, class A, class B, class C, class D, class E,
          class F, class G>
inline CancelableTask* NewRunnableMethod(T* object, Method method,
                                         const A& a, const B& b,
                                         const C& c, const D& d, const E& e,
                                         const F& f, const G& g) {
  return new RunnableMethod<T,
                            Method,
                            Tuple7<A, B, C, D, E, F, G> >(object,
                                                          method,
                                                          MakeTuple(a, b, c, d,
                                                                    e, f, g));
}

template <class T, class Method, class A, class B, class C, class D, class E,
          class F, class G, class H>
inline CancelableTask* NewRunnableMethod(T* object, Method method,
                                         const A& a, const B& b,
                                         const C& c, const D& d, const E& e,
                                         const F& f, const G& g, const H& h) {
  return new RunnableMethod<T,
                            Method,
                            Tuple8<A, B, C, D, E, F, G, H> >(object,
                                                             method,
                                                             MakeTuple(a, b, c,
                                                                       d, e, f,
                                                                       g, h));
}

// RunnableFunction and NewRunnableFunction implementation ---------------------

template <class Function, class Params>
class RunnableFunction : public Task {
 public:
  RunnableFunction(Function function, const Params& params)
      : function_(function), params_(params) {
    COMPILE_ASSERT(
        (base::internal::ParamsUseScopedRefptrCorrectly<Params>::value),
        badrunnablefunctionparams);
  }

  ~RunnableFunction() {
    function_ = reinterpret_cast<Function>(base::kDeadTask);
  }

  virtual void Run() {
    // TODO(apatrick): Remove this ASAP. This ensures that the function pointer
    // is available in minidumps for the purpose of diagnosing
    // http://crbug.com/81449.
    Function function = function_;
    base::debug::Alias(&function);
    Params params = params_;
    base::debug::Alias(&params);

    if (function_)
      DispatchToFunction(function_, params_);
  }

 private:
  Function function_;
  Params params_;
};

template <class Function>
inline Task* NewRunnableFunction(Function function) {
  return new RunnableFunction<Function, Tuple0>(function, MakeTuple());
}

template <class Function, class A>
inline Task* NewRunnableFunction(Function function, const A& a) {
  return new RunnableFunction<Function, Tuple1<A> >(function, MakeTuple(a));
}

template <class Function, class A, class B>
inline Task* NewRunnableFunction(Function function, const A& a, const B& b) {
  return new RunnableFunction<Function, Tuple2<A, B> >(function,
                                                       MakeTuple(a, b));
}

template <class Function, class A, class B, class C>
inline Task* NewRunnableFunction(Function function, const A& a, const B& b,
                                 const C& c) {
  return new RunnableFunction<Function, Tuple3<A, B, C> >(function,
                                                          MakeTuple(a, b, c));
}

template <class Function, class A, class B, class C, class D>
inline Task* NewRunnableFunction(Function function, const A& a, const B& b,
                                 const C& c, const D& d) {
  return new RunnableFunction<Function, Tuple4<A, B, C, D> >(function,
                                                             MakeTuple(a, b,
                                                                       c, d));
}

template <class Function, class A, class B, class C, class D, class E>
inline Task* NewRunnableFunction(Function function, const A& a, const B& b,
                                 const C& c, const D& d, const E& e) {
  return new RunnableFunction<Function, Tuple5<A, B, C, D, E> >(function,
                                                                MakeTuple(a, b,
                                                                          c, d,
                                                                          e));
}

template <class Function, class A, class B, class C, class D, class E,
          class F>
inline Task* NewRunnableFunction(Function function, const A& a, const B& b,
                                 const C& c, const D& d, const E& e,
                                 const F& f) {
  return new RunnableFunction<Function, Tuple6<A, B, C, D, E, F> >(function,
      MakeTuple(a, b, c, d, e, f));
}

template <class Function, class A, class B, class C, class D, class E,
          class F, class G>
inline Task* NewRunnableFunction(Function function, const A& a, const B& b,
                                 const C& c, const D& d, const E& e, const F& f,
                                 const G& g) {
  return new RunnableFunction<Function, Tuple7<A, B, C, D, E, F, G> >(function,
      MakeTuple(a, b, c, d, e, f, g));
}

template <class Function, class A, class B, class C, class D, class E,
          class F, class G, class H>
inline Task* NewRunnableFunction(Function function, const A& a, const B& b,
                                 const C& c, const D& d, const E& e, const F& f,
                                 const G& g, const H& h) {
  return new RunnableFunction<Function, Tuple8<A, B, C, D, E, F, G, H> >(
      function, MakeTuple(a, b, c, d, e, f, g, h));
}

namespace base {

// ScopedTaskRunner is akin to scoped_ptr for Tasks.  It ensures that the Task
// is executed and deleted no matter how the current scope exits.
class BASE_API ScopedTaskRunner {
 public:
  // Takes ownership of the task.
  explicit ScopedTaskRunner(Task* task);
  ~ScopedTaskRunner();

  Task* Release();

 private:
  Task* task_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedTaskRunner);
};

}  // namespace base

#endif  // BASE_TASK_H_
