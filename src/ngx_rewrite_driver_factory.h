/*
 * Copyright 2012 Google Inc.
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

#ifndef NGX_REWRITE_DRIVER_FACTORY_H_
#define NGX_REWRITE_DRIVER_FACTORY_H_

#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_gflags.h"
#include "net/instaweb/util/public/simple_stats.h"

namespace net_instaweb {

class NgxRewriteDriverFactory : public RewriteDriverFactory {
 public:
  NgxRewriteDriverFactory();
  virtual ~NgxRewriteDriverFactory();
  virtual Hasher* NewHasher();
  virtual UrlFetcher* DefaultUrlFetcher();
  virtual UrlAsyncFetcher* DefaultAsyncUrlFetcher();
  virtual MessageHandler* DefaultHtmlParseMessageHandler();
  virtual MessageHandler* DefaultMessageHandler();
  virtual FileSystem* DefaultFileSystem();
  virtual Timer* DefaultTimer();
  virtual void SetupCaches(ServerContext* resource_manager);
  virtual Statistics* statistics();

 private:
  SimpleStats simple_stats_;
  const RewriteGflags gflags_;
  
  DISALLOW_COPY_AND_ASSIGN(NgxRewriteDriverFactory);
};

} // namespace net_instaweb

#endif  // NGX_REWRITE_DRIVER_FACTORY_H_
