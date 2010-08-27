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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_IMG_REWRITE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_IMG_REWRITE_FILTER_H_

#include "net/instaweb/rewriter/public/rewrite_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/img_tag_scanner.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/util/public/atom.h"
#include <string>

namespace net_instaweb {

class ContentType;
class FileSystem;
class HtmlParse;
class Image;
class ImgRewriteUrl;
class OutputResource;
class ResourceManager;
class Variable;

// Identify img tags in html and optimize them.
// TODO(jmaessen): See which ones have immediately-obvious size info.
// TODO(jmaessen): Provide alternate resources at rewritten urls
//     asynchronously somehow.
// TODO(jmaessen): Big open question: how best to link pulled-in resources to
//     rewritten urls, when in general those urls will be in a different domain.
class ImgRewriteFilter : public RewriteFilter {
 public:
  ImgRewriteFilter(StringPiece path_prefix,
                   HtmlParse* html_parse,
                   ResourceManager* resource_manager,
                   FileSystem* file_system);
  virtual void EndElement(HtmlElement* element);
  virtual void Flush();
  virtual bool Fetch(OutputResource* resource,
                     Writer* writer,
                     const MetaData& request_header,
                     MetaData* response_headers,
                     UrlAsyncFetcher* fetcher,
                     MessageHandler* message_handler,
                     UrlAsyncFetcher::Callback* callback);
  virtual const char* Name() const { return "ImgRewrite"; }

 private:
  // Helper methods.
  Image* GetImage(const ImgRewriteUrl& url_proto, Resource* img_resource);
  OutputResource* ImageOutputResource(const std::string& url_string,
                                      Image* image);
  void OptimizeImage(
      const Resource* input_resource, const ImgRewriteUrl& url_proto,
      Image* image, OutputResource* result);
  OutputResource* OptimizedImageFor(
      const ImgRewriteUrl& url_proto, const std::string& url_string,
      Resource* input_resource);
  void RewriteImageUrl(const HtmlElement& element, HtmlElement::Attribute* src);

  FileSystem* file_system_;
  HtmlParse* html_parse_;
  scoped_ptr<ImgTagScanner> img_filter_;
  ResourceManager* resource_manager_;
  const Atom s_width_;
  const Atom s_height_;
  Variable* rewrite_count_;
  Variable* inline_count_;
  Variable* rewrite_saved_bytes_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IMG_REWRITE_FILTER_H_
