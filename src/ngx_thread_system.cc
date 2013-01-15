/*
 * Copyright 2013 Google Inc.
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

// Author: jefftk@google.com (Jeff Kaufman)

#include "ngx_thread_system.h"

#include "apr_thread_proc.h"

namespace net_instaweb {

class Timer;

NgxThreadSystem::NgxThreadSystem() : may_start_threads_(false) {}

NgxThreadSystem::~NgxThreadSystem() {}

void NgxThreadSystem::PermitThreadStarting() {
  CHECK(!may_start_threads_);
  may_start_threads_ = true;
}

void NgxThreadSystem::BeforeThreadRunHook() {
  // We disable all signals here, since the nginx worker process is expecting to
  // catch them and pagespeed doesn't use signals.
  apr_setup_signal_thread();

  // If this fails you can get a backtrace from gdb by setting a breakpoint on
  // "pthread_create".
  CHECK(may_start_threads_);
}

}  // namespace net_instaweb
