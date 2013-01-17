// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "net/instaweb/rewriter/public/rewrite_query.h"

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/basictypes.h"        // for int64
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"

namespace net_instaweb {

const char RewriteQuery::kModPagespeed[] = "ModPagespeed";
const char RewriteQuery::kModPagespeedFilters[] = "ModPagespeedFilters";
const char RewriteQuery::kNoscriptValue[] = "noscript";

// static array of query params that have setters taking a single int64 arg.
// TODO(matterbury): Accept or solve the problem that the query parameter
// names are duplicated here and in apache/mod_instaweb.cc.
typedef void (RewriteOptions::*RewriteOptionsInt64PMF)(int64);

struct Int64QueryParam {
  const char* name_;
  RewriteOptionsInt64PMF method_;
};

static struct Int64QueryParam int64_query_params_[] = {
  { "ModPagespeedCssFlattenMaxBytes",
    &RewriteOptions::set_css_flatten_max_bytes },
  { "ModPagespeedCssInlineMaxBytes",
    &RewriteOptions::set_css_inline_max_bytes },
  // Note: If ModPagespeedImageInlineMaxBytes is specified, and
  // ModPagespeedCssImageInlineMaxBytes is not set explicitly, both the
  // thresholds get set to ModPagespeedImageInlineMaxBytes.
  { "ModPagespeedImageInlineMaxBytes",
    &RewriteOptions::set_image_inline_max_bytes },
  { "ModPagespeedCssImageInlineMaxBytes",
    &RewriteOptions::set_css_image_inline_max_bytes },
  { "ModPagespeedJsInlineMaxBytes",
    &RewriteOptions::set_js_inline_max_bytes },
  { "ModPagespeedDomainShardCount",
    &RewriteOptions::set_domain_shard_count },
  { "ModPagespeedJpegRecompressionQuality",
    &RewriteOptions::set_image_jpeg_recompress_quality },
  { "ModPagespeedImageRecompressionQuality",
    &RewriteOptions::set_image_recompress_quality },
  { "ModPagespeedWebpRecompressionQuality",
    &RewriteOptions::set_image_webp_recompress_quality },
};

template <class HeaderT>
RewriteQuery::Status RewriteQuery::ScanHeader(
    HeaderT* headers,
    RewriteOptions* options,
    MessageHandler* handler) {
  Status status = kNoneFound;

  if (headers == NULL) {
    return status;
  }

  // Tracks the headers that need to be removed.
  HeaderT headers_to_remove;

  for (int i = 0, n = headers->NumAttributes(); i < n; ++i) {
    switch (ScanNameValue(headers->Name(i), headers->Value(i), options,
                          handler)) {
      case kNoneFound:
        break;
      case kSuccess:
        headers_to_remove.Add(headers->Name(i), headers->Value(i));
        status = kSuccess;
        break;
      case kInvalid:
        return kInvalid;
    }
  }

  for (int i = 0, n = headers_to_remove.NumAttributes(); i < n; ++i) {
    headers->Remove(headers_to_remove.Name(i), headers_to_remove.Value(i));
  }

  return status;
}

// Scan for option-sets in query-params. We will only allow a limited number of
// options to be set. In particular, some options are risky to set per query,
// such as image inline threshold, which exposes a DOS vulnerability and a risk
// of poisoning our internal cache. Domain adjustments can potentially introduce
// a security vulnerability.
RewriteQuery::Status RewriteQuery::Scan(
    RewriteDriverFactory* factory,
    GoogleUrl* request_url,
    RequestHeaders* request_headers,
    ResponseHeaders* response_headers,
    scoped_ptr<RewriteOptions>* options,
    MessageHandler* handler) {
  options->reset(NULL);

  QueryParams query_params;
  query_params.Parse(request_url->Query());

  // See if anything looks even remotely like one of our options before doing
  // any more work.
  if (!MayHaveCustomOptions(query_params, request_headers, response_headers)) {
    return kNoneFound;
  }

  options->reset(factory->NewRewriteOptionsForQuery());

  Status status = kNoneFound;
  QueryParams temp_query_params;
  for (int i = 0; i < query_params.size(); ++i) {
    const GoogleString* value = query_params.value(i);
    if (value != NULL) {
      switch (ScanNameValue(
                  query_params.name(i), *value, options->get(), handler)) {
        case kNoneFound:
          temp_query_params.Add(query_params.name(i), *value);
          break;
        case kSuccess:
          status = kSuccess;
          break;
        case kInvalid:
          return kInvalid;
      }
    } else {
      temp_query_params.Add(query_params.name(i), NULL);
    }
  }
  if (status == kSuccess) {
    // Remove the ModPagespeed* for url.
    GoogleString temp_params = temp_query_params.empty() ? "" :
        StrCat("?", temp_query_params.ToString());
    request_url->Reset(StrCat(request_url->AllExceptQuery(), temp_params,
                              request_url->AllAfterQuery()));
  }

  switch (ScanHeader<RequestHeaders>(request_headers, options->get(),
                                     handler)) {
    case kNoneFound:
      break;
    case kSuccess:
      status = kSuccess;
      break;
    case kInvalid:
      return kInvalid;
  }

  switch (ScanHeader<ResponseHeaders>(response_headers, options->get(),
                                      handler)) {
    case kNoneFound:
      break;
    case kSuccess:
      status = kSuccess;
      break;
    case kInvalid:
      return kInvalid;
  }

  // Set a default rewrite level in case the mod_pagespeed server has no
  // rewriting options configured.
  // Note that if any filters are explicitly set with
  // ModPagespeedFilters=..., then the call to
  // DisableAllFiltersNotExplicitlyEnabled() below will make the 'level'
  // irrelevant.
  if (status == kSuccess) {
    options->get()->SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  }

  return status;
}

template <class HeaderT>
bool RewriteQuery::HeadersMayHaveCustomOptions(const QueryParams& params,
                                               const HeaderT* headers) {
  if (headers != NULL) {
    for (int i = 0, n = headers->NumAttributes(); i < n; ++i) {
      if (StringPiece(headers->Name(i)).starts_with(kModPagespeed)) {
        return true;
      }
    }
  }
  return false;
}

bool RewriteQuery::MayHaveCustomOptions(
    const QueryParams& params, const RequestHeaders* req_headers,
    const ResponseHeaders* resp_headers) {
  for (int i = 0, n = params.size(); i < n; ++i) {
    if (StringPiece(params.name(i)).starts_with(kModPagespeed)) {
      return true;
    }
  }
  if (HeadersMayHaveCustomOptions(params, req_headers)) {
    return true;
  }
  if (HeadersMayHaveCustomOptions(params, resp_headers)) {
    return true;
  }
  return false;
}

RewriteQuery::Status RewriteQuery::ScanNameValue(
    const StringPiece& name, const GoogleString& value,
    RewriteOptions* options, MessageHandler* handler) {
  Status status = kNoneFound;
  if (name == kModPagespeed) {
    RewriteOptions::EnabledEnum enabled;
    if (RewriteOptions::ParseFromString(value, &enabled)) {
      options->set_enabled(enabled);
      status = kSuccess;
    } else if (value == kNoscriptValue) {
      // Disable filters that depend on custom script execution.
      options->DisableFiltersRequiringScriptExecution();
      // Blink cache hit response will also redirect to "?Noscript=" and hence
      // we need to disable blink.  Otherwise we will enter
      // blink_flow_critical_line (causing a redirect loop).
      options->DisableFilter(RewriteOptions::kPrioritizeVisibleContent);
      options->EnableFilter(RewriteOptions::kHandleNoscriptRedirect);
      status = kSuccess;
    } else {
      // TODO(sligocki): Return 404s instead of logging server errors here
      // and below.
      handler->Message(kWarning, "Invalid value for %s: %s "
                       "(should be on, off, unplugged, or noscript)",
                       name.as_string().c_str(),
                       value.c_str());
      status = kInvalid;
    }
  } else if (name == kModPagespeedFilters) {
    // When using ModPagespeedFilters query param, only the
    // specified filters should be enabled.
    if (options->AdjustFiltersByCommaSeparatedList(value, handler)) {
      status = kSuccess;
    } else {
      status = kInvalid;
    }
  } else {
    for (unsigned i = 0; i < arraysize(int64_query_params_); ++i) {
      if (name == int64_query_params_[i].name_) {
        int64 int_val;
        if (StringToInt64(value, &int_val)) {
          RewriteOptionsInt64PMF method = int64_query_params_[i].method_;
          (options->*method)(int_val);
          status = kSuccess;
        } else {
          handler->Message(kWarning, "Invalid integer value for %s: %s",
                           name.as_string().c_str(), value.c_str());
          status = kInvalid;
        }
        break;
      }
    }
  }

  return status;
}

}  // namespace net_instaweb
