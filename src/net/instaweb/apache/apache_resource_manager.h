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
//
// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_APACHE_APACHE_RESOURCE_MANAGER_H_
#define NET_INSTAWEB_APACHE_APACHE_RESOURCE_MANAGER_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/resource_manager.h"

struct apr_pool_t;
struct server_rec;

namespace net_instaweb {

class ApacheConfig;
class ApacheMessageHandler;
class ApacheRewriteDriverFactory;
class HTTPCache;
class RewriteStats;
class SharedMemStatistics;
class ThreadSystem;
class UrlPollableAsyncFetcher;

// Creates an Apache-specific ResourceManager.  This differs from base class
// that it incorporates by adding per-VirtualHost configuration, including:
//    - file-cache path & limits
//    - default RewriteOptions.
// Additionally, there are startup semantics for apache's prefork model
// that require a phased initialization.
class ApacheResourceManager : public ResourceManager {
 public:
  ApacheResourceManager(ApacheRewriteDriverFactory* factory,
                        server_rec* server,
                        const StringPiece& version);
  virtual ~ApacheResourceManager();

  GoogleString hostname_identifier() { return hostname_identifier_; }
  void SetStatistics(SharedMemStatistics* x);
  ApacheRewriteDriverFactory* apache_factory() { return apache_factory_; }
  ApacheConfig* config();
  bool InitFileCachePath();

  // Should be called after the child process is forked.
  void ChildInit();

  UrlPollableAsyncFetcher* subresource_fetcher() {
    return subresource_fetcher_;
  }

  bool initialized() const { return initialized_; }

  // Called on notification from Apache on child exit. Returns true
  // if this is the last ResourceManager that exists.
  bool PoolDestroyed();

 private:
  ApacheRewriteDriverFactory* apache_factory_;
  server_rec* server_rec_;
  GoogleString version_;

  // hostname_identifier_ equals to "server_hostname:port" of Apache,
  // it's used to distinguish the name of shared memory,
  // so that each vhost has its own SharedCircularBuffer.
  GoogleString hostname_identifier_;

  bool initialized_;

  // A pollable fetcher provides a Poll() to wait for outstanding
  // fetches to complete.  This is used in
  // instaweb_handler.cc:handle_as_resource() to block the apache
  // request thread until the requested resource has been delivered.
  //
  // TODO(jmarantz): use the scheduler & condition variables to
  // accomplish this instead.
  UrlPollableAsyncFetcher* subresource_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ApacheResourceManager);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_RESOURCE_MANAGER_H_
