/*
 * Copyright 2013 Google Inc.
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

// Author: matterbury@google.com (Matt Atterbury)

#include "net/instaweb/rewriter/public/dedup_inlined_images_filter.h"

#include <map>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "pagespeed/kernel/http/request_headers.h"

namespace net_instaweb {

const unsigned int DedupInlinedImagesFilter::kMinimumImageCutoff = 160;

const char DedupInlinedImagesFilter::kDiiInitializer[] =
    "pagespeed.dedupInlinedImagesInit();";

const char DedupInlinedImagesFilter::kCandidatesFound[] =
    "num_dedup_inlined_images_candidates_found";
const char DedupInlinedImagesFilter::kCandidatesReplaced[] =
    "num_dedup_inlined_images_candidates_replaced";

DedupInlinedImagesFilter::DedupInlinedImagesFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      script_inserted_(false),
      snippet_id_(0) {
  Statistics* stats = server_context()->statistics();
  num_dedup_inlined_images_candidates_found_ =
      stats->GetVariable(kCandidatesFound);
  num_dedup_inlined_images_candidates_replaced_ =
      stats->GetVariable(kCandidatesReplaced);
}

DedupInlinedImagesFilter::~DedupInlinedImagesFilter() {
  hash_to_id_map_.clear();
}

void DedupInlinedImagesFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(DedupInlinedImagesFilter::kCandidatesFound);
  statistics->AddVariable(DedupInlinedImagesFilter::kCandidatesReplaced);
}

void DedupInlinedImagesFilter::DetermineEnabled() {
  // We are treating this filter like a version of lazyload images because
  // they both replace an image with JavaScript, and in both cases we need
  // to disable the filter for certain classes of UA.
  if (!driver()->request_properties()->SupportsLazyloadImages() ||
      driver()->flushing_early() ||
      (driver()->request_headers() != NULL &&
       driver()->request_headers()->IsXmlHttpRequest())) {
    set_is_enabled(false);
  }
}

void DedupInlinedImagesFilter::StartDocumentImpl() {
  script_inserted_ = false;
  snippet_id_ = 0;
}

void DedupInlinedImagesFilter::EndDocument() {
  hash_to_id_map_.clear();
}

void DedupInlinedImagesFilter::StartElementImpl(HtmlElement* element) {
  // If this is an inlined image that we've seen before, we will replace it
  // with JS in EndElementImpl. Before we do that for the first time we need
  // to insert our JS script of functions, though not if we're inside a
  // <noscript> as that would be dumb.
  if (!script_inserted_) {
    StringPiece src;
    if (IsDedupCandidate(element, &src)) {
      GoogleString hash = server_context()->hasher()->Hash(src);
      if (hash_to_id_map_.find(hash) != hash_to_id_map_.end()) {
        InsertOurScriptElement(element);
      }
    }
  }
}

void DedupInlinedImagesFilter::EndElementImpl(HtmlElement* element) {
  StringPiece src;
  if (IsDedupCandidate(element, &src)) {
    num_dedup_inlined_images_candidates_found_->Add(1);
    GoogleString hash = server_context()->hasher()->Hash(src);
    if (hash_to_id_map_.find(hash) == hash_to_id_map_.end()) {
      // The first time we've seen it: we need to ensure it has an id.
      // TODO(matterbury): We could check if an id is used more than once and
      // refuse to deduplicate it if so. We'd need to check ALL images at least
      // though to be correct we should check all tags; this seems like a lot
      // of work to cater for something people tend not to do (because it's
      // such a bad idea basically).
      const char* id = element->AttributeValue(HtmlName::kId);
      if (id == NULL || id[0] == '\0') {
        GoogleString img_id("pagespeed_img_" + hash);
        hash_to_id_map_[hash] = img_id;
        driver()->AddAttribute(element, HtmlName::kId, img_id);
      } else {
        hash_to_id_map_[hash] = id;
      }
    } else {
      // A subsequent use of an already inlined image: dedup it!
      DCHECK(script_inserted_);
      num_dedup_inlined_images_candidates_replaced_->Add(1);
      GoogleString img_id = hash_to_id_map_[hash];
      GoogleString script_id = StrCat("pagespeed_script_",
                                      IntegerToString(++snippet_id_));
      // NOTE: If you change this you need to update kMinimumImageCutoff,
      // which is currently set to 160, slightly less than this snippet:
      //   <script type="text/javascript" id="pagespeed_script_1"
      //    pagespeed_no_defer>
      //   pagespeed.dedupInlinedImages.inlineImg("pagespeed_img_12345678",
      //                                          "pagespeed_script_1");
      //   </script>
      GoogleString snippet("pagespeed.dedupInlinedImages.");
      StrAppend(&snippet, "inlineImg(\"", img_id, "\",\"", script_id, "\");");
      HtmlElement* script = driver()->NewElement(element, HtmlName::kScript);
      driver()->InsertElementAfterElement(element, script);
      driver()->server_context()->static_asset_manager()->AddJsToElement(
          snippet, script, driver());
      driver()->AddAttribute(script, HtmlName::kId, script_id);
      script->AddAttribute(driver()->MakeName(HtmlName::kPagespeedNoDefer),
                           NULL, HtmlElement::NO_QUOTE);
      element->DeleteAttribute(HtmlName::kSrc);
    }
  }
}

bool DedupInlinedImagesFilter::IsDedupCandidate(HtmlElement* element,
                                                StringPiece* src_iff_true) {
  // Ignore images inside a <noscript> as inserting any JS is pointless.
  // Ignore images that aren't inlined (a data URI).
  // Ignore images that are smaller than the cutoff, current set to roughly
  // the size of the JS snippet we insert (ignoring the functions JS overhead).
  // TODO(matterbury): Also handle input tags.
  if (noscript_element() == NULL && element->keyword() == HtmlName::kImg) {
    const StringPiece src(element->AttributeValue(HtmlName::kSrc));
    if (IsDataImageUrl(src) && src.size() > kMinimumImageCutoff) {
      *src_iff_true = src;
      return true;
    }
  }
  return false;
}

void DedupInlinedImagesFilter::InsertOurScriptElement(HtmlElement* before) {
  StaticAssetManager* static_asset_manager =
      server_context()->static_asset_manager();
  StringPiece dedup_inlined_images_js =
      static_asset_manager->GetAsset(
          StaticAssetManager::kDedupInlinedImagesJs, driver()->options());
  const GoogleString& initialized_js = StrCat(dedup_inlined_images_js,
                                              kDiiInitializer);
  HtmlElement* script_element = driver()->NewElement(before->parent(),
                                                     HtmlName::kScript);
  driver()->InsertElementBeforeElement(before, script_element);
  static_asset_manager->AddJsToElement(
      initialized_js, script_element, driver());
  script_element->AddAttribute(driver()->MakeName(HtmlName::kPagespeedNoDefer),
                               NULL, HtmlElement::NO_QUOTE);
  script_inserted_ = true;
}

}  // namespace net_instaweb
