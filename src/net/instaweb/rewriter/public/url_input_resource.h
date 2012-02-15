/*
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

#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
struct ContentType;
class MessageHandler;
class RewriteDriver;

class UrlInputResource : public Resource {
 public:
  UrlInputResource(RewriteDriver* rewrite_driver,
                   const RewriteOptions* options,
                   const ContentType* type,
                   const StringPiece& url);
  virtual ~UrlInputResource();

  virtual bool IsValidAndCacheable() const;
  virtual GoogleString url() const { return url_; }
  virtual const RewriteOptions* rewrite_options() const {
    return rewrite_options_;
  }

  // Note that this only updates the resource contents in cache and does not
  // update the metadata that associates an input resource with a rewritten
  // resource.
  virtual void Freshen(MessageHandler* handler);

 protected:
  virtual bool Load(MessageHandler* message_handler);
  virtual void LoadAndCallback(NotCacheablePolicy not_cacheable_policy,
                               AsyncCallback* callback,
                               MessageHandler* message_handler);

 private:
  GoogleString url_;
  // This may be NULL. However, it should always be set if we freshen the
  // resource.
  RewriteDriver* rewrite_driver_;
  const RewriteOptions* rewrite_options_;
  bool respect_vary_;

  DISALLOW_COPY_AND_ASSIGN(UrlInputResource);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_URL_INPUT_RESOURCE_H_
