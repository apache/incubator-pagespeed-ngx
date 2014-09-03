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

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/cacheable_resource_base.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"  // for GoogleString
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {
struct ContentType;
class RequestHeaders;
class RewriteDriver;
class Statistics;

class UrlInputResource : public CacheableResourceBase {
 public:
  // Created only from RewriteDriver::CreateInputResource*
  virtual ~UrlInputResource();

  static void InitStats(Statistics* stats);

 private:
  friend class RewriteDriver;
  friend class UrlInputResourceTest;
  UrlInputResource(RewriteDriver* rewrite_driver,
                   const ContentType* type,
                   const StringPiece& url,
                   bool is_authorized_domain);

  virtual void PrepareRequest(const RequestContextPtr& request_context,
                              RequestHeaders* headers);

  // If the resource is from a domain that is not explicitly authorized,
  // the domain for the resource is stored in origin_ by the constructor
  // so that when PrepareRequest is eventually called, this domain can be
  // temporarily authorized for fetching purposes. Note that this is done
  // to support inlining of unauthorized resources into the HTML, which is
  // considered to be a safe action.
  GoogleString origin_;

  DISALLOW_COPY_AND_ASSIGN(UrlInputResource);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_URL_INPUT_RESOURCE_H_
