/**
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
#include "net/instaweb/util/public/meta_data.h"
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
    : RewriteFilter(driver, filter_prefix),
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

void CacheExtender::StartElement(HtmlElement* element) {
  MessageHandler* message_handler = html_parse_->message_handler();
  HtmlElement::Attribute* href = tag_scanner_.ScanElement(element);
  if ((href != NULL) && html_parse_->IsRewritable(element)) {
    const char* origin_url = href->value();
    scoped_ptr<Resource> input_resource(
        resource_manager_->CreateInputResource(html_parse_->url(), origin_url,
                                               message_handler));

    // TODO(jmarantz): create an output resource to generate a new url,
    // rather than doing the content-hashing here.
    if ((input_resource != NULL) &&
        input_resource->IsCacheable() &&
        resource_manager_->ReadIfCached(input_resource.get(),
                                        message_handler)) {
      const MetaData* headers = input_resource->metadata();
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
        std::string trimmed_url;
        TrimWhitespace(origin_url, &trimmed_url);
        std::string url_safe_id;
        resource_manager_->url_escaper()->EncodeToUrlSegment(
            trimmed_url, &url_safe_id);
        scoped_ptr<OutputResource> output(
            resource_manager_->CreateNamedOutputResource(
                filter_prefix_, url_safe_id, input_resource->type(),
                message_handler));
        CHECK(!output->IsWritten());
        if (!output->HasValidUrl()) {
          StringPiece contents(input_resource->contents());
          std::string absolutified;
          if (input_resource->type() == &kContentTypeCss) {
            // TODO(jmarantz): find a mechanism to write this directly into
            // the HTTPValue so we can reduce the number of times that we
            // copy entire resources.
            StringWriter writer(&absolutified);
            CssTagScanner::AbsolutifyUrls(contents, input_resource->url(),
                                          &writer, message_handler);
            contents = absolutified;
          }
          resource_manager_->Write(HttpStatus::kOK, contents, output.get(),
                                   headers->CacheExpirationTimeMs(),
                                   message_handler);
        }
        if (output->IsWritten()) {
          href->SetValue(output->url());
          if (extension_count_ != NULL) {
            extension_count_->Add(1);
          }
        }
      }
    }
  }
}

bool CacheExtender::Fetch(OutputResource* output_resource,
                          Writer* writer,
                          const MetaData& request_headers,
                          MetaData* response_headers,
                          UrlAsyncFetcher* fetcher,
                          MessageHandler* message_handler,
                          UrlAsyncFetcher::Callback* callback) {
  std::string url, decoded_resource;
  bool ret = false;
  if (resource_manager_->url_escaper()->DecodeFromUrlSegment(
          output_resource->name(), &url)) {
    fetcher->StreamingFetch(url, request_headers, response_headers,
                            writer, message_handler, callback);
    ret = true;
  } else {
    output_resource->name().CopyToString(&url);
    message_handler->Error(url.c_str(), 0, "Unable to decode resource string");
  }
  return ret;
}

}  // namespace net_instaweb
