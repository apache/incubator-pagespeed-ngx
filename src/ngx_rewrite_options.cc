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

#include "ngx_rewrite_options.h"

extern "C" {
  #include <ngx_config.h>
  #include <ngx_core.h>
  #include <ngx_http.h>
}

#include "ngx_pagespeed.h"
#include "ngx_rewrite_driver_factory.h"

#include "net/instaweb/public/version.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/system/public/system_caches.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

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
  // Nothing ngx-specific for now.

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
    SystemRewriteOptions::Initialize();
    AddProperties();
  }
}

void NgxRewriteOptions::Terminate() {
  if (Properties::Terminate(&ngx_properties_)) {
    SystemRewriteOptions::Terminate();
  }
}

bool NgxRewriteOptions::IsDirective(StringPiece config_directive,
                                    StringPiece compare_directive) {
  return StringCaseEqual(config_directive, compare_directive);
}

RewriteOptions::OptionSettingResult NgxRewriteOptions::ParseAndSetOptions0(
    StringPiece directive, GoogleString* msg, MessageHandler* handler) {
  if (IsDirective(directive, "on")) {
    set_enabled(RewriteOptions::kEnabledOn);
  } else if (IsDirective(directive, "off")) {
    set_enabled(RewriteOptions::kEnabledOff);
  } else if (IsDirective(directive, "unplugged")) {
    set_enabled(RewriteOptions::kEnabledUnplugged);
  } else {
    return RewriteOptions::kOptionNameUnknown;
  }
  return RewriteOptions::kOptionOk;
}


RewriteOptions::OptionSettingResult
    NgxRewriteOptions::ParseAndSetOptionFromEnum1(
        OptionEnum directive, StringPiece arg,
        GoogleString* msg, MessageHandler* handler) {
  // FileCachePath needs error checking.
  if (directive == kFileCachePath) {
    if (!StringCaseStartsWith(arg, "/")) {
      *msg = "must start with a slash";
      return RewriteOptions::kOptionValueInvalid;
    }
  }

  // TODO(jefftk): port these (no enums for them yet, even!)
  //  DangerPermitFetchFromUnknownHosts, FetchWithGzip, ForceCaching

  return SystemRewriteOptions::ParseAndSetOptionFromEnum1(
      directive, arg, msg, handler);
}

// Very similar to apache/mod_instaweb::ParseDirective.
const char*
NgxRewriteOptions::ParseAndSetOptions(
    StringPiece* args, int n_args, ngx_pool_t* pool, MessageHandler* handler,
    NgxRewriteDriverFactory* driver_factory) {
  CHECK_GE(n_args, 1);

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
    StringPiece arg = args[1];
    // TODO(morlovich): Remove these special hacks, and handle these via
    // ParseAndSetOptionFromEnum1.
    if (IsDirective(directive, "UsePerVHostStatistics")) {
        // TODO(oschaaf): mod_pagespeed has a nicer way to do this.
        if (IsDirective(arg, "on")) {
          driver_factory->set_use_per_vhost_statistics(true);
          result = RewriteOptions::kOptionOk;
        } else if (IsDirective(arg, "off")) {
          driver_factory->set_use_per_vhost_statistics(false);
          result = RewriteOptions::kOptionOk;
        } else {
          result = RewriteOptions::kOptionValueInvalid;
        }
      } else if (IsDirective(directive, "InstallCrashHandler")) {
        // TODO(oschaaf): mod_pagespeed has a nicer way to do this.
        if (IsDirective(arg, "on")) {
          driver_factory->set_install_crash_handler(true);
          result = RewriteOptions::kOptionOk;
        } else if (IsDirective(arg, "off")) {
          driver_factory->set_install_crash_handler(false);
          result = RewriteOptions::kOptionOk;
        } else {
          result = RewriteOptions::kOptionValueInvalid;
        }
      } else if (IsDirective(directive, "MessageBufferSize")) {
        // TODO(oschaaf): mod_pagespeed has a nicer way to do this.
        int message_buffer_size;
        bool ok = StringToInt(arg.as_string(), &message_buffer_size);
        if (ok && message_buffer_size >= 0) {
          driver_factory->set_message_buffer_size(message_buffer_size);
          result = RewriteOptions::kOptionOk;
        } else {
          result = RewriteOptions::kOptionValueInvalid;
        }
      } else if (IsDirective(directive, "UseNativeFetcher")) {
        // TODO(oschaaf): mod_pagespeed has a nicer way to do this.
        if (IsDirective(arg, "on")) {
          driver_factory->set_use_native_fetcher(true);
          result = RewriteOptions::kOptionOk;
        } else if (IsDirective(arg, "off")) {
          driver_factory->set_use_native_fetcher(false);
          result = RewriteOptions::kOptionOk;
        } else {
          result = RewriteOptions::kOptionValueInvalid;
        }
      } else {
        result = ParseAndSetOptionFromName1(directive, args[1], &msg, handler);
      }
  } else if (n_args == 3) {
    // Short-term special handling, until this moves to common code.
    // TODO(morlovich): Clean this up.
    if (StringCaseEqual(directive, "CreateSharedMemoryMetadataCache")) {
      int64 kb = 0;
      if (!StringToInt64(args[2], &kb) || kb < 0) {
        result = RewriteOptions::kOptionValueInvalid;
        msg = "size_kb must be a positive 64-bit integer";
      } else {
        bool ok = driver_factory->caches()->CreateShmMetadataCache(
            args[1].as_string(), kb, &msg);
        result = ok ? kOptionOk : kOptionValueInvalid;
      }
    } else {
      result = ParseAndSetOptionFromName2(directive, args[1], args[2],
                                          &msg, handler);
    }
  } else if (n_args == 4) {
    result = ParseAndSetOptionFromName3(
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
  return dynamic_cast<const NgxRewriteOptions*>(instance);
}

NgxRewriteOptions* NgxRewriteOptions::DynamicCast(RewriteOptions* instance) {
  return dynamic_cast<NgxRewriteOptions*>(instance);
}

}  // namespace net_instaweb
