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

// Author: nforman@google.com (Naomi Forman)
//
// This provides the MetaTagFilter class which converts meta tags in html with
// response headers.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_META_TAG_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_META_TAG_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class HtmlElement;
class ResponseHeaders;
class RewriteDriver;
class Statistics;
class Variable;

// This class is the implementation of convert_meta_tags filter, which removes
// meta tags from the html and replaces them with a corresponding
// response header.
class MetaTagFilter : public CommonFilter {
 public:
  explicit MetaTagFilter(RewriteDriver* rewrite_driver);
  virtual ~MetaTagFilter();

  static void Initialize(Statistics* stats);

  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element) {}

  // Rewrite tags in the following form:
  // <meta http-equiv="Content-Type" content="text/html" >
  // into response headers.
  // In theory we could delete the tag, but since it is somewhat
  // "dangerous" to mutate the html (in case a script is looking for something),
  // we leave the tag in there.  As long as the tags and the headers match,
  // there should not be a performance hit.
  virtual void EndElementImpl(HtmlElement* element);
  virtual void Flush();

  virtual const char* Name() const { return "ConvertMetaTags"; }

  // Utility function to extract the mime type and/or charset from a meta tag
  // and update the response_headers if they are not set already.
  static bool ExtractAndUpdateMetaTagDetails(
      HtmlElement* element,
      ResponseHeaders* response_headers);

 private:
  ResponseHeaders* response_headers_;

  // Stats on how many tags we moved.
  Variable* converted_meta_tag_count_;

  DISALLOW_COPY_AND_ASSIGN(MetaTagFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_META_TAG_FILTER_H_
