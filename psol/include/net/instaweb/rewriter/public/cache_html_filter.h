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

// Authors: mmohabey@google.com (Megha Mohabey)
//          rahulbansal@google.com (Rahul Bansal)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CACHE_HTML_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CACHE_HTML_FILTER_H_

#include <vector>

#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/rewriter/cache_html_info.pb.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/json.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;
class RewriteOptions;

// This class extracts the non cacheable panels and sends it to the client.
// TODO(mmohabey): Integrate with SplitFilter and send non critical JSON too.
class CacheHtmlFilter : public HtmlWriterFilter {
 public:
  explicit CacheHtmlFilter(RewriteDriver* rewrite_driver);
  virtual ~CacheHtmlFilter();

  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual void EndDocument();
  void WriteString(StringPiece str);
  virtual void Flush();
  virtual const char* Name() const { return "CacheHtmlFilter"; }

 private:
  void SendCookies();
  void SendNonCacheableObject(const Json::Value& json);
  // Produces a custom xpath relative to the body or relative to the nearest
  // ancestor with an id (if there is one). Xpath comprises of the tag name
  // and the id (if it exists) or the position of the elements.
  GoogleString GetXpathOfCurrentElement(HtmlElement* element);

  RewriteDriver* rewrite_driver_;  // We do not own this.
  const RewriteOptions* rewrite_options_;  // We do not own this.
  AttributesToNonCacheableValuesMap attribute_non_cacheable_values_map_;
  std::vector<int> panel_number_num_instances_;
  GoogleString buffer_;
  StringWriter string_writer_;
  const HtmlElement* current_non_cacheable_element_;  // We do not own this.
  GoogleString current_panel_id_;
  const PropertyCache::Cohort* cohort_;  // We do not own this.
  CacheHtmlInfo cache_html_info_;
  bool abort_filter_;
  std::vector<int> num_children_stack_;

  DISALLOW_COPY_AND_ASSIGN(CacheHtmlFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CACHE_HTML_FILTER_H_
