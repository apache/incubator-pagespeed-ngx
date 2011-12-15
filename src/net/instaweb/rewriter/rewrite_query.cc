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
#include "net/instaweb/util/public/basictypes.h"        // for int64
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"

namespace net_instaweb {

const char RewriteQuery::kModPagespeed[] =
    "ModPagespeed";
const char RewriteQuery::kModPagespeedDisableForBots[] =
    "ModPagespeedDisableForBots";
const char RewriteQuery::kModPagespeedFilters[] =
    "ModPagespeedFilters";

// static array of query params that have setters taking a single int64 arg.
// TODO(matterbury): Accept or solve the problem that the query parameter
// names are duplicated here and in apache/mod_instaweb.cc.
typedef void (RewriteOptions::*RewriteOptionsInt64PMF)(int64);
struct Int64QueryParam {
  const char* name_;
  RewriteOptionsInt64PMF method_;
};
static struct Int64QueryParam int64_query_params_[] = {
  { "ModPagespeedCssInlineMaxBytes",
    &RewriteOptions::set_css_inline_max_bytes },
  { "ModPagespeedImageInlineMaxBytes",
    &RewriteOptions::set_image_inline_max_bytes },
  { "ModPagespeedCssImageInlineMaxBytes",
    &RewriteOptions::set_css_image_inline_max_bytes },
  { "ModPagespeedJsInlineMaxBytes",
    &RewriteOptions::set_js_inline_max_bytes }
};

// Scan for option-sets in query-params.  We will only allow a limited
// number of options to be set.  In particular, some options are risky
// to set per query, such as image inline threshold, which exposes a
// DOS vulnerability and a risk of poisoning our internal cache.
// Domain adjustments can potentially introduce a security
// vulnerability.
//
// So we will check for explicit parameters we want to support.
RewriteQuery::Status RewriteQuery::Scan(
    const QueryParams& query_params,
    const RequestHeaders& request_headers,
    RewriteOptions* options,
    MessageHandler* handler) {
  Status status = kNoneFound;

  for (int i = 0; i < query_params.size(); ++i) {
    const GoogleString* value = query_params.value(i);
    if (value != NULL) {  // All query-params we care about have values.
      switch (ScanNameValue(query_params.name(i), *value, options, handler)) {
        case kNoneFound:
          break;
        case kSuccess:
          status = kSuccess;
          break;
        case kInvalid:
          return kInvalid;
      }
    }
  }

  for (int i = 0, n = request_headers.NumAttributes(); i < n; ++i) {
    switch (ScanNameValue(request_headers.Name(i), request_headers.Value(i),
                          options, handler)) {
      case kNoneFound:
        break;
      case kSuccess:
        status = kSuccess;
        break;
      case kInvalid:
        return kInvalid;
    }
  }

  // This semantic provides for a mod_pagespeed server that has no rewriting
  // options configured at all.  Turning the module on should some reasonable
  // defaults.  Note that if any filters are explicitly set with
  // ModPagespeedFilters=..., then the call to
  // DisableAllFiltersNotExplicitlyEnabled() below will make the 'level'
  // irrelevant.
  if (status == kSuccess) {
    options->SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  }

  return status;
}

RewriteQuery::Status RewriteQuery::ScanNameValue(
    const StringPiece& name, const GoogleString& value,
    RewriteOptions* options, MessageHandler* handler) {
  Status status = kNoneFound;
  if (name == kModPagespeed) {
    bool is_on = value == "on";
    if (is_on || (value == "off")) {
      options->set_enabled(is_on);
      status = kSuccess;
    } else {
      // TODO(sligocki): Return 404s instead of logging server errors here
      // and below.
      handler->Message(kWarning, "Invalid value for %s: %s "
                       "(should be on or off)",
                       name.as_string().c_str(),
                       value.c_str());
      status = kInvalid;
    }
  } else if (name == kModPagespeedDisableForBots) {
    bool is_on = value == "on";
    if (is_on || (value == "off")) {
      options->set_botdetect_enabled(is_on);
      status = kSuccess;
    } else {
      handler->Message(kWarning, "Invalid value for %s: %s "
                       "(should be on or off)",
                       name.as_string().c_str(),
                       value.c_str());
      status = kInvalid;
    }
  } else if (name == kModPagespeedFilters) {
    // When using ModPagespeedFilters query param, only the
    // specified filters should be enabled.
    options->SetRewriteLevel(RewriteOptions::kPassThrough);
    if (options->AdjustFiltersByCommaSeparatedList(value, handler)) {
      options->DisableAllFiltersNotExplicitlyEnabled();
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
