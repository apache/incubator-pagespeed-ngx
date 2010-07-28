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

#include "net/instaweb/rewriter/public/url_input_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/url_fetcher.h"

namespace net_instaweb {

UrlInputResource::~UrlInputResource() {
}

bool UrlInputResource::Read(MessageHandler* message_handler) {
  bool ret = true;
  if (!loaded()) {
    // TODO(jmarantz): consider request_headers.  E.g. will we ever
    // get different resources depending on user-agent?
    SimpleMetaData request_headers;
    ret = resource_manager_->url_fetcher()->StreamingFetchUrl(
        url_, request_headers, &meta_data_, &value_, message_handler);
    if (ret) {
      value_.SetHeaders(meta_data_);
    }
  }
  return ret;
}

}  // namespace net_instaweb
