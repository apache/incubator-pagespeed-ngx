/*
 * Copyright 2015 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RENDER_BLOCKING_HTML_COMPUTATION_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RENDER_BLOCKING_HTML_COMPUTATION_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class RewriteDriver;

// This class helps one perform a background computation based on an HTML
// webpage at a given URL, while blocking rendering on parent RewriteDriver
// until the computation succeeds. Basic usage is as follows:
// 1) Subclass this, overriding two methods:
//    a) SetupFilters to add filters to child_driver.
//    b) Done() to pass result on to the client.
// 2) In the parent, create a new object for the computation, call Compute(),
//    and use result it passed from its Done in RenderDone(). The object will
//    self-delete.
class RenderBlockingHtmlComputation {
 public:
  explicit RenderBlockingHtmlComputation(RewriteDriver* parent_driver);
  virtual ~RenderBlockingHtmlComputation();

  // Tries to fetch a webpage on url. If fetching is successful, it will
  // create a new RewriteDriver child_driver with same options as parent_driver
  // but no filters, call SetupFilters on it, then pass the document through it,
  // then call Done(true). If something fails before that, Done(false) will be
  // called instead. this object will be deleted after the call completes.
  //
  // Rendering on the rewrite driver will be disabled until Done() is invoked.
  void Compute(const GoogleString& url);

 protected:
  virtual void SetupFilters(RewriteDriver* child_driver) = 0;

  // Override this to extract and save the computation result. The object
  // will be deleted after this returns.
  //
  // Warning: this method can run in a variety of threads, so make sure you
  // properly lock access to data this updates.
  virtual void Done(bool success) = 0;

 private:
  class ResourceCallback;
  void ReportResult(bool success);

  RewriteDriver* parent_driver_;

  DISALLOW_COPY_AND_ASSIGN(RenderBlockingHtmlComputation);
};


}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RENDER_BLOCKING_HTML_COMPUTATION_H_
