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
//         jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/content_type.h"

namespace net_instaweb {

const int64 Resource::kDefaultExpireTimeMs = 5 * 60 * 1000;

Resource::Resource(RewriteDriver* driver, const ContentType* type)
    : driver_(driver),
      resource_manager_(driver->resource_manager()),
      type_(type) {
}

Resource::~Resource() {
}

int64 Resource::CacheExpirationTimeMs() const {
  int64 input_expire_time_ms = kDefaultExpireTimeMs;
  if (meta_data_.IsCacheable()) {
    input_expire_time_ms = meta_data_.CacheExpirationTimeMs();
  }
  return input_expire_time_ms;
}

// Note: OutputResource overrides this to also set the file extension.
void Resource::SetType(const ContentType* type) {
  type_ = type;
}

void Resource::DetermineContentType() {
  // Try to determine the content type from the URL extension, or
  // the response headers.
  StringStarVector content_types;
  ResponseHeaders* headers = metadata();
  const ContentType* content_type = NULL;
  if (headers->Lookup("Content-type", &content_types)) {
    for (int i = 0, n = content_types.size(); (i < n) && (content_type == NULL);
         ++i) {
      if (content_types[i] != NULL) {
        content_type = MimeTypeToContentType(*(content_types[i]));
      }
    }
  }

  if (content_type == NULL) {
    // If there is no content type in input headers, then try to
    // determine it from the name.
    GoogleString trimmed_url;
    TrimWhitespace(url(), &trimmed_url);
    content_type = NameExtensionToContentType(trimmed_url);
  }
  if (content_type != NULL) {
    SetType(content_type);
  }
}

// Default, blocking implementation which calls Load.
// Resources which can fetch asynchronously should override this.
void Resource::LoadAndCallback(AsyncCallback* callback,
                               MessageHandler* message_handler) {
  callback->Done(Load(message_handler), this);
}

Resource::AsyncCallback::~AsyncCallback() {
}

bool Resource::Link(HTTPValue* value, MessageHandler* handler) {
  SharedString* contents_and_headers = value->share();
  return value_.Link(contents_and_headers, &meta_data_, handler);
}

bool Resource::IsCacheable() const {
  return true;
}

void Resource::Freshen(MessageHandler* handler) {
  // We don't need Freshining for data urls or output resources.
}

}  // namespace net_instaweb
