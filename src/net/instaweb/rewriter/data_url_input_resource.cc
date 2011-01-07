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

// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/data_url_input_resource.h"

#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace net_instaweb {

bool DataUrlInputResource::Load(MessageHandler* message_handler) {
  if (DecodeDataUrlContent(encoding_, encoded_contents_,
                           &decoded_contents_) &&
      value_.Write(decoded_contents_, message_handler)) {
    resource_manager_->SetDefaultHeaders(type_, &meta_data_);
    value_.SetHeaders(meta_data_);
  }
  return loaded();
}

bool DataUrlInputResource::IsCacheable() const {
  return false;
}

}
