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
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

const char kStatisticsPath[] = "StatisticsPath";
const char kGlobalStatisticsPath[] = "GlobalStatisticsPath";
const char kConsolePath[] = "ConsolePath";
const char kMessagesPath[] = "MessagesPath";
const char kAdminPath[] = "AdminPath";
const char kGlobalAdminPath[] = "GlobalAdminPath";

// These options are copied from mod_instaweb.cc, where APACHE_CONFIG_OPTIONX
// indicates that they can not be set at the directory/location level. They set
// options in the RewriteDriverFactory, so they're entirely global and do not
// appear in RewriteOptions.  They are not alphabetized on purpose, but rather
// left in the same order as in mod_instaweb.cc in case we end up needing to
// compare.
// TODO(oschaaf): this duplication is a short term solution.
const char* const server_only_options[] = {
  "FetcherTimeoutMs",
  "FetchProxy",
  "ForceCaching",
  "GeneratedFilePrefix",
  "ImgMaxRewritesAtOnce",
  "InheritVHostConfig",
  "InstallCrashHandler",
  "MessageBufferSize",
  "NumRewriteThreads",
  "NumExpensiveRewriteThreads",
  "StaticAssetPrefix",
  "TrackOriginalContentLength",
  "UsePerVHostStatistics",  // TODO(anupama): What to do about "No longer used"
  "BlockingRewriteRefererUrls",
  "CreateSharedMemoryMetadataCache",
  "LoadFromFile",
  "LoadFromFileMatch",
  "LoadFromFileRule",
  "LoadFromFileRuleMatch",
  "UseNativeFetcher"
};

// Options that can only be used in the main (http) option scope.
const char* const main_only_options[] = {
  "UseNativeFetcher"
};

}  // namespace

RewriteOptions::Properties* NgxRewriteOptions::ngx_properties_ = NULL;

NgxRewriteOptions::NgxRewriteOptions(const StringPiece& description,
                                     ThreadSystem* thread_system)
    : SystemRewriteOptions(description, thread_system) {
  Init();
}

NgxRewriteOptions::NgxRewriteOptions(ThreadSystem* thread_system)
    : SystemRewriteOptions(thread_system) {
  Init();
}

void NgxRewriteOptions::Init() {
  DCHECK(ngx_properties_ != NULL)
      << "Call NgxRewriteOptions::Initialize() before construction";
  clear_inherited_scripts_ = false;
  InitializeOptions(ngx_properties_);
}

