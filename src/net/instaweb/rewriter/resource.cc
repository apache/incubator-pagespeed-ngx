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

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"  // for HttpAttributes, etc
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
class MessageHandler;
class SharedString;

namespace {

const int64 kNotCacheable = 0;

}  // namespace

Resource::Resource(ResourceManager* resource_manager, const ContentType* type)
    : resource_manager_(resource_manager),
      type_(type) {
}

Resource::~Resource() {
}

bool Resource::IsValidAndCacheable() const {
  return ((response_headers_.status_code() == HttpStatus::kOK) &&
          !resource_manager_->http_cache()->IsAlreadyExpired(
              response_headers_));
}

GoogleString Resource::ContentsHash() const {
  DCHECK(IsValidAndCacheable());
  return resource_manager_->contents_hasher()->Hash(contents());
}

void Resource::AddInputInfoToPartition(int index, CachedResult* partition) {
  InputInfo* input = partition->add_input();
  input->set_index(index);
  // FillInPartitionInputInfo can be specialized based on resource type.
  FillInPartitionInputInfo(input);
}

// Default version.
void Resource::FillInPartitionInputInfo(InputInfo* input) {
  CHECK(loaded());
  input->set_type(InputInfo::CACHED);
  input->set_last_modified_time_ms(response_headers_.last_modified_time_ms());
  input->set_expiration_time_ms(response_headers_.CacheExpirationTimeMs());
  input->set_date_ms(response_headers_.date_ms());
  // TODO(morlovich) This is wasteful for the cache extender, is there
  // a way of avoiding this other than passing a param all over the place?
  if (IsValidAndCacheable()) {
    input->set_input_content_hash(ContentsHash());
  }
}

int64 Resource::CacheExpirationTimeMs() const {
  int64 input_expire_time_ms = kNotCacheable;
  if (response_headers_.IsCacheable()) {
    input_expire_time_ms = response_headers_.CacheExpirationTimeMs();
  }
  return input_expire_time_ms;
}

// Note: OutputResource overrides this to also set the file extension.
void Resource::SetType(const ContentType* type) {
  type_ = type;
}

// Try to determine the content type from the URL extension, or
// the response headers.
void Resource::DetermineContentType() {
  // First try the HTTP headers, the definitive source of Content-Type.
  const ContentType* content_type = response_headers()->DetermineContentType();
  // If there is no content type in headers, then guess from extension.
  if (content_type == NULL) {
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
  callback->Done(Load(message_handler));
}

Resource::AsyncCallback::~AsyncCallback() {
}

bool Resource::Link(HTTPValue* value, MessageHandler* handler) {
  SharedString* contents_and_headers = value->share();
  return value_.Link(contents_and_headers, &response_headers_, handler);
}

void Resource::Freshen(MessageHandler* handler) {
  // We don't need Freshining for data urls or output resources.
}

}  // namespace net_instaweb
