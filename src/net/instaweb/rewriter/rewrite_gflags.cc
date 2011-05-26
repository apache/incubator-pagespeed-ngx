/*
 * Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/rewrite_gflags.h"

#include <vector>

#include "base/logging.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gflags.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

using namespace google;  // NOLINT

DEFINE_string(filename_prefix, "", "Filesystem prefix for storing resources.");

DEFINE_string(rewriters, "", "Comma-separated list of rewriters");
DEFINE_string(domains, "", "Comma-separated list of domains");

DEFINE_int64(css_outline_min_bytes,
             net_instaweb::RewriteOptions::kDefaultCssOutlineMinBytes,
             "Number of bytes above which inline "
             "CSS resources will be outlined.");
DEFINE_int64(js_outline_min_bytes,
             net_instaweb::RewriteOptions::kDefaultJsOutlineMinBytes,
             "Number of bytes above which inline "
             "Javascript resources will be outlined.");
DEFINE_int64(image_inline_max_bytes,
             net_instaweb::RewriteOptions::kDefaultImageInlineMaxBytes,
             "Number of bytes below which images will be inlined.");
DEFINE_int64(js_inline_max_bytes,
             net_instaweb::RewriteOptions::kDefaultJsInlineMaxBytes,
             "Number of bytes below which javascript will be inlined.");
DEFINE_int64(css_inline_max_bytes,
             net_instaweb::RewriteOptions::kDefaultCssInlineMaxBytes,
             "Number of bytes below which stylesheets will be inlined.");
DEFINE_int32(image_max_rewrites_at_once,
             net_instaweb::RewriteOptions::kDefaultImageMaxRewritesAtOnce,
             "Maximum number of images that will be rewritten simultaneously.");
DEFINE_bool(log_rewrite_timing, false, "Log time taken by rewrite filters.");

DEFINE_int32(lru_cache_size_bytes, 10 * 1000 * 1000, "LRU cache size");
DEFINE_bool(force_caching, false,
            "Ignore caching headers and cache everything.");

namespace net_instaweb {

class MessageHandler;

RewriteGflags::RewriteGflags(const char* progname, int* argc, char*** argv) {
  ParseCommandLineFlags(argc, argv, true);
}

bool RewriteGflags::SetOptions(RewriteDriverFactory* factory) const {
  bool ret = true;
  factory->set_filename_prefix(FLAGS_filename_prefix);
  factory->set_force_caching(FLAGS_force_caching);

  RewriteOptions* options = factory->options();
  if (WasExplicitlySet("css_outline_min_bytes")) {
    options->set_css_outline_min_bytes(FLAGS_css_outline_min_bytes);
  }
  if (WasExplicitlySet("js_outline_min_bytes")) {
    options->set_js_outline_min_bytes(FLAGS_js_outline_min_bytes);
  }
  if (WasExplicitlySet("image_inline_max_bytes")) {
    options->set_image_inline_max_bytes(FLAGS_image_inline_max_bytes);
  }
  if (WasExplicitlySet("css_inline_max_bytes")) {
    options->set_css_inline_max_bytes(FLAGS_css_inline_max_bytes);
  }
  if (WasExplicitlySet("js_inline_max_bytes")) {
    options->set_js_inline_max_bytes(FLAGS_js_inline_max_bytes);
  }
  if (WasExplicitlySet("image_max_rewrites_at_once")) {
    options->set_image_max_rewrites_at_once(
        FLAGS_image_max_rewrites_at_once);
  }
  if (WasExplicitlySet("log_rewrite_timing")) {
    options->set_log_rewrite_timing(FLAGS_log_rewrite_timing);
  }

  MessageHandler* handler = factory->message_handler();
  if (!options->EnableFiltersByCommaSeparatedList(FLAGS_rewriters, handler)) {
    LOG(ERROR) << "Invalid filters-list: " << FLAGS_rewriters;
    ret = false;
  }
  std::vector<StringPiece> domains;
  SplitStringPieceToVector(FLAGS_domains, ",", &domains, true);
  DomainLawyer* lawyer = options->domain_lawyer();
  for (int i = 0, n = domains.size(); i < n; ++i) {
    if (!lawyer->AddDomain(domains[i], handler)) {
      LOG(ERROR) << "Invalid domain: " << domains[i];
      ret = false;
    }
  }
  return ret;
}

// prefix with "::" is needed because of 'using namespace google' above.
::int64 RewriteGflags::lru_cache_size_bytes() const {
  return FLAGS_lru_cache_size_bytes;
}

bool RewriteGflags::WasExplicitlySet(const char* name) const {
  CommandLineFlagInfo flag_info;
  CHECK(GetCommandLineFlagInfo(name, &flag_info));
  return !flag_info.is_default;
}

}  // namespace
