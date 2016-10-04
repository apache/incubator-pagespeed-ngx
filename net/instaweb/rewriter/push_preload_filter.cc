/*
 * Copyright 2016 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
//
// This implements a filter which generate HTTP2 push or preload fetch hints.
// (e.g. Link: <foo>; rel=preload HTTP headers). Over HTTP2 with mod_http2
// or h2o this will result in a push (if the server is authoritative for the
// resource host); some clients (Chrome 50 as of writing) will also interpret
// it as a hint to preload the resource regardless of protocol.
// http://w3c.github.io/preload is the spec that provides for both behaviors.

#include "net/instaweb/rewriter/public/push_preload_filter.h"

#include <algorithm>
#include <unordered_set>
#include <utility>                      // for pair

#include "base/logging.h"
#include "net/instaweb/rewriter/dependencies.pb.h"
#include "net/instaweb/rewriter/public/collect_dependencies_filter.h"
#include "net/instaweb/rewriter/public/dependency_tracker.h"
#include "net/instaweb/rewriter/public/input_info_utils.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

PushPreloadFilter::PushPreloadFilter(RewriteDriver* rewrite_driver)
    : CommonFilter(rewrite_driver) {
}

PushPreloadFilter::~PushPreloadFilter() {}

void PushPreloadFilter::StartDocumentImpl() {
  ResponseHeaders* headers = driver()->mutable_response_headers();

  // This is something of a workaround, see comments in
  // PushPreloadFilterTest.WeirdTiming.
  if (headers == nullptr) {
    return;
  }

  const Dependencies* deps = driver()->dependency_tracker()->read_in_info();
  CHECK(deps != nullptr) << "DetermineEnabled should have prevented this";

  // Sort dependencies by order_key.
  std::vector<Dependency> ordered_deps;
  for (int i = 0, n = deps->dependency_size(); i < n; ++i) {
    ordered_deps.push_back(deps->dependency(i));
  }
  DependencyOrderCompator dep_order;
  std::sort(ordered_deps.begin(), ordered_deps.end(), dep_order);

  std::unordered_set<GoogleString> already_seen;

  for (const Dependency& dep : ordered_deps) {
    GoogleUrl dep_url(dep.url());

    if (!dep_url.IsWebValid()) {
      continue;
    }

    if (!already_seen.insert(dep.url()).second) {
      // Skip dupes.
      continue;
    }

    // See if all the inputs are valid.
    int64 now_ms = driver()->timer()->NowMs();
    for (int i = 0; i < dep.validity_info_size(); ++i) {
      const InputInfo& input = dep.validity_info(i);
      bool purged_ignored, stale_rewrite_ignored;
      if (!input_info_utils::IsInputValid(
            server_context(), rewrite_options(), false /* not nested_rewriter*/,
            input, now_ms, &purged_ignored, &stale_rewrite_ignored)) {
        // Stop at first invalid entry, to avoid out-of-order hints.
        return;
      }
    }

    StringPiece rel_url =
        dep_url.Relativize(kAbsolutePath, driver()->google_url());

    GoogleString link_val =
        StrCat("<", GoogleUrl::Sanitize(rel_url), ">; rel=preload");

    switch (dep.content_type()) {
      case DEP_JAVASCRIPT:
        StrAppend(&link_val, "; as=script");
        break;
      case DEP_CSS:
        StrAppend(&link_val, "; as=style");
        break;
      default:
        LOG(DFATAL) << dep.content_type();
    }

    // We don't want pushes now, since we can't tell for sure when they're
    // a good idea.
    StrAppend(&link_val, "; nopush");

    headers->Add(HttpAttributes::kLink, link_val);
  }
}

void PushPreloadFilter::DetermineEnabled(GoogleString* disabled_reason) {
  if (driver()->dependency_tracker()->read_in_info() == nullptr) {
    set_is_enabled(false);
    *disabled_reason = "No push/preload candidates found in pcache";
  } else {
    set_is_enabled(true);
  }
}

}  // namespace net_instaweb
