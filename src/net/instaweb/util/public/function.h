/*
 * Copyright 2011 Google Inc.
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_FUNCTION_H_
#define NET_INSTAWEB_UTIL_PUBLIC_FUNCTION_H_

#include "net/instaweb/util/public/atomic_bool.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

// Encapsulates a task to be run in response to some event, such as
// a Timer callback, an fetch, or a cache lookup.
class Function {
 public:
  Function();
  virtual ~Function();

  // Callers must override this to define the action to take when a closure
  // is run.  If this is called, Cancel() should not be called.  This is
  // a convention that's expected of callers of Function objects, but is
  // not enforced by the Function implementation.
  virtual void Run() = 0;

  // Allows an infrastructure (e.g. Worker or Alarm) to request that
  // a running Function stop soon, as it is being shut down.
  bool quit_requested() const {
    return quit_requested_.value();
  }

  // Requests that a running closure shut down.
  void set_quit_requested(bool q) {
    quit_requested_.set_value(q);
  }

  // Informs a the Function that it is being shut down.  If this is
  // called, Run() should not be called.  This should never be called
  // while a function is running.  See also set_quit_requested(),
  // which can be called during Run(), so that Run() implementations
  // can check quit_requested() at their convenience to stop the
  // operation in progress.
  virtual void Cancel() {}

 private:
  AtomicBool quit_requested_;
  DISALLOW_COPY_AND_ASSIGN(Function);
};

// A Macro is recommended for making a readable call to a pointer-to-member
// function per section 33.6 of
// http://www.parashift.com/c++-faq-lite/pointers-to-members.html
#define CALL_MEMBER_FN(object, ptrToMember) ((this->object)->*(ptrToMember))

// Base class for a MemberFunctionN classes, implementing an optional Cancel
// via a Member function.
template<class C>
class MemberFunctionBase : public Function {
 public:
  typedef void (C::*CancelFunc)();

  // base-class ctor variant without a cancel method.
  explicit MemberFunctionBase(C* c) : c_(c), has_cancel_(false) {}

  // base-class ctor variant with a cancel method.
  MemberFunctionBase(C* c, CancelFunc cancel)
      : c_(c), cancel_(cancel), has_cancel_(true) {}

  virtual void Cancel() {
    if (has_cancel_) {
      CALL_MEMBER_FN(c_, cancel_)();
    }
  }

 protected:
  C* c_;

 private:
  CancelFunc cancel_;
  bool has_cancel_;
};

// Captures a delayed call to a 0-arg member function as a closure.
template<class C>
class MemberFunction0 : public MemberFunctionBase<C> {
 public:
  typedef void (C::*Func)();

  // Constructor suppying a Run method, but no Cancel method.
  MemberFunction0(Func f, C* c) : MemberFunctionBase<C>(c), f_(f) {}

  // Constructor suppying a Run method and a Cancel method.
  MemberFunction0(Func f,
                  typename MemberFunctionBase<C>::CancelFunc cancel,
                  C* c)
      : MemberFunctionBase<C>(c, cancel), f_(f) {}
  virtual void Run() { CALL_MEMBER_FN(c_, f_)(); }

 private:
  Func f_;
};

// Captures a delayed call to a 1-arg member function as a closure.
template<class C, typename T1>
class MemberFunction1 : public MemberFunctionBase<C> {
 public:
  typedef void (C::*Func)(T1);

  // Constructor suppying a Run method, but no Cancel method.
  MemberFunction1(Func f, C* c, T1 v1)
      : MemberFunctionBase<C>(c), f_(f), v1_(v1) {}

  // Constructor suppying a Run method and a Cancel method.
  MemberFunction1(Func f,
                  typename MemberFunctionBase<C>::CancelFunc cancel,
                  C* c, T1 v1)
      : MemberFunctionBase<C>(c, cancel), f_(f), v1_(v1)  {}
  virtual void Run() { CALL_MEMBER_FN(c_, f_)(v1_); }
 private:
  Func f_;
  T1 v1_;
};

// Captures a delayed call to a 2-arg member function as a closure.
template<class C, typename T1, typename T2>
class MemberFunction2 : public MemberFunctionBase<C> {
 public:
  typedef void (C::*Func)(T1, T2);

  // Constructor suppying a Run method, but no Cancel method.
  MemberFunction2(Func f, C* c, T1 v1, T2 v2)
      : MemberFunctionBase<C>(c), f_(f), v1_(v1), v2_(v2) {}

  // Constructor suppying a Run method and a Cancel method.
  MemberFunction2(Func f,
                  typename MemberFunctionBase<C>::CancelFunc cancel,
                  C* c, T1 v1, T2 v2)
      : MemberFunctionBase<C>(c, cancel), f_(f), v1_(v1), v2_(v2)  {}
  virtual void Run() { CALL_MEMBER_FN(c_, f_)(v1_, v2_); }
 private:
  Func f_;
  T1 v1_;
  T2 v2_;
};

// Captures a delayed call to a 3-arg member function as a closure.
template<class C, typename T1, typename T2, typename T3>
class MemberFunction3 : public MemberFunctionBase<C> {
 public:
  typedef void (C::*Func)(T1, T2, T3);

  // Constructor suppying a Run method, but no Cancel method.
  MemberFunction3(Func f, C* c, T1 v1, T2 v2, T3 v3)
      : MemberFunctionBase<C>(c), f_(f), v1_(v1), v2_(v2), v3_(v3) {}

  // Constructor suppying a Run method and a Cancel method.
  MemberFunction3(Func f,
                  typename MemberFunctionBase<C>::CancelFunc cancel,
                  C* c, T1 v1, T2 v2, T3 v3)
      : MemberFunctionBase<C>(c, cancel), f_(f), v1_(v1), v2_(v2), v3_(v3)  {}
  virtual void Run() { CALL_MEMBER_FN(c_, f_)(v1_, v2_, v3_); }
 private:
  Func f_;
  T1 v1_;
  T2 v2_;
  T3 v3_;
};

#undef CALL_MEMBER_FN

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_FUNCTION_H_