void NgxRewriteOptions::AddProperties() {
  // Nginx-specific options.
  add_ngx_option(
      "", &NgxRewriteOptions::statistics_path_, "nsp", kStatisticsPath,
      kServerScope, "Set the statistics path. Ex: /ngx_pagespeed_statistics");
  add_ngx_option(
      "", &NgxRewriteOptions::global_statistics_path_, "ngsp",
      kGlobalStatisticsPath, kProcessScope,
      "Set the global statistics path. Ex: /ngx_pagespeed_global_statistics");
  add_ngx_option(
      "", &NgxRewriteOptions::console_path_, "ncp", kConsolePath, kServerScope,
      "Set the console path. Ex: /pagespeed_console");
  add_ngx_option(
      "", &NgxRewriteOptions::messages_path_, "nmp", kMessagesPath,
      kServerScope, "Set the messages path.  Ex: /ngx_pagespeed_message");
  add_ngx_option(
      "", &NgxRewriteOptions::admin_path_, "nap", kAdminPath,
      kServerScope, "Set the admin path.  Ex: /pagespeed_admin");
  add_ngx_option(
      "", &NgxRewriteOptions::global_admin_path_, "ngap", kGlobalAdminPath,
      kProcessScope, "Set the global admin path.  Ex: /pagespeed_global_admin");

  MergeSubclassProperties(ngx_properties_);

  // Default properties are global but to set them the current API requires
  // a RewriteOptions instance and we're in a static method.
  NgxRewriteOptions dummy_config(NULL);
  dummy_config.set_default_x_header_value(kModPagespeedVersion);
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

RewriteOptions::OptionScope NgxRewriteOptions::GetOptionScope(
    StringPiece option_name) {
  ngx_uint_t i;
  ngx_uint_t size = sizeof(main_only_options) / sizeof(char*);
  for (i = 0; i < size; i++) {
    if (StringCaseEqual(main_only_options[i], option_name)) {
      return kProcessScopeStrict;
    }
  }

  size = sizeof(server_only_options) / sizeof(char*);
  for (i = 0; i < size; i++) {
    if (StringCaseEqual(server_only_options[i], option_name)) {
      return kServerScope;
    }
  }

  // This could be made more efficient if RewriteOptions provided a map allowing
  // access of options by their name. It's not too much of a worry at present
  // since this is just during initialization.
  for (OptionBaseVector::const_iterator it = all_options().begin();
       it != all_options().end(); ++it) {
    RewriteOptions::OptionBase* option = *it;
    if (option->option_name() == option_name) {
      // We treat kProcessScope as kProcessScopeStrict, failing to start if an
      // option is out of place.
      return option->scope() == kProcessScope ? kProcessScopeStrict
                                              : option->scope();
    }
  }
  return kDirectoryScope;
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
    NgxRewriteOptions::ParseAndSetOptionFromName1(
        StringPiece name, StringPiece arg,
        GoogleString* msg, MessageHandler* handler) {
  // FileCachePath needs error checking.
  if (StringCaseEqual(name, kFileCachePath)) {
    if (!StringCaseStartsWith(arg, "/")) {
      *msg = "must start with a slash";
      return RewriteOptions::kOptionValueInvalid;
    }
  }

  return SystemRewriteOptions::ParseAndSetOptionFromName1(
      name, arg, msg, handler);
}

template <class DriverFactoryT>
RewriteOptions::OptionSettingResult ParseAndSetOptionHelper(
    StringPiece option_value,
    DriverFactoryT* driver_factory,
    void (DriverFactoryT::*set_option_method)(bool)) {
  bool parsed_value;
  if (StringCaseEqual(option_value, "on") ||
      StringCaseEqual(option_value, "true")) {
    parsed_value = true;
  } else if (StringCaseEqual(option_value, "off") ||
             StringCaseEqual(option_value, "false")) {
    parsed_value = false;
  } else {
    return RewriteOptions::kOptionValueInvalid;
  }

  (driver_factory->*set_option_method)(parsed_value);
  return RewriteOptions::kOptionOk;
}

namespace {

const char* ps_error_string_for_option(
    ngx_pool_t* pool, StringPiece directive, StringPiece warning) {
  GoogleString msg =
      StrCat("\"", directive, "\" ", warning);
  char* s = string_piece_to_pool_string(pool, msg);
  if (s == NULL) {
    return "failed to allocate memory";
  }
  return s;
}

}  // namespace

// Very similar to apache/mod_instaweb::ParseDirective.
const char* NgxRewriteOptions::ParseAndSetOptions(
    StringPiece* args, int n_args, ngx_pool_t* pool, MessageHandler* handler,
    NgxRewriteDriverFactory* driver_factory,
    RewriteOptions::OptionScope scope, ngx_conf_t* cf, bool compile_scripts) {
  CHECK_GE(n_args, 1);

  StringPiece directive = args[0];

  // Remove initial "ModPagespeed" if there is one.
  StringPiece mod_pagespeed("ModPagespeed");
  if (StringCaseStartsWith(directive, mod_pagespeed)) {
    directive.remove_prefix(mod_pagespeed.size());
  }

  if (GetOptionScope(directive) > scope) {
    return ps_error_string_for_option(
        pool, directive, "cannot be set at this scope.");
  }

  ScriptLine* script_line;
  script_line = NULL;
  // Only allow script variable support for LoadFromFile for now.
  // Note that LoadFromFile should not be scriptable on wildcard hosts,
  // as browsers might be able to manipulate its natural use-case: $http_host.
  if (!StringCaseStartsWith(directive, "LoadFromFile")) {
    compile_scripts = false;
  }

  if (n_args == 1 && StringCaseEqual(directive, "ClearInheritedScripts")) {
    clear_inherited_scripts_ = true;
    return NGX_CONF_OK;
  }

  if (compile_scripts) {
    CHECK(cf != NULL);
    int i;
    // Skip the first arg which is always 'pagespeed'
    for (i = 1; i < n_args; i++) {
      ngx_str_t script_source;

      script_source.len = args[i].as_string().length();
      std::string tmp = args[i].as_string();
      script_source.data = reinterpret_cast<u_char*>(
          const_cast<char*>(tmp.c_str()));

      if (ngx_http_script_variables_count(&script_source) > 0) {
        ngx_http_script_compile_t* sc =
            reinterpret_cast<ngx_http_script_compile_t*>(
                ngx_pcalloc(cf->pool, sizeof(ngx_http_script_compile_t)));
        sc->cf = cf;
        sc->source = &script_source;
        sc->lengths = reinterpret_cast<ngx_array_t**>(
            ngx_pcalloc(cf->pool, sizeof(ngx_array_t*)));
        sc->values = reinterpret_cast<ngx_array_t**>(
            ngx_pcalloc(cf->pool, sizeof(ngx_array_t*)));
        sc->variables = 1;
        sc->complete_lengths = 1;
        sc->complete_values = 1;
        if (ngx_http_script_compile(sc) != NGX_OK) {
          return ps_error_string_for_option(
              pool, directive, "Failed to compile script variables");
        } else {
          if (script_line == NULL) {
            script_line = new ScriptLine(args, n_args, scope);
          }
          script_line->AddScriptAndArgIndex(sc, i);
        }
      }
    }

    if (script_line != NULL) {
      script_lines_.push_back(RefCountedPtr<ScriptLine>(script_line));
      // We have found script variables in the current configuration line, and
      // prepared the associated rewriteoptions for that.
      // We will defer parsing, validation and processing of this line to
      // request time. That means we are done handling this configuration line.
      return NGX_CONF_OK;
    }
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
      result = ParseAndSetOptionHelper<NgxRewriteDriverFactory>(
          arg, driver_factory,
          &NgxRewriteDriverFactory::set_use_per_vhost_statistics);
    } else if (IsDirective(directive, "InstallCrashHandler")) {
      result = ParseAndSetOptionHelper<NgxRewriteDriverFactory>(
          arg, driver_factory,
          &NgxRewriteDriverFactory::set_install_crash_handler);
    } else if (IsDirective(directive, "MessageBufferSize")) {
      int message_buffer_size;
      bool ok = StringToInt(arg.as_string(), &message_buffer_size);
      if (ok && message_buffer_size >= 0) {
        driver_factory->set_message_buffer_size(message_buffer_size);
        result = RewriteOptions::kOptionOk;
      } else {
        result = RewriteOptions::kOptionValueInvalid;
      }
    } else if (IsDirective(directive, "UseNativeFetcher")) {
      result = ParseAndSetOptionHelper<NgxRewriteDriverFactory>(
          arg, driver_factory,
          &NgxRewriteDriverFactory::set_use_native_fetcher);
    } else if (IsDirective(directive, "RateLimitBackgroundFetches")) {
      result = ParseAndSetOptionHelper<NgxRewriteDriverFactory>(
          arg, driver_factory,
          &NgxRewriteDriverFactory::set_rate_limit_background_fetches);
    } else if (IsDirective(directive, "ForceCaching")) {
      result = ParseAndSetOptionHelper<SystemRewriteDriverFactory>(
          arg, driver_factory,
          &SystemRewriteDriverFactory::set_force_caching);
    } else if (IsDirective(directive, "ListOutstandingUrlsOnError")) {
      result = ParseAndSetOptionHelper<SystemRewriteDriverFactory>(
          arg, driver_factory,
          &SystemRewriteDriverFactory::list_outstanding_urls_on_error);
    } else if (IsDirective(directive, "TrackOriginalContentLength")) {
      result = ParseAndSetOptionHelper<SystemRewriteDriverFactory>(
          arg, driver_factory,
          &SystemRewriteDriverFactory::set_track_original_content_length);
    } else if (IsDirective(directive, "StaticAssetPrefix")) {
      driver_factory->set_static_asset_prefix(arg);
      result = RewriteOptions::kOptionOk;
    } else if (StringCaseEqual("ProcessScriptVariables", args[0])) {
      if (scope == RewriteOptions::kProcessScopeStrict) {
        if (StringCaseEqual(arg, "on")) {
          if (driver_factory->SetProcessScriptVariables(true)) {
            result = RewriteOptions::kOptionOk;
          } else {
            return const_cast<char*>(
                "pagespeed ProcessScriptVariables: can only be set once");
          }
        } else if (StringCaseEqual(arg, "off")) {
          if (driver_factory->SetProcessScriptVariables(false)) {
            result = RewriteOptions::kOptionOk;
          } else {
            return const_cast<char*>(
                "pagespeed ProcessScriptVariables: can only be set once");
          }
        } else {
          return const_cast<char*>(
              "pagespeed ProcessScriptVariables: invalid value");
        }
      } else {
        return const_cast<char*>(
            "ProcessScriptVariables is only allowed at the top level");
      }
    } else {
      result = ParseAndSetOptionFromName1(directive, arg, &msg, handler);
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
    return ps_error_string_for_option(
        pool, directive, "not recognized or too many arguments");
  }

  switch (result) {
    case RewriteOptions::kOptionOk:
      return NGX_CONF_OK;
    case RewriteOptions::kOptionNameUnknown:
      return ps_error_string_for_option(
          pool, directive, "not recognized or too many arguments");
    case RewriteOptions::kOptionValueInvalid: {
      GoogleString full_directive;
      for (int i = 0 ; i < n_args ; i++) {
        StrAppend(&full_directive, i == 0 ? "" : " ", args[i]);
      }
      return ps_error_string_for_option(pool, full_directive, msg);
    }
  }

  CHECK(false);
  return NULL;
}

// Execute all entries in the script_lines vector, and hand the result off to
// ParseAndSetOptions to obtain the final option values.
bool NgxRewriteOptions::ExecuteScriptVariables(
    ngx_http_request_t* r, MessageHandler* handler,
    NgxRewriteDriverFactory* driver_factory) {
  bool script_error = false;

  if (script_lines_.size() > 0) {
    std::vector<RefCountedPtr<ScriptLine> >::iterator it;
    for (it = script_lines_.begin() ; it != script_lines_.end(); ++it) {
      ScriptLine* script_line = it->get();
      StringPiece args[NGX_PAGESPEED_MAX_ARGS];
      std::vector<ScriptArgIndex*>::iterator cs_it;
      int i;

      for (i = 0; i < script_line->n_args(); i++) {
        args[i] = script_line->args()[i];
      }

      for (cs_it = script_line->data().begin();
           cs_it != script_line->data().end(); cs_it++) {
        ngx_http_script_compile_t* script;
        ngx_array_t* values;
        ngx_array_t* lengths;
        ngx_str_t value;

        script = (*cs_it)->script();
        lengths = *script->lengths;
        values = *script->values;

        if (ngx_http_script_run(r, &value, lengths->elts, 0, values->elts)
            == NULL) {
          handler->Message(kError, "ngx_http_script_run error");
          script_error = true;
          break;
        } else  {
          args[(*cs_it)->index()] = str_to_string_piece(value);
        }
      }

      const char* status = ParseAndSetOptions(args, script_line->n_args(),
          r->pool, handler, driver_factory, script_line->scope(), NULL /*cf*/,
          false /*compile scripts*/);

      if (status != NULL) {
        script_error = true;
        handler->Message(kWarning,
            "Error setting option value from script: '%s'", status);
        break;
      }
    }
  }

  if (script_error) {
    handler->Message(kWarning,
        "Script error(s) in configuration, disabling optimization");
    set_enabled(RewriteOptions::kEnabledOff);
    return false;
  }

  return true;
}

void NgxRewriteOptions::CopyScriptLinesTo(
    NgxRewriteOptions* destination) const {
  destination->script_lines_ = script_lines_;
}

void NgxRewriteOptions::AppendScriptLinesTo(
    NgxRewriteOptions* destination) const {
  destination->script_lines_.insert(destination->script_lines_.end(),
                                    script_lines_.begin(), script_lines_.end());
}

NgxRewriteOptions* NgxRewriteOptions::Clone() const {
  NgxRewriteOptions* options = new NgxRewriteOptions(
      StrCat("cloned from ", description()), thread_system());
  this->CopyScriptLinesTo(options);
  options->Merge(*this);
  return options;
}

void NgxRewriteOptions::Merge(const RewriteOptions& src) {
  SystemRewriteOptions::Merge(src);
}

const NgxRewriteOptions* NgxRewriteOptions::DynamicCast(
    const RewriteOptions* instance) {
  return dynamic_cast<const NgxRewriteOptions*>(instance);
}

NgxRewriteOptions* NgxRewriteOptions::DynamicCast(RewriteOptions* instance) {
  return dynamic_cast<NgxRewriteOptions*>(instance);
}

}  // namespace net_instaweb
