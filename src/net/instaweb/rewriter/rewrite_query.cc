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
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"        // for int64
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"

namespace net_instaweb {

const char RewriteQuery::kModPagespeed[] =
    "ModPagespeed";
const char RewriteQuery::kModPagespeedCssInlineMaxBytes[] =
    "ModPagespeedCssInlineMaxBytes";
const char RewriteQuery::kModPagespeedDisableForBots[] =
    "ModPagespeedDisableForBots";
const char RewriteQuery::kModPagespeedFilters[] =
    "ModPagespeedFilters";

namespace {

RewriteOptions* GetOptions(scoped_ptr<RewriteOptions>* options) {
  if (options->get() == NULL) {
    // TODO(jmarantz): instead of newing a RewriteOptions here,
    // require the caller of RewriteQuery::Scan to pass in constructed
    // RewriteOptions.  This will be a little easier if we make the
    // RewriteOptions constructor faster by using gperf rather than
    // constructing maps at construction time.
    options->reset(new RewriteOptions);
    (*options)->SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  }
  return options->get();
}

}  // namespace

// Scan for option-sets in query-params.  We will only allow a limited
// number of options to be set.  In particular, some options are risky
// to set per query, such as image inline threshold, which exposes a
// DOS vulnerability and a risk of poisoning our internal cache.
// Domain adjustments can potentially introduce a security
// vulnerability.
//
// So we will check for explicit parameters we want to support.
RewriteOptions* RewriteQuery::Scan(const QueryParams& query_params,
                                   MessageHandler* handler) {
  scoped_ptr<RewriteOptions> options;
  bool ret = true;

  for (int i = 0; i < query_params.size(); ++i) {
    const char* name = query_params.name(i);
    const GoogleString* value = query_params.value(i);
    if (value == NULL) {
      // Empty; all our options require a value, so skip.  It might be a
      // perfectly legitimate query param for the underlying page.
      continue;
    }
    int64 int_val;
    if (strcmp(name, kModPagespeed) == 0) {
      bool is_on = (value->compare("on") == 0);
      if (is_on || (value->compare("off") == 0)) {
        GetOptions(&options)->set_enabled(is_on);
      } else {
        // TODO(sligocki): Return 404s instead of logging server errors here
        // and below.
        handler->Message(kWarning, "Invalid value for %s: %s "
                         "(should be on or off)", name, value->c_str());
        ret = false;
      }
    } else if (strcmp(name, kModPagespeedDisableForBots) == 0) {
      bool is_on = (value->compare("on") == 0);
      if (is_on || (value->compare("off") == 0)) {
        GetOptions(&options)->set_botdetect_enabled(is_on);
      } else {
        handler->Message(kWarning, "Invalid value for %s: %s "
                         "(should be on or off)", name, value->c_str());
        ret = false;
      }
    } else if (strcmp(name, kModPagespeedFilters) == 0) {
      // When using ModPagespeedFilters query param, only the
      // specified filters should be enabled.
      GetOptions(&options)->SetRewriteLevel(RewriteOptions::kPassThrough);
      if (options->EnableFiltersByCommaSeparatedList(*value, handler)) {
        options->DisableAllFiltersNotExplicitlyEnabled();
      } else {
        ret = false;
      }
    // TODO(jmarantz): add js inlinine threshold, outline threshold.
    } else if (strcmp(name, kModPagespeedCssInlineMaxBytes) == 0) {
      if (StringToInt64(*value, &int_val)) {
        GetOptions(&options)->set_css_inline_max_bytes(int_val);
      } else {
        handler->Message(kWarning, "Invalid integer value for %s: %s",
                         name, value->c_str());
        ret = false;
      }
    }
  }

  // mod_pagespeed behavior is that if any options look like they are
  // for ModPagespeed, but are bad in some way, then we don't ignore all
  // the query-params, printing warnings to logs.
  if (!ret) {
    options.reset(NULL);
  }

  return options.release();
}

}  // namespace net_instaweb
