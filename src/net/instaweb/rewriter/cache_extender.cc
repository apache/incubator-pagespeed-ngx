/*
 * Copyright 2010 Google Inc.
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

#include "net/instaweb/rewriter/public/cache_extender.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/statistics.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace {

// names for Statistics variables.
const char kCacheExtensions[] = "cache_extensions";
const char kNotCacheable[] = "not_cacheable";

}  // namespace

namespace net_instaweb {

// We do not want to bother to extend the cache lifetime for any resource
// that is already cached for a month.
const int64 kMinThresholdMs = Timer::kMonthMs;

// TODO(jmarantz): consider factoring out the code that finds external resources

CacheExtender::CacheExtender(RewriteDriver* driver, const char* filter_prefix)
    : RewriteSingleResourceFilter(driver, filter_prefix),
      html_parse_(driver->html_parse()),
      resource_manager_(driver->resource_manager()),
      tag_scanner_(html_parse_),
      extension_count_(NULL),
      not_cacheable_count_(NULL) {
  Statistics* stats = resource_manager_->statistics();
  if (stats != NULL) {
    extension_count_ = stats->GetVariable(kCacheExtensions);
    not_cacheable_count_ = stats->GetVariable(kNotCacheable);
  }
}

void CacheExtender::Initialize(Statistics* statistics) {
  statistics->AddVariable(kCacheExtensions);
  statistics->AddVariable(kNotCacheable);
}

void CacheExtender::StartElementImpl(HtmlElement* element) {
  MessageHandler* message_handler = html_parse_->message_handler();
  HtmlElement::Attribute* href = tag_scanner_.ScanElement(element);
  if ((href != NULL) && html_parse_->IsRewritable(element)) {
    scoped_ptr<Resource> input_resource(CreateInputResource(href->value()));
    if ((input_resource.get() != NULL) &&
        !IsRewrittenResource(input_resource->url()) &&
        resource_manager_->ReadIfCached(input_resource.get(),
                                        message_handler)) {
      const ResponseHeaders* headers = input_resource->metadata();
      int64 now_ms = resource_manager_->timer()->NowMs();

      // We cannot cache-extend a resource that's completely uncacheable,
      // as our serving-side image would b
      if (!resource_manager_->http_cache()->force_caching() &&
          !headers->IsCacheable()) {
        if (not_cacheable_count_ != NULL) {
          not_cacheable_count_->Add(1);
        }
      } else if (((headers->CacheExpirationTimeMs() - now_ms) <
                  kMinThresholdMs) &&
                 (input_resource->type() != NULL)) {
        scoped_ptr<OutputResource> output(
            resource_manager_->CreateOutputResourceFromResource(
                filter_prefix_, input_resource->type(),
                resource_manager_->url_escaper(), input_resource.get(),
                message_handler));
        if (output.get() != NULL) {
          CHECK(!output->IsWritten());

          // TODO(sligocki): Shouldn't we be rewriting if !IsWritten?
          if (!output->HasValidUrl()) {
            // Transfer the input resource to output resource.
            RewriteLoadedResource(input_resource.get(), output.get());
          }

          if (output->HasValidUrl()) {
            href->SetValue(output->url());
            if (extension_count_ != NULL) {
              extension_count_->Add(1);
            }
          }
        }
      }
    }
  }
}

// Just based on the pattern of the URL, see if we think this was
// already the result of a rewrite.  It should, in general, be
// functionally correct to apply a new filter to an
// already-rewritten resoure.  However, in the case of cache
// extension, there is no benefit because every rewriter generates
// URLs that are served with long cache lifetimes.  This filter
// just wants to pick up the scraps.  Note that we would discover
// this anywahy in the cache expiration time below, but it's worth
// going to the extra trouble to reduce the cache lookups since this
// happens for basically every resource.
bool CacheExtender::IsRewrittenResource(const StringPiece& url) const {
  RewriteFilter* filter;
  scoped_ptr<OutputResource> output_resource(driver_->DecodeOutputResource(
      url, &filter));
  return (output_resource.get() != NULL);
}

bool CacheExtender::RewriteLoadedResource(const Resource* input_resource,
                                          OutputResource* output_resource) {
  CHECK(input_resource->loaded());

  MessageHandler* message_handler = html_parse_->message_handler();
  StringPiece contents(input_resource->contents());
  std::string absolutified;
  std::string input_dir =
      GoogleUrl::AllExceptLeaf(GoogleUrl::Create(input_resource->url()));
  if ((input_resource->type() == &kContentTypeCss) &&
      (input_dir != output_resource->resolved_base())) {
    // TODO(jmarantz): find a mechanism to write this directly into
    // the HTTPValue so we can reduce the number of times that we
    // copy entire resources.
    StringWriter writer(&absolutified);
    CssTagScanner::AbsolutifyUrls(contents, input_resource->url(),
                                  &writer, message_handler);
    contents = absolutified;
  }
  // TODO(sligocki): Should we preserve the response headers from the
  // original resource?
  // TODO(sligocki): Maybe we shouldn't cache the rewritten resource,
  // just the input_resource.
  return resource_manager_->Write(
      HttpStatus::kOK, contents, output_resource,
      input_resource->metadata()->CacheExpirationTimeMs(), message_handler);
}

}  // namespace net_instaweb
