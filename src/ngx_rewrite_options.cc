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
#include "net/instaweb/public/version.h"

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

const char*
NgxRewriteOptions::ParseAndSetOptions(StringPiece* args, int n_args) {
  // TODO(jefftk): implement this.
  int i;
  fprintf(stderr, "Would set options from (");
  for (i = 0 ; i < n_args ; i++) {
    fprintf(stderr, "%s\"%s\"",
            i == 0 ? "" : ", ",
            args[i].as_string().c_str());
  }
  fprintf(stderr, ")\n");
  return NGX_CONF_OK;
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
