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

// Author: guptaa@google.com (Ashish Gupta)

#include "net/instaweb/rewriter/public/javascript_url_manager.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char JavascriptUrlManager::kGStaticBase[] =
    "http://www.gstatic.com/psa/static/";

const char JavascriptUrlManager::kBlinkGstaticSuffix[] = "-blink.js";

const char JavascriptUrlManager::kBlinkRelativePath[] = "/psajs/blink.js";

JavascriptUrlManager::JavascriptUrlManager(
    UrlNamer* url_namer,
    bool serve_js_from_gstatic,
    const GoogleString& blink_hash)
    : url_namer_(url_namer),
      serve_js_from_gstatic_(serve_js_from_gstatic) {
  InitBlink(blink_hash);
}

JavascriptUrlManager::~JavascriptUrlManager() {
}

const GoogleString& JavascriptUrlManager::GetBlinkJsUrl(
    RewriteOptions* options) const {
  if (serve_js_from_gstatic_ && !options->Enabled(RewriteOptions::kDebug)) {
    return blink_javascript_gstatic_url_;
  }
  return blink_javascript_handler_url_;
}

void JavascriptUrlManager::InitBlink(const GoogleString& hash) {
  if (serve_js_from_gstatic_) {
    CHECK(!hash.empty());
    blink_javascript_gstatic_url_ =
        StrCat(kGStaticBase, hash, kBlinkGstaticSuffix);
  }
  blink_javascript_handler_url_ =
      StrCat(url_namer_->get_proxy_domain(), kBlinkRelativePath);
}

}  // namespace net_instaweb
