/*
 * Copyright 2014 Google Inc.
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

// Author: kspoelstra@we-amp.com (Kees Spoelstra)

#include "ngx_gzip_setter.h"

#include <ngx_conf_file.h>

namespace net_instaweb {

NgxGZipSetter g_gzip_setter;

extern "C" {
  // These functions replace the setters for:
  //   gzip
  //   gzip_types
  //   gzip_http_version
  //   gzip_vary
  //
  // If these functions are called it means there is an explicit gzip
  // configuration. The gzip configuration set by pagespeed is then rolled
  // back and pagespeed will stop enabling gzip automatically.
  char* ngx_gzip_redirect_conf_set_flag_slot(
      ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    if (g_gzip_setter.enabled()) {
      g_gzip_setter.RollBackAndDisable(cf);
    }
    char* ret = ngx_conf_set_flag_slot(cf, cmd, conf);
    return ret;
  }

  char* ngx_gzip_redirect_http_types_slot(
      ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    if (g_gzip_setter.enabled()) {
      g_gzip_setter.RollBackAndDisable(cf);
    }
    char* ret = ngx_http_types_slot(cf, cmd, conf);
    return ret;
  }

  char* ngx_gzip_redirect_conf_set_enum_slot(
      ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    if (g_gzip_setter.enabled()) {
      g_gzip_setter.RollBackAndDisable(cf);
    }
    char* ret = ngx_conf_set_enum_slot(cf, cmd, conf);
    return ret;
  }
  char* ngx_gzip_redirect_conf_set_bitmask_slot(
      ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    if (g_gzip_setter.enabled()) {
      g_gzip_setter.RollBackAndDisable(cf);
    }
    char* ret = ngx_conf_set_bitmask_slot(cf, cmd, conf);
    return ret;
  }
}

NgxGZipSetter::NgxGZipSetter() : enabled_(0) { }
NgxGZipSetter::~NgxGZipSetter() { }

// Helper functions to determine signature.
bool HasLocalConfig(ngx_command_t* command) {
  return (!(command->type & (NGX_DIRECT_CONF|NGX_MAIN_CONF)) &&
          command->conf == NGX_HTTP_LOC_CONF_OFFSET);
}
bool IsNgxFlagCommand(ngx_command_t* command) {
  return (command->set == ngx_conf_set_flag_slot &&
          HasLocalConfig(command));
}
bool IsNgxHttpTypesCommand(ngx_command_t* command) {
  return (command->set == ngx_http_types_slot &&
          HasLocalConfig(command));
}
bool IsNgxEnumCommand(ngx_command_t* command) {
  return (command->set == ngx_conf_set_enum_slot &&
          HasLocalConfig(command));
}
bool IsNgxBitmaskCommand(ngx_command_t* command) {
  return (command->set == ngx_conf_set_bitmask_slot &&
          HasLocalConfig(command));
}

// Initialize the NgxGzipSetter.
// Find the gzip, gzip_vary, gzip_http_version and gzip_types commands in the
// gzip module. Enable if the signature of the zip command matches with what we
// trust. Also sets up redirects for the configurations. These redirect handle
// a rollback if expicit configuration is found.
// If commands are not found the method will inform the user by logging.
void NgxGZipSetter::Init(ngx_conf_t* cf) {
#if (NGX_HTTP_GZIP)
  bool gzip_signature_mismatch = false;
  bool other_signature_mismatch = false;
  for (int m = 0; ngx_modules[m] != NULL; m++) {
    if (ngx_modules[m]->commands != NULL) {
      for (int c = 0; ngx_modules[m]->commands[c].name.len; c++) {
        ngx_command_t* current_command =& ngx_modules[m]->commands[c];

        // We look for the gzip command, and the exact signature we trust
        // this means configured as an config location offset
        // and a ngx_flag_t setter.
        // Also see:
        //   ngx_conf_handler in ngx_conf_file.c
        //   ngx_http_gzip_filter_commands in ngx_http_gzip_filter.c
        if (gzip_command_.command_ == NULL &&
            STR_EQ_LITERAL(current_command->name, "gzip")) {
          if (IsNgxFlagCommand(current_command)) {
            current_command->set = ngx_gzip_redirect_conf_set_flag_slot;
            gzip_command_.command_ = current_command;
            gzip_command_.module_ = ngx_modules[m];
            enabled_ = 1;
          } else {
            ngx_conf_log_error(
                NGX_LOG_WARN, cf, 0,
                "pagespeed: cannot set gzip, signature mismatch");
            gzip_signature_mismatch = true;
          }
        }

        if (!gzip_http_version_command_.command_ &&
            STR_EQ_LITERAL(current_command->name, "gzip_http_version")) {
          if (IsNgxEnumCommand(current_command)) {
            current_command->set = ngx_gzip_redirect_conf_set_enum_slot;
            gzip_http_version_command_.command_ = current_command;
            gzip_http_version_command_.module_ = ngx_modules[m];
          } else {
            ngx_conf_log_error(
                NGX_LOG_WARN, cf, 0,
                "pagespeed: cannot set gzip_http_version, signature mismatch");
            other_signature_mismatch = true;
          }
        }

        if (!gzip_proxied_command_.command_ &&
            STR_EQ_LITERAL(current_command->name, "gzip_proxied")) {
          if (IsNgxBitmaskCommand(current_command)) {
            current_command->set = ngx_gzip_redirect_conf_set_bitmask_slot;
            gzip_proxied_command_.command_ = current_command;
            gzip_proxied_command_.module_ = ngx_modules[m];
          } else {
            ngx_conf_log_error(
                NGX_LOG_WARN, cf, 0,
                "pagespeed: cannot set gzip_proxied, signature mismatch");
            other_signature_mismatch = true;
          }
        }

        if (!gzip_http_types_command_.command_ &&
            STR_EQ_LITERAL(current_command->name, "gzip_types")) {
          if (IsNgxHttpTypesCommand(current_command)) {
            current_command->set = ngx_gzip_redirect_http_types_slot;
            gzip_http_types_command_.command_ = current_command;
            gzip_http_types_command_.module_ = ngx_modules[m];
          } else {
            ngx_conf_log_error(
                NGX_LOG_WARN, cf, 0,
                "pagespeed: cannot set gzip_types, signature mismatch");
            other_signature_mismatch = true;
          }
        }

        if (!gzip_vary_command_.command_ &&
            STR_EQ_LITERAL(current_command->name, "gzip_vary")) {
          if (IsNgxFlagCommand(current_command)) {
            current_command->set = ngx_gzip_redirect_conf_set_flag_slot;
            gzip_vary_command_.command_ = current_command;
            gzip_vary_command_.module_ = ngx_modules[m];
          } else {
            ngx_conf_log_error(
                NGX_LOG_WARN, cf, 0,
                "pagespeed: cannot set gzip_vary, signature mismatch");
            other_signature_mismatch = true;
          }
        }
      }
    }
  }
  if (gzip_signature_mismatch) {
    return;  // Already logged error.
  } else if (!enabled_) {
    // Looked through all the available commands and didn't find the "gzip" one.
    ngx_conf_log_error(
        NGX_LOG_WARN, cf, 0, "pagespeed: cannot set gzip, command not found");
    return;
  } else if (other_signature_mismatch) {
    return;  // Already logged error.
  } else if (!gzip_vary_command_.command_) {
    ngx_conf_log_error(
        NGX_LOG_WARN, cf, 0, "pagespeed: missing gzip_vary");
    return;
  } else if (!gzip_http_types_command_.command_) {
    ngx_conf_log_error(
        NGX_LOG_WARN, cf, 0, "pagespeed: missing gzip_types");
    return;
  } else if (!gzip_http_version_command_.command_) {
    ngx_conf_log_error(
        NGX_LOG_WARN, cf, 0, "pagespeed: missing gzip_http_version");
    return;
  } else if (!gzip_proxied_command_.command_) {
    ngx_conf_log_error(
        NGX_LOG_WARN, cf, 0, "pagespeed: missing gzip_proxied");
    return;
  } else {
    return;  // Success.
  }
#else
  ngx_conf_log_error(
      NGX_LOG_WARN, cf, 0, "pagespeed: gzip not compiled into nginx");
  return;
#endif
}

void* ngx_command_ctx::GetConfPtr(ngx_conf_t* cf) {
  return GetModuleConfPtr(cf) + command_->offset;
}

char* ngx_command_ctx::GetModuleConfPtr(ngx_conf_t* cf) {
  return reinterpret_cast<char*>(
      ngx_http_conf_get_module_loc_conf(cf, (*(module_))));
}

void NgxGZipSetter::SetNgxConfFlag(ngx_conf_t* cf,
                                   ngx_command_ctx* command_ctx,
                                   ngx_flag_t value) {
  ngx_flag_t* flag = reinterpret_cast<ngx_flag_t*>(command_ctx->GetConfPtr(cf));
  *flag = value;
  // Save the flag position for possible rollback.
  ngx_flags_set_.push_back(flag);
}

void NgxGZipSetter::SetNgxConfEnum(ngx_conf_t* cf,
                                   ngx_command_ctx* command_ctx,
                                   ngx_uint_t value) {
  ngx_uint_t* enum_to_set =
      reinterpret_cast<ngx_uint_t*>(command_ctx->GetConfPtr(cf));
  *enum_to_set = value;
  ngx_uint_set_.push_back(enum_to_set);
}

void NgxGZipSetter::SetNgxConfBitmask(ngx_conf_t* cf,
                                      ngx_command_ctx* command_ctx,
                                      ngx_uint_t value) {
  ngx_uint_t* enum_to_set =
      reinterpret_cast<ngx_uint_t*>(command_ctx->GetConfPtr(cf));
  *enum_to_set = value;
  ngx_uint_set_.push_back(enum_to_set);
}

// These are the content types we want to compress.
ngx_str_t gzip_http_types[] = {
  ngx_string("application/ecmascript"),
  ngx_string("application/javascript"),
  ngx_string("application/json"),
  ngx_string("application/pdf"),
  ngx_string("application/postscript"),
  ngx_string("application/x-javascript"),
  ngx_string("image/svg+xml"),
  ngx_string("text/css"),
  ngx_string("text/csv"),
  // ngx_string("text/html"),  // This is the default implied value.
  ngx_string("text/javascript"),
  ngx_string("text/plain"),
  ngx_string("text/xml"),
  ngx_null_string  // Indicates end of array.
};

gzs_enable_result NgxGZipSetter::SetGZipForLocation(ngx_conf_t* cf,
                                                    bool value) {
  if (!enabled_) {
    return kEnableGZipNotEnabled;
  }
  if (gzip_command_.command_) {
    SetNgxConfFlag(cf, &gzip_command_, value);
  }
  return kEnableGZipOk;
}

void NgxGZipSetter::EnableGZipForLocation(ngx_conf_t* cf) {
  if (!enabled_) {
    return;
  }

  // When we get called twice for the same location{}, we ignore the second call
  // to prevent adding duplicate gzip http types and so on.
  ngx_flag_t* flag =
      reinterpret_cast<ngx_flag_t*>(gzip_command_.GetConfPtr(cf));
  if (*flag == 1) {
    return;
  }
  SetGZipForLocation(cf, true);
  if (gzip_vary_command_.command_) {
    SetNgxConfFlag(cf, &gzip_vary_command_, 1);
  }
  if (gzip_http_version_command_.command_) {
    SetNgxConfEnum(cf, &gzip_http_version_command_, NGX_HTTP_VERSION_10);
  }
  if (gzip_proxied_command_.command_) {
    SetNgxConfBitmask(
        cf, &gzip_http_version_command_, NGX_HTTP_GZIP_PROXIED_ANY);
  }

  // This is actually the most prone to future API changes, because gzip_types
  // is not a simple type like ngx_flag_t. The signature check should be enough
  // to prevent problems.
  AddGZipHTTPTypes(cf);
  return;
}

void NgxGZipSetter::AddGZipHTTPTypes(ngx_conf_t* cf) {
  if (gzip_http_types_command_.command_) {
    // Following should not happen, but if it does return gracefully.
    if (cf->args->nalloc < 2) {
      ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                         "pagespeed: unexpected small cf->args in gzip_types");
      return;
    }

    ngx_command_t* command = gzip_http_types_command_.command_;
    char* gzip_conf = reinterpret_cast<char* >(
        gzip_http_types_command_.GetModuleConfPtr(cf));

    // Backup the old settings.
    ngx_str_t old_elt0 = reinterpret_cast<ngx_str_t*>(cf->args->elts)[0];
    ngx_str_t old_elt1 = reinterpret_cast<ngx_str_t*>(cf->args->elts)[1];
    ngx_uint_t old_nelts = cf->args->nelts;

    // Setup first arg.
    ngx_str_t gzip_types_string = ngx_string("gzip_types");
    reinterpret_cast<ngx_str_t*>(cf->args->elts)[0] = gzip_types_string;
    cf->args->nelts = 2;

    ngx_str_t* http_types = gzip_http_types;
    while (http_types->data) {
      ngx_str_t d;
      // We allocate the http type on the configuration pool and actually
      // leak this if we rollback. This does not seem to be a big problem,
      // because nginx also allocates tokens in ngx_conf_file.c and does not
      // free them. This way they can be used safely by configurations.
      // We must use a copy of gzip_http_types array here because nginx will
      // manipulate the values.
      // TODO(kspoelstra): better would be to allocate once on init and not
      // every time we enable gzip. This needs further investigation, sharing
      // tokens might be problematic.
      // For now I think it is not a large problem. This might add up in case
      // of a large multi server/location config with a lot of "pagespeed on"
      // directives.
      // Estimates are 300-400KB for 1000 times "pagespeed on".
      d.data = reinterpret_cast<u_char*>(
          ngx_pnalloc(cf->pool, http_types->len + 1));
      snprintf(reinterpret_cast<char*>(d.data), http_types->len + 1, "%s",
               reinterpret_cast<const char*>(http_types->data));
      d.len = http_types->len;
      reinterpret_cast<ngx_str_t*>(cf->args->elts)[1] = d;
      // Call the original setter.
      ngx_http_types_slot(cf, command, gzip_conf);
      http_types++;
    }

    // Restore args.
    cf->args->nelts = old_nelts;
    reinterpret_cast<ngx_str_t*>(cf->args->elts)[1] = old_elt1;
    reinterpret_cast<ngx_str_t*>(cf->args->elts)[0] = old_elt0;

    // Backup configuration location for rollback.
    ngx_httptypes_set_.push_back(gzip_conf + command->offset);
  }
}

void NgxGZipSetter::RollBackAndDisable(ngx_conf_t* cf) {
  ngx_conf_log_error(NGX_LOG_INFO, cf, 0,
                     "pagespeed: rollback gzip, explicit configuration");
  for (std::vector<ngx_flag_t*>::iterator i = ngx_flags_set_.begin();
       i != ngx_flags_set_.end(); ++i) {
    *(*i)=NGX_CONF_UNSET;
  }
  for (std::vector<ngx_uint_t*>::iterator i = ngx_uint_set_.begin();
       i != ngx_uint_set_.end(); ++i) {
    *(*i)=NGX_CONF_UNSET_UINT;
  }
  for (std::vector<void*>::iterator i = ngx_httptypes_set_.begin();
       i != ngx_httptypes_set_.end(); ++i) {
    ngx_array_t** type_array = reinterpret_cast<ngx_array_t**>(*i);
    ngx_array_destroy(*type_array);
    *type_array = NULL;
  }
  enabled_ = 0;
}

}  // namespace net_instaweb
