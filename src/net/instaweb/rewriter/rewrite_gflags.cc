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

#include "base/logging.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gflags.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

using namespace google;  // NOLINT

// TODO(sligocki): What is this used for these days?
DEFINE_string(filename_prefix, "/tmp/instaweb/",
              "Filesystem prefix for storing resources.");

DEFINE_string(rewrite_level, "CoreFilters",
              "Base rewrite level. Must be one of: "
              "PassThrough, CoreFilters, TestingCoreFilters, AllFilters.");
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
DEFINE_int64(html_cache_time_ms,
             net_instaweb::RewriteOptions::kDefaultHtmlCacheTimeMs,
             "Default Cache-Control TTL for HTML. This will be the max we "
             "set HTML TTL and also the min input resource TTL we allow "
             "rewriting for.");
DEFINE_string(rewrite_domain_map, "",
              "Semicolon-separated list of rewrite_domain maps. "
              "Each domain-map is of the form dest=src1,src2,src3");
DEFINE_string(shard_domain_map, "",
              "Semicolon-separated list of shard_domain maps. "
              "Each domain-map is of the form master=shard1,shard2,shard3");

DEFINE_int32(lru_cache_size_bytes, 10 * 1000 * 1000, "LRU cache size");
DEFINE_bool(force_caching, false,
            "Ignore caching headers and cache everything.");
DEFINE_bool(flush_html, false, "Pass fetcher-generated flushes through HTML");

namespace net_instaweb {

class MessageHandler;

namespace {

#define CALL_MEMBER_FN(object, var) (object->*(var))

bool AddDomainMap(const StringPiece& flag_value, DomainLawyer* lawyer,
                  bool (DomainLawyer::*fn)(const StringPiece& to_domain,
                                           const StringPiece& from,
                                           MessageHandler* handler),
                  MessageHandler* message_handler) {
  bool ret = true;
  StringPieceVector maps;
  // split "a=b,c,d=e:g,f" by semicolons.
  SplitStringPieceToVector(flag_value, ";", &maps, true);
  for (int i = 0, n = maps.size(); i < n; ++i) {
    // parse "a=b,c,d" into "a" and "b,c,d"
    StringPieceVector name_values;
    SplitStringPieceToVector(maps[i], "=", &name_values, true);
    if (name_values.size() != 2) {
      message_handler->Message(kError, "Invalid rewrite_domain_map: %s",
                               maps[i].as_string().c_str());
      ret = false;
    } else {
      ret &= CALL_MEMBER_FN(lawyer, fn)(name_values[0], name_values[1],
                                        message_handler);
    }
  }
  return ret;
}

}  // namespace

RewriteGflags::RewriteGflags(const char* progname, int* argc, char*** argv) {
  ParseCommandLineFlags(argc, argv, true);
}

bool RewriteGflags::SetOptions(RewriteDriverFactory* factory,
                               RewriteOptions* options) const {
  bool ret = true;
  factory->set_filename_prefix(FLAGS_filename_prefix);
  factory->set_force_caching(FLAGS_force_caching);

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
  if (WasExplicitlySet("html_cache_time_ms")) {
    options->set_html_cache_time_ms(FLAGS_html_cache_time_ms);
  }
  if (WasExplicitlySet("flush_html")) {
    options->set_flush_html(FLAGS_flush_html);
  }

  RewriteOptions::RewriteLevel rewrite_level;
  if (options->ParseRewriteLevel(FLAGS_rewrite_level, &rewrite_level)) {
    options->SetRewriteLevel(rewrite_level);
  } else {
    LOG(ERROR) << "Invalid --rewrite_level: " << FLAGS_rewrite_level;
    ret = false;
  }

  MessageHandler* handler = factory->message_handler();
  if (!options->EnableFiltersByCommaSeparatedList(FLAGS_rewriters, handler)) {
    LOG(ERROR) << "Invalid filters-list: " << FLAGS_rewriters;
    ret = false;
  }

  StringPieceVector domains;
  SplitStringPieceToVector(FLAGS_domains, ",", &domains, true);
  DomainLawyer* lawyer = options->domain_lawyer();
  for (int i = 0, n = domains.size(); i < n; ++i) {
    if (!lawyer->AddDomain(domains[i], handler)) {
      LOG(ERROR) << "Invalid domain: " << domains[i];
      ret = false;
    }
  }

  if (WasExplicitlySet("rewrite_domain_map")) {
    ret &= AddDomainMap(FLAGS_rewrite_domain_map, lawyer,
                        &DomainLawyer::AddRewriteDomainMapping, handler);
  }

  if (WasExplicitlySet("shard_domain_map")) {
    ret &= AddDomainMap(FLAGS_shard_domain_map, lawyer,
                        &DomainLawyer::AddShard, handler);
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
