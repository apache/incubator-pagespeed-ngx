/*
 * Copyright 2012 Google Inc.
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

// Author: jefftk@google.com (Jeff Kaufman)

extern "C" {
  #include <ngx_config.h>
  #include <ngx_core.h>
  #include <ngx_http.h>
}


#include "ngx_rewrite_options.h"
#include "ngx_pagespeed.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"

namespace net_instaweb {

const char NgxRewriteOptions::kClassName[] = "NgxRewriteOptions";

RewriteOptions::Properties* NgxRewriteOptions::ngx_properties_ = NULL;

NgxRewriteOptions::NgxRewriteOptions() {
  Init();
}

void NgxRewriteOptions::Init() {
  DCHECK(ngx_properties_ != NULL)
      << "Call NgxRewriteOptions::Initialize() before construction";
  InitializeOptions(ngx_properties_);
}

void NgxRewriteOptions::AddProperties() {
  // TODO(jefftk): All these caching-related properties could move to an
  // OriginRewriteOptions.
  add_ngx_option("", &NgxRewriteOptions::file_cache_path_, "nfcp",
                 RewriteOptions::kFileCachePath);
  add_ngx_option(Timer::kHourMs,
                 &NgxRewriteOptions::file_cache_clean_interval_ms_,
                 "nfcci", RewriteOptions::kFileCacheCleanIntervalMs);
  add_ngx_option(100 * 1024,  // 100MB
                 &NgxRewriteOptions::file_cache_clean_size_kb_, "nfc",
                 RewriteOptions::kFileCacheCleanSizeKb);
  add_ngx_option(500000,
                 &NgxRewriteOptions::file_cache_clean_inode_limit_, "nfcl",
                 RewriteOptions::kFileCacheCleanInodeLimit);
  add_ngx_option(16384,  //16MB
                 &NgxRewriteOptions::lru_cache_byte_limit_, "nlcb",
                 RewriteOptions::kLruCacheByteLimit);
  add_ngx_option(1024,  // 1MB
                 &NgxRewriteOptions::lru_cache_kb_per_process_, "nlcp",
                 RewriteOptions::kLruCacheKbPerProcess);
  add_ngx_option("", &NgxRewriteOptions::memcached_servers_, "ams",
                 RewriteOptions::kMemcachedServers);
  add_ngx_option(1, &NgxRewriteOptions::memcached_threads_, "amt",
                 RewriteOptions::kMemcachedThreads);
  add_ngx_option(false, &NgxRewriteOptions::use_shared_mem_locking_, "ausml",
                 RewriteOptions::kUseSharedMemLocking);
  add_ngx_option("", &NgxRewriteOptions::fetcher_proxy_, "afp",
                 RewriteOptions::kFetcherProxy);

  MergeSubclassProperties(ngx_properties_);
  NgxRewriteOptions config;
  config.InitializeSignaturesAndDefaults();
}

void NgxRewriteOptions::InitializeSignaturesAndDefaults() {
  // Calls to foo_.DoNotUseForSignatureComputation() would go here.

  // Set default header value.
  set_default_x_header_value(kModPagespeedVersion);
}

void NgxRewriteOptions::Initialize() {
  if (Properties::Initialize(&ngx_properties_)) {
    RewriteOptions::Initialize();
    AddProperties();
  }
}

void NgxRewriteOptions::Terminate() {
  if (Properties::Terminate(&ngx_properties_)) {
    RewriteOptions::Terminate();
  }
}

bool NgxRewriteOptions::IsDirective(StringPiece config_directive,
                                    StringPiece compare_directive) {
  return StringCaseEqual(config_directive, compare_directive);
}

RewriteOptions::OptionSettingResult NgxRewriteOptions::ParseAndSetOptions0(
    StringPiece directive, GoogleString* msg, MessageHandler* handler) {
  if (IsDirective(directive, "on")) {
    set_enabled(true);
  } else if (IsDirective(directive, "off")) {
    set_enabled(false);
  } else {
    return RewriteOptions::kOptionNameUnknown;
  }
  return RewriteOptions::kOptionOk;
}

RewriteOptions::OptionSettingResult NgxRewriteOptions::ParseAndSetOptions1(
    StringPiece directive, StringPiece arg,
    GoogleString* msg, MessageHandler* handler) {

  // FileCachePath needs error checking.
  if (IsDirective(directive, "FileCachePath")) {
    if (!StringCaseStartsWith(arg, "/")) {
      *msg = "must start with a slash";
      return RewriteOptions::kOptionValueInvalid;
    } else {
      set_file_cache_path(arg.as_string());
    }
  }

  RewriteOptions::OptionSettingResult result =
      SetOptionFromName(directive, arg.as_string(), msg);
  if (result != RewriteOptions::kOptionNameUnknown) {
    return result;
  }

  if (IsDirective(directive, "Allow")) {
    Allow(arg);
  } else if (IsDirective(directive, "DangerPermitFetchFromUnknownHosts")) {
    // TODO(jefftk): port this.
    *msg = "not supported";
    return RewriteOptions::kOptionValueInvalid;
  } else if (IsDirective(directive, "DisableFilters")) {
    bool ok = DisableFiltersByCommaSeparatedList(arg, handler);
    if (!ok) {
      *msg = "Failed to disable some filters.";
      return RewriteOptions::kOptionValueInvalid;
    }
  } else if (IsDirective(directive, "Disallow")) {
    Disallow(arg);
  } else if (IsDirective(directive, "Domain")) {
    domain_lawyer()->AddDomain(arg, handler);
  } else if (IsDirective(directive, "EnableFilters")) {
    bool ok = EnableFiltersByCommaSeparatedList(arg, handler);
    if (!ok) {
      *msg = "Failed to enable some filters.";
      return RewriteOptions::kOptionValueInvalid;
    }
  } else if (IsDirective(directive, "FetchWithGzip")) {
    // TODO(jefftk): port this.
    *msg = "not supported";
    return RewriteOptions::kOptionValueInvalid;
  } else if (IsDirective(directive, "ForceCaching")) {
    // TODO(jefftk): port this.
    *msg = "not supported";
    return RewriteOptions::kOptionValueInvalid;
  } else if (IsDirective(directive, "ExperimentVariable")) {
    int slot;
    bool ok = StringToInt(arg.as_string().c_str(), &slot);
    if (!ok || slot < 1 || slot > 5) {
      *msg = "must be an integer between 1 and 5";
      return RewriteOptions::kOptionValueInvalid;
    }
    set_furious_ga_slot(slot);
  } else if (IsDirective(directive, "ExperimentSpec")) {
    bool ok = AddFuriousSpec(arg, handler);
    if (!ok) {
      *msg = "not a valid experiment spec";
      return RewriteOptions::kOptionValueInvalid;
    }
  } else if (IsDirective(directive, "RetainComment")) {
    RetainComment(arg);
  } else if (IsDirective(directive, "BlockingRewriteKey")) {
    set_blocking_rewrite_key(arg);
  } else {
    return RewriteOptions::kOptionNameUnknown;
  }

  return RewriteOptions::kOptionOk;
}

RewriteOptions::OptionSettingResult NgxRewriteOptions::ParseAndSetOptions2(
    StringPiece directive, StringPiece arg1, StringPiece arg2,
    GoogleString* msg, MessageHandler* handler) {
  if (IsDirective(directive, "MapRewriteDomain")) {
    domain_lawyer()->AddRewriteDomainMapping(arg1, arg2, handler);
  } else if (IsDirective(directive, "MapOriginDomain")) {
    domain_lawyer()->AddOriginDomainMapping(arg1, arg2, handler);
  } else if (IsDirective(directive, "MapProxyDomain")) {
    domain_lawyer()->AddProxyDomainMapping(arg1, arg2, handler);
  } else if (IsDirective(directive, "ShardDomain")) {
    domain_lawyer()->AddShard(arg1, arg2, handler);
  } else if (IsDirective(directive, "CustomFetchHeader")) {
    AddCustomFetchHeader(arg1, arg2);
  } else if (IsDirective(directive, "LoadFromFile")) {
    file_load_policy()->Associate(arg1,arg2);
  } else if (IsDirective(directive, "LoadFromFileMatch")) {
    if (!file_load_policy()->AssociateRegexp(arg1,arg2,msg)) {
      return RewriteOptions::kOptionValueInvalid;
    }
  } else if (IsDirective(directive, "LoadFromFileRule")
             || IsDirective(directive, "LoadFromFileRuleMatch")) {
    bool is_regexp = IsDirective(directive, "LoadFromFileRuleMatch");
    bool allow;
    // TODO(oschaaf): we should probably define consts for Allow/Disallow
    if (IsDirective(arg1, "Allow")) {
      allow = true;
    } else if (IsDirective(arg1, "Disallow")) {
      allow = false;
    } else {
      *msg = "Argument 1 must be either 'Allow' or 'Disallow'";
      return RewriteOptions::kOptionValueInvalid;
    }
    if (!file_load_policy()->AddRule(arg2.as_string().c_str(),
                                     is_regexp, allow, msg)) {
      return RewriteOptions::kOptionValueInvalid;
    }
  } else {
    return RewriteOptions::kOptionNameUnknown;
  }
  return RewriteOptions::kOptionOk;
}

RewriteOptions::OptionSettingResult NgxRewriteOptions::ParseAndSetOptions3(
    StringPiece directive, StringPiece arg1, StringPiece arg2, StringPiece arg3,
    GoogleString* msg, MessageHandler* handler) {
  if (IsDirective(directive, "UrlValuedAttribute")) {
    semantic_type::Category category;
    bool ok = semantic_type::ParseCategory(arg3, &category);
    if (!ok) {
      *msg = "Invalid resource category";
      return RewriteOptions::kOptionValueInvalid;
    }
    AddUrlValuedAttribute(arg1, arg2, category);
  } else if (IsDirective(directive, "Library")) {
    int64 bytes;
    bool ok = StringToInt64(arg1.as_string().c_str(), &bytes);
    if (!ok || bytes < 0) {
      *msg = "Size must be a positive 64-bit integer";
      return RewriteOptions::kOptionValueInvalid;
    }
    ok = RegisterLibrary(bytes, arg2, arg3);
    if (!ok) {
      *msg = "Format is size md5 url; bad md5 or URL";
      return RewriteOptions::kOptionValueInvalid;
    }    
  } else {
    return RewriteOptions::kOptionNameUnknown;
  }
  return RewriteOptions::kOptionOk;
}

// Very similar to apache/mod_instaweb::ParseDirective.
// TODO(jefftk): Move argument parsing to OriginRewriteOptions.
const char*
NgxRewriteOptions::ParseAndSetOptions(
    StringPiece* args, int n_args, ngx_pool_t* pool, MessageHandler* handler) {
  CHECK(n_args >= 1);

  int i;
  fprintf(stderr, "Setting option from (");
  for (i = 0 ; i < n_args ; i++) {
    fprintf(stderr, "%s\"%s\"",
            i == 0 ? "" : ", ",
            args[i].as_string().c_str());
  }
  fprintf(stderr, ")\n");

  StringPiece directive = args[0];

  // Remove initial "ModPagespeed" if there is one.
  StringPiece mod_pagespeed("ModPagespeed");
  if (StringCaseStartsWith(directive, mod_pagespeed)) {
    directive.remove_prefix(mod_pagespeed.size());
  }

  GoogleString msg;
  OptionSettingResult result;
  if (n_args == 1) {
    result = ParseAndSetOptions0(directive, &msg, handler);
  } else if (n_args == 2) {
    result = ParseAndSetOptions1(directive, args[1], &msg, handler);
  } else if (n_args == 3) {
    result = ParseAndSetOptions2(directive, args[1], args[2], &msg, handler);
  } else if (n_args == 4) {
    result = ParseAndSetOptions3(
        directive, args[1], args[2], args[3], &msg, handler);
  } else {
    return "unknown option";
  }

  switch (result) {
    case RewriteOptions::kOptionOk:
      return NGX_CONF_OK;
    case RewriteOptions::kOptionNameUnknown:
      return "unknown option";
    case RewriteOptions::kOptionValueInvalid: {
      GoogleString full_directive = "\"";
      for (int i = 0 ; i < n_args ; i++) {
        StrAppend(&full_directive, i == 0 ? "" : " ", args[i]);
      }
      StrAppend(&full_directive, "\": ", msg);
      char* s = ngx_psol::string_piece_to_pool_string(pool, full_directive);
      if (s == NULL) {
        return "failed to allocate memory";
      }
      return s;
    }
  }

  CHECK(false);
  return NULL;
}

NgxRewriteOptions* NgxRewriteOptions::Clone() const {
  NgxRewriteOptions* options = new NgxRewriteOptions();
  options->Merge(*this);
  return options;
}

const NgxRewriteOptions* NgxRewriteOptions::DynamicCast(
    const RewriteOptions* instance) {
  return (instance == NULL ||
          instance->class_name() != NgxRewriteOptions::kClassName
          ? NULL
          : static_cast<const NgxRewriteOptions*>(instance));
}

NgxRewriteOptions* NgxRewriteOptions::DynamicCast(RewriteOptions* instance) {
  return (instance == NULL ||
          instance->class_name() != NgxRewriteOptions::kClassName
          ? NULL
          : static_cast<NgxRewriteOptions*>(instance));
}

const char* NgxRewriteOptions::class_name() const {
  return NgxRewriteOptions::kClassName;
}


}  // namespace net_instaweb
