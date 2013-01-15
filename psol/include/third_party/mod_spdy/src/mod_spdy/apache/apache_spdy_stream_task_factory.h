// Copyright 2011 Google Inc.
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

#ifndef MOD_SPDY_APACHE_APACHE_SPDY_STREAM_TASK_FACTORY_H_
#define MOD_SPDY_APACHE_APACHE_SPDY_STREAM_TASK_FACTORY_H_

#include "httpd.h"

#include "base/basictypes.h"
#include "mod_spdy/apache/slave_connection.h"
#include "mod_spdy/common/spdy_stream_task_factory.h"

namespace net_instaweb { class Function; }

namespace mod_spdy {

class SpdyStream;

class ApacheSpdyStreamTaskFactory : public SpdyStreamTaskFactory {
 public:
  explicit ApacheSpdyStreamTaskFactory(conn_rec* connection);
  ~ApacheSpdyStreamTaskFactory();

  // This must be called from hooks registration to create the filters
  // this class needs to route bytes between Apache & mod_spdy.
  static void InitFilters();

  // SpdyStreamTaskFactory methods:
  virtual net_instaweb::Function* NewStreamTask(SpdyStream* stream);

 private:
  SlaveConnectionFactory connection_factory_;

  DISALLOW_COPY_AND_ASSIGN(ApacheSpdyStreamTaskFactory);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_APACHE_APACHE_SPDY_STREAM_TASK_FACTORY_H_
