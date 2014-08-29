/*
 * Copyright 2014 Google Inc.
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

// Author: stevensr@google.com (Ryan Stevens)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_REWRITE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_REWRITE_FILTER_H_

#include <vector>

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/util/public/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class HtmlCharactersNode;
class HtmlElement;
class RewriteDriver;
class Statistics;
class Variable;

// A mobile role and its associated HTML attribute value.
struct MobileRole {
  enum Level {
    // Tags which aren't explicitly tagged with a data-mobile-role attribute,
    // but we want to keep anyway, such as <style> or <script> tags in the body.
    kKeeper = 0,
    // The page header, such as <h1> or logos.
    kHeader,
    // Nav sections of the page. The HTML of nav blocks will be completely
    // rewritten to be mobile friendly by deleting unwanted elements in the
    // block.
    kNavigational,
    // Main content of the page.
    kContent,
    // Any block that isn't one of the above. Marginal content is put at the end
    // and remains pretty much untouched with respect to modifying HTML or
    // styling.
    kMarginal,
    // Elements without a data-mobile-role attribute, or with an unknown
    // attribute value, will be kInvalid.
    kInvalid
  };

  static const MobileRole kMobileRoles[kInvalid];

  MobileRole(Level level, const char* value)
      : level(level),
        value(value) { }

  static const MobileRole* FromString(const StringPiece& mobile_role);
  static Level LevelFromString(const StringPiece& mobile_role);
  static const char* StringFromLevel(Level level) {
    return (level < kInvalid) ? kMobileRoles[level].value : NULL;
  }

  const Level level;
  const char* const value;  // Set to a static string in cc.
};

// Rewrite HTML to be mobile-friendly based on "data-mobile-role" attributes in
// the HTML tags. To reorganize the DOM, the filter puts containers at the end
// of the body into which we move tagged elements. The containers are later
// removed after the filter is done processing the document body. The filter
// applies the following transformations:
//  - Add mobile <style> and <meta name="viewport"...> tags to the head.
//  - Remove all table tags (but keep the content). Almost all tables in desktop
//    HTML are for formatting, not displaying data, and they tend not to resize
//    well for mobile. The easiest thing to do is to simply strip out the
//    formatting and hope the content reflows properly.
//  - Reorder body of the HTML DOM elements based on mobile role. Any elements
//    which don't have an important parent will get removed, except for a
//    special set of "keeper" tags (like <script> or <style>). The keeper tags
//    are retained because they are often necessary for the website to work
//    properly, and because they have no visible appearance on the page.
//  - Remove all elements from inside data-mobile-role="navigational" elements
//    except in a special set of nav tags (notably <a>). Nav sections often do
//    not resize well due to fixed width formatting and drop-down menus, so it
//    is often necessary to pull out what you want, instead of shuffling around
//    what is there.
//
// Remaining todos:
//  - TODO (stevensr): This script does not handle flush windows in the body.
//  - TODO (stevensr): It would be nice to tweak the table-xform behavior via
//    options. Also, there has been mention that removing tables across flush
//    windows could be problematic. This should be addressed at some point.
//  - TODO (stevensr): Enable this filter only for mobile UAs, and have a query
//    param option to turn it on for all UAs for debugging.
//  - TODO (stevensr): Write pcache entry if rewriting page fails. We should
//    then probably inject some JS to auto-refresh the page so the user does not
//    see the badly rewritten result.
//  - TODO (stevensr): Add a separate wildcard option to allow/disallow URLs
//    from using this filter. Of course sites can use our existing Allow and
//    Disallow directives but that turns off all optimizations, and this one is
//    one that might be extra finicky (e.g. don't touch my admin pages).
//  - TODO (stevensr): Turn on css_move_to_head_filter.cc to reorder elements
//    we inject into the head.
class MobilizeRewriteFilter : public EmptyHtmlFilter {
 public:
  static const char kPagesMobilized[];
  static const char kKeeperBlocks[];
  static const char kHeaderBlocks[];
  static const char kNavigationalBlocks[];
  static const char kContentBlocks[];
  static const char kMarginalBlocks[];
  static const char kDeletedElements[];

  explicit MobilizeRewriteFilter(RewriteDriver* rewrite_driver);
  virtual ~MobilizeRewriteFilter();

  static void InitStats(Statistics* statistics);

  virtual void StartDocument();
  virtual void EndDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual const char* Name() const { return "MobilizeRewrite"; }

 private:
  void HandleStartTagInBody(HtmlElement* element);
  void HandleEndTagInBody(HtmlElement* element);
  void AddStyleAndViewport(HtmlElement* element);
  void AddReorderContainers(HtmlElement* element);
  void RemoveReorderContainers();
  bool IsReorderContainer(HtmlElement* element);
  HtmlElement* MobileRoleToContainer(MobileRole::Level level);
  MobileRole::Level GetMobileRole(HtmlElement* element);

  bool InImportantElement() {
    return (important_element_depth_ > 0);
  }

  bool CheckForKeyword(
      const HtmlName::Keyword* sorted_list, int len, HtmlName::Keyword keyword);
  void LogMovedBlock(MobileRole::Level level);

  RewriteDriver* driver_;
  std::vector<HtmlName::Keyword> nav_keyword_stack_;
  std::vector<HtmlElement*> mobile_role_containers_;
  int important_element_depth_;
  int body_element_depth_;
  int nav_element_depth_;
  bool reached_reorder_containers_;
  bool added_style_;
  bool added_containers_;

  // Statistics
  // Number of web pages we have mobilized.
  Variable* num_pages_mobilized_;
  // Number of blocks of each mobile role encountered and reordered.
  Variable* num_keeper_blocks_;
  Variable* num_header_blocks_;
  Variable* num_navigational_blocks_;
  Variable* num_content_blocks_;
  Variable* num_marginal_blocks_;
  // Number of elements deleted.
  Variable* num_elements_deleted_;

  // Used for overriding default behavior in testing.
  friend class MobilizeRewriteFilterTest;
  // Style content we are injecting into the page. Usually points to a static
  // asset, but MobilizeRewriteFilterTest will override this with something
  // small to simplify testing.
  const char* style_css_;

  DISALLOW_COPY_AND_ASSIGN(MobilizeRewriteFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_REWRITE_FILTER_H_
