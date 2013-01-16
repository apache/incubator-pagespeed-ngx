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

// Author: jmarantz@google.com (Joshua Marantz)
//
// Captures the Apache request_rec* in our request context.

#ifndef NET_INSTAWEB_APACHE_APACHE_REQUEST_CONTEXT_H_
#define NET_INSTAWEB_APACHE_APACHE_REQUEST_CONTEXT_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/http/public/request_context.h"

struct request_rec;

namespace net_instaweb {

class AbstractMutex;

class ApacheRequestContext : public RequestContext {
 public:
  ApacheRequestContext(AbstractMutex* logging_mutex, request_rec* req);

  request_rec* apache_request() { return apache_request_; }
  virtual const char* class_name() const;

  // Returns rc as an ApacheRequestContext* if it is one, nor NULL if it's not.
  static ApacheRequestContext* DynamicCast(RequestContext* rc);

 protected:
  virtual ~ApacheRequestContext();

 private:
  request_rec* apache_request_;

  DISALLOW_COPY_AND_ASSIGN(ApacheRequestContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_REQUEST_CONTEXT_H_
