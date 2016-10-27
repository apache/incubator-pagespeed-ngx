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

#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/input_info.pb.h"  // for InputInfo, etc
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"  // for HttpAttributes, etc
#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/gzip_inflater.h"

namespace net_instaweb {

class MessageHandler;

namespace {

const int64 kNotCacheable = 0;

}  // namespace

Resource::Resource(const RewriteDriver* driver, const ContentType* type)
    : server_context_(driver->server_context()),
      type_(type),
      response_headers_(driver->options()->ComputeHttpOptions()),
      fetch_response_status_(kFetchStatusNotSet),
      is_background_fetch_(true),
      enable_cache_purge_(false),
      proactive_resource_freshening_(false),
      disable_rewrite_on_no_transform_(true),
      is_authorized_domain_(true),
      respect_vary_(ResponseHeaders::kRespectVaryOnResources),
      extracted_(false) {
}

Resource::Resource() : server_context_(NULL), type_(NULL),
                       response_headers_(kDefaultHttpOptionsForTests),
                       fetch_response_status_(kFetchStatusNotSet),
                       is_background_fetch_(true),
                       enable_cache_purge_(false),
                       proactive_resource_freshening_(false),
                       disable_rewrite_on_no_transform_(true),
                       is_authorized_domain_(true),
                       respect_vary_(ResponseHeaders::kRespectVaryOnResources),
                       extracted_(false) {
}

Resource::~Resource() {
}

bool Resource::IsValidAndCacheable() const {
  // We don't have to worry about request_headers here since
  // if we have some we should be using UrlInputResource's implementation
  // of this method.
  return (HttpStatusOk() &&
          !server_context_->http_cache()->IsExpired(response_headers_) &&
          response_headers_.IsProxyCacheable(RequestHeaders::Properties(),
                                             respect_vary_,
                                             ResponseHeaders::kNoValidator));
}

bool Resource::IsSafeToRewrite(bool rewrite_uncacheable,
                               GoogleString* reason) const {
  RewriteStats* stats = server_context_->rewrite_stats();
  if (!HttpStatusOk()) {
    // Frustratingly, we have thrown away the headers of a CacheableResource at
    // this point, so we need to give feedback based upon the
    // fetch_response_status_.
    switch (fetch_response_status_) {
      case kFetchStatusDropped:
        StrAppend(reason, "Fetch was dropped due to load, ");
        break;
      case kFetchStatus4xxError:
        StrAppend(reason, "4xx status code, ");
        break;
      case kFetchStatusUncacheable200:
      case kFetchStatusUncacheableError:
        StrAppend(reason, "Uncacheable content, ");
        break;
      case kFetchStatusEmpty:
        // https://github.com/pagespeed/mod_pagespeed/issues/1050
        StrAppend(reason, "Resource is empty, ");
        break;
      case kFetchStatusOtherError:
        StrAppend(reason, "Fetch failure, ");
        break;
      case kFetchStatusNotSet:
        StrAppend(reason, "Resource headers are ");
        break;
      case kFetchStatusOK:
        LOG(WARNING) << "Fetch status OK but !HttpStatusOk in IsSafeToRewrite!";
        StrAppend(reason,
                  "Fetch status OK but !HttpStatusOk in IsSafeToRewrite!  ");
        break;
    }
  } else if (!rewrite_uncacheable && !IsValidAndCacheable()) {
    StrAppend(reason,
              (server_context_->http_cache()->IsExpired(response_headers_) ?
               "Cached content expired, " :
               "Invalid or uncacheable content, "));
  } else if (disable_rewrite_on_no_transform_ &&
             response_headers_.HasValue(HttpAttributes::kCacheControl,
                                        "no-transform")) {
    StrAppend(reason, "Cache-control: no-transform, ");
  } else if (response_headers_.Lookup1(HttpAttributes::kXSendfile) ||
             response_headers_.Lookup1(HttpAttributes::kXAccelRedirect)) {
    StrAppend(reason, "Sendfile in header, unsafe to rewrite! ");
  } else if (IsContentsEmpty()) {
    // https://github.com/pagespeed/mod_pagespeed/issues/1050
    StrAppend(reason, "Resource is empty, ");
  } else {
    // Safe.
    stats->num_cache_control_rewritable_resources()->Add(1);
    return true;
  }
  // If we get here, we're unsafe for the reason given.
  StrAppend(reason, "preventing rewriting of ", UrlForDebug());
  // TODO(sligocki): Are we over-counting this because uncacheable
  // resources will hit this stat for every filter, but cacheable ones
  // will only hit the above stat once?
  stats->num_cache_control_not_rewritable_resources()->Add(1);
  return false;
}

void Resource::LoadAsync(
    NotCacheablePolicy not_cacheable_policy,
    const RequestContextPtr& request_context,
    AsyncCallback* callback) {
  DCHECK(callback->resource().get() == this);
  if (loaded()) {
    RefreshIfImminentlyExpiring();
    callback->Done(false /* lock_failure */, true /* resource_ok */);
  } else {
    // Let the subclass handle it.
    LoadAndCallback(not_cacheable_policy, request_context, callback);
  }
}

void Resource::RefreshIfImminentlyExpiring() {
}

GoogleString Resource::ContentsHash() const {
  DCHECK(IsValidAndCacheable());
  return server_context_->contents_hasher()->Hash(
      ExtractUncompressedContents());
}

void Resource::AddInputInfoToPartition(HashHint suggest_include_content_hash,
                                       int index, CachedResult* partition) {
  InputInfo* input = partition->add_input();
  input->set_index(index);
  // FillInPartitionInputInfo can be specialized based on resource type.
  FillInPartitionInputInfo(suggest_include_content_hash, input);
}

// Default version.
void Resource::FillInPartitionInputInfo(HashHint include_content_hash,
                                        InputInfo* input) {
  CHECK(loaded());
  input->set_type(InputInfo::CACHED);
  DCHECK(!response_headers_.cache_fields_dirty()) << UrlForDebug();
  FillInPartitionInputInfoFromResponseHeaders(response_headers_, input);
  if ((include_content_hash == kIncludeInputHash) && IsValidAndCacheable()) {
    input->set_input_content_hash(ContentsHash());
  } else {
    input->clear_input_content_hash();
  }

  // TODO(jmarantz):  Implement this correctly for OutputResource which we also
  // have to purge if one of its inputs has been purged.
  if ((enable_cache_purge_ || proactive_resource_freshening_) && has_url()) {
    input->set_url(url());
  }
}

void Resource::FillInPartitionInputInfoFromResponseHeaders(
      const ResponseHeaders& headers,
      InputInfo* input) {
  if (headers.has_last_modified_time_ms()) {
    input->set_last_modified_time_ms(headers.last_modified_time_ms());
  }
  input->set_expiration_time_ms(headers.CacheExpirationTimeMs());
  input->set_date_ms(headers.date_ms());
}

int64 Resource::CacheExpirationTimeMs() const {
  int64 input_expire_time_ms = kNotCacheable;
  if (response_headers_.IsProxyCacheable(RequestHeaders::Properties(),
                                         respect_vary_,
                                         ResponseHeaders::kNoValidator)) {
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
  const ContentType* content_type;
  response_headers()->DetermineContentTypeAndCharset(&content_type, &charset_);
  // If there is no content type in headers, then guess from extension.
  if (content_type == NULL && has_url()) {
    GoogleString trimmed_url;
    TrimWhitespace(url(), &trimmed_url);
    content_type = NameExtensionToContentType(trimmed_url);
  }

  SetType(content_type);
}

Resource::AsyncCallback::~AsyncCallback() {
}

Resource::FreshenCallback::~FreshenCallback() {
}

bool Resource::Link(HTTPValue* value, MessageHandler* handler) {
  DCHECK(UseHttpCache());
  const SharedString& contents_and_headers = value->share();
  // Invalidate extracted_contents_.
  extracted_ = false;
  extracted_contents_.clear();
  return value_.Link(contents_and_headers, &response_headers_, handler);
}

void Resource::LinkFallbackValue(HTTPValue* value) {
  DCHECK(UseHttpCache());
  if (!value->Empty()) {
    fallback_value_.Link(value);
  }
}

StringPiece Resource::ExtractUncompressedContents() const {
  ResponseHeaders headers;
  if (!extracted_ && value_.ExtractHeaders(&headers, NULL)) {
    if (headers.IsGzipped()) {
      StringWriter inflate_writer(&extracted_contents_);
      if (GzipInflater::Inflate(raw_contents(), GzipInflater::kGzip,
                                &inflate_writer)) {
        extracted_ = true;
      }
    }
  }
  return extracted_ ? extracted_contents_ : raw_contents();
}

void Resource::Freshen(FreshenCallback* callback, MessageHandler* handler) {
  // We don't need Freshining for data urls or output resources.
  if (callback != NULL) {
    callback->Done(false /* lock_failure */, false /* resource_ok */);
  }
}

}  // namespace net_instaweb
