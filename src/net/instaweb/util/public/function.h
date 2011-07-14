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

#include "net/instaweb/util/public/atomicops.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

// Encapsulates a task to be run in response to some event, such as
// a Timer callback, an fetch, or a cache lookup.
class Function {
 public:
  Function();
  virtual ~Function();

  // Callers must override this to define the action to take when a closure
  // is run.
  virtual void Run() = 0;

  // Allows an infrastructure (e.g. Worker or Alarm) to request that
  // a running Function stop soon, as it is being shut down.
  bool quit_requested() const {
    return base::subtle::Acquire_Load(&quit_requested_);
  }

  // Requests that a running closure shut down.
  void set_quit_requested(bool q) {
    base::subtle::Release_Store(&quit_requested_, q);
  }

 private:
  base::subtle::AtomicWord quit_requested_;
  DISALLOW_COPY_AND_ASSIGN(Function);
};

// A Macro is recommended for making a readable call to a pointer-to-member
// function per section 33.6 of
// http://www.parashift.com/c++-faq-lite/pointers-to-members.html
#define CALL_MEMBER_FN(object, ptrToMember) ((object)->*(ptrToMember))

// Captures a delayed call to a 0-arg member function as a closure.
template<class C>
class MemberFunction0 : public Function {
 public:
  typedef void (C::*Func)();

  MemberFunction0(Func f, C* c) : f_(f), c_(c) {}
  virtual void Run() { CALL_MEMBER_FN(c_, f_)(); }
 private:
  Func f_;
  C* c_;
};

// Captures a delayed call to a 1-arg member function as a closure.
template<class C, typename T1>
class MemberFunction1 : public Function {
 public:
  typedef void (C::*Func)(T1);

  MemberFunction1(Func f, C* c, T1 v1) : f_(f), c_(c), v1_(v1) {}
  virtual void Run() { CALL_MEMBER_FN(c_, f_)(v1_); }
 private:
  Func f_;
  C* c_;
  T1 v1_;
};

// Captures a delayed call to a 2-arg member function as a closure.
template<class C, typename T1, typename T2>
class MemberFunction2 : public Function {
 public:
  typedef void (C::*Func)(T1, T2);

  MemberFunction2(Func f, C* c, T1 v1, T2 v2)
      : f_(f), c_(c), v1_(v1), v2_(v2) {}
  virtual void Run() { CALL_MEMBER_FN(c_, f_)(v1_, v2_); }
 private:
  Func f_;
  C* c_;
  T1 v1_;
  T2 v2_;
};

// Captures a delayed call to a 3-arg member function as a closure.
template<class C, typename T1, typename T2, typename T3>
class MemberFunction3 : public Function {
 public:
  typedef void (C::*Func)(T1, T2, T3);

  MemberFunction3(Func f, C* c, T1 v1, T2 v2, T3 v3)
      : f_(f), c_(c), v1_(v1), v2_(v2), v3_(v3) {}
  virtual void Run() { CALL_MEMBER_FN(c_, f_)(v1_, v2_, v3_); }
 private:
  Func f_;
  C* c_;
  T1 v1_;
  T2 v2_;
  T3 v3_;
};

#undef CALL_MEMBER_FN

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_FUNCTION_H_
