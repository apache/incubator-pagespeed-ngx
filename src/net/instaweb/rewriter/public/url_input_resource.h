/**
 * Copyright 2010 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)
//
// Input resource created based on a network resource.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_URL_INPUT_RESOURCE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_URL_INPUT_RESOURCE_H_

#include "base/basictypes.h"
#include "net/instaweb/rewriter/public/resource.h"

namespace net_instaweb {

class MessageHandler;
class ResponseHeaders;
class RewriteOptions;
class UrlFetcher;

class UrlInputResource : public Resource {
 public:
  UrlInputResource(ResourceManager* manager,
                   const RewriteOptions* options,
                   const ContentType* type,
                   const StringPiece& url)
      : Resource(manager, type),
        url_(url.data(), url.size()),
        rewrite_options_(options) {
  }
  virtual ~UrlInputResource();

  virtual std::string url() const { return url_; }
  const RewriteOptions* rewrite_options() const { return rewrite_options_; }

  void Freshen(MessageHandler* handler);

 protected:
  virtual bool Load(MessageHandler* message_handler);
  virtual void LoadAndCallback(AsyncCallback* callback,
                               MessageHandler* message_handler);

 private:
  std::string url_;
  const RewriteOptions* rewrite_options_;

  DISALLOW_COPY_AND_ASSIGN(UrlInputResource);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_URL_INPUT_RESOURCE_H_
