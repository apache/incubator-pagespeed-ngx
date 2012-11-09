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

// s1: StringPiece, s2: string literal
// true if they're equal ignoring case, false otherwise
#define STRP_CEQ_LITERAL(s1, s2)             \
    ((s1).size() == (sizeof(s2)-1) &&        \
     ngx_strncasecmp((u_char*)((s1).data()), \
                     (u_char*)(s2),          \
                     (sizeof(s2)-1)) == 0)

// WARNING: leaky macro.  Assumes that the directive to process is args[0].
// This simplifies ParseAndSetOptions a lot, but is a little tricky.
#define DIRECTIVE_IS(x) STRP_CEQ_LITERAL(args[0], (x))
const char*
NgxRewriteOptions::ParseAndSetOptions(StringPiece* args, int n_args) {
  CHECK(n_args >= 1);

  int i;
  fprintf(stderr, "Setting option from (");
  for (i = 0 ; i < n_args ; i++) {
    fprintf(stderr, "%s\"%s\"",
            i == 0 ? "" : ", ",
            args[i].as_string().c_str());
  }
  fprintf(stderr, ")\n");

  if (DIRECTIVE_IS("on")) {
    set_enabled(true);
  } else if (DIRECTIVE_IS("off")) {
    set_enabled(false);
  }  // Many more DIRECTIVE_IS statements go here.
  else {
    return "unknown option";
  }

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
