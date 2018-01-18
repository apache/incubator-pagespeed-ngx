/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */



/*
 * NgxGZipSetter sets up gzip for pagespeed
 * with the following configuration:
 * gzip  on;
 * gzip_vary on;
 * gzip_types application/ecmascript;
 * gzip_types application/javascript;
 * gzip_types application/json;
 * gzip_types application/pdf;
 * gzip_types application/postscript;
 * gzip_types application/x-javascript;
 * gzip_types image/svg+xml;
 * gzip_types text/css;
 * gzip_types text/csv;
 * gzip_types text/javascript;
 * gzip_types text/plain;
 * gzip_types text/xml;
 * gzip_http_version 1.0;
 *
 * If there is an explicit gzip configuration in the nginx.conf
 * pagespeed will rollback the set configuration and let the
 * user decide what the configuration will be.
 *
 * It manipulates the configuration by manipulating ngx_flag_t
 * and ngx_uint_t settings directly and using the nginx setter for
 * gzip_http_types.
 * This is probably a safe way to do it. If this mechanism
 * changes all non nginx module setup & configuration will
 * fail.
 */

#ifndef NGX_GZIP_SETTER_H_
#define NGX_GZIP_SETTER_H_

extern "C" {
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
}

#include <vector>

#include "ngx_pagespeed.h"

#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

// We need this class because configuration for gzip is in different modules, so
// just saving the command will not work.
class ngx_command_ctx {
 public:
  ngx_command_ctx() : command_(NULL), module_(NULL) { }
  void* GetConfPtr(ngx_conf_t* cf);
  char* GetModuleConfPtr(ngx_conf_t* cf);
  ngx_command_t* command_;
  ngx_module_t* module_;
};

enum gzs_enable_result {
  kEnableGZipOk,
  kEnableGZipPartial,
  kEnableGZipNotEnabled
};

class NgxGZipSetter {
  std::vector<ngx_flag_t*> ngx_flags_set_;
  std::vector<ngx_uint_t*> ngx_uint_set_;
  std::vector<void*> ngx_httptypes_set_;
  ngx_command_ctx gzip_command_;
  ngx_command_ctx gzip_http_types_command_;
  ngx_command_ctx gzip_proxied_command_;
  ngx_command_ctx gzip_vary_command_;
  ngx_command_ctx gzip_http_version_command_;
  bool enabled_;

 public:
  NgxGZipSetter();
  ~NgxGZipSetter();
  void Init(ngx_conf_t* cf);

  void SetNgxConfFlag(ngx_conf_t* cf,
                      ngx_command_ctx* command_ctx,
                      ngx_flag_t value);
  void SetNgxConfEnum(ngx_conf_t* cf,
                      ngx_command_ctx* command_ctx,
                      ngx_uint_t value);
  void SetNgxConfBitmask(ngx_conf_t* cf,
                         ngx_command_ctx* command_ctx,
                         ngx_uint_t value);
  void EnableGZipForLocation(ngx_conf_t* cf);
  gzs_enable_result SetGZipForLocation(ngx_conf_t* cf, bool value);
  void AddGZipHTTPTypes(ngx_conf_t* cf);
  void RollBackAndDisable(ngx_conf_t* cf);

  bool enabled() { return enabled_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NgxGZipSetter);
};

extern NgxGZipSetter g_gzip_setter;

}  // namespace net_instaweb

#endif  // NGX_GZIP_SETTER_H_
