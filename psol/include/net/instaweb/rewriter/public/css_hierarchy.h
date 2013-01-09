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

// Author: matterbury@google.com (Matt Atterbury)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_HIERARCHY_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_HIERARCHY_H_

#include <vector>

#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace Css {
class Stylesheet;
}  // namespace Css

namespace net_instaweb {

class CssFilter;
class MessageHandler;

// Representation of a CSS with all the information required for import
// flattening, image rewriting, and minifying. A flattened CSS has had all
// of its @import's replaced with the contents of the @import'd file (and
// each of those have had their @import's replaced, and so on recursively).
//
// Lifecycle:
//   Processing:
//       Construct + InitializeRoot
//       if (ExpandChildren) <------------------+
//         for each child                       |
//           InitializeNested                   |
//           set_input_contents                 |
//           if (Parse)                         |
//             if (CheckCharsetOk)              |
//               Kick off recursion from here --+
//   Harvesting (when all the children of a node have completed):
//       if you need the rolled-up text form:
//         RollUpContents
//         Use minified_contents
//       if you need the rolled-up parsed form:
//         RollUpStylesheet
//         Use stylesheet
//
class CssHierarchy {
 public:
  // Initialized in an empty state, which is considered successful since it
  // can be flattened into nothing.
  explicit CssHierarchy(CssFilter* filter);
  ~CssHierarchy();

  // Initialize the top-level hierarchy's state from the given values.
  // A StringPiece reference to input_contents is made so it must remain
  // valid for the life of this object.
  void InitializeRoot(const GoogleUrl& css_base_url,
                      const GoogleUrl& css_trim_url,
                      const StringPiece input_contents,
                      bool has_unparseables,
                      int64 flattened_result_limit,
                      Css::Stylesheet* stylesheet,
                      MessageHandler* message_handler);

  // A hierarchy needs rewriting only if it has an import to read and expand.
  bool NeedsRewriting() const {
    return flattening_succeeded_ && !url_.empty();
  }

  const StringPiece url() const { return url_; }

  const GoogleUrl& css_base_url() const { return css_base_url_; }
  const GoogleUrl& css_trim_url() const { return css_trim_url_; }

  const Css::Stylesheet* stylesheet() const { return stylesheet_.get(); }
  Css::Stylesheet* mutable_stylesheet() { return stylesheet_.get(); }
  void set_stylesheet(Css::Stylesheet* stylesheet);

  const StringPiece input_contents() const { return input_contents_; }
  // A StringPiece reference to input_contents is made so it must remain
  // valid for the life of this object.
  void set_input_contents(const StringPiece input_contents) {
    input_contents_ = input_contents;
  }

  const GoogleString& minified_contents() const { return minified_contents_; }
  void set_minified_contents(const StringPiece minified_contents);

  const GoogleString& charset() const { return charset_; }
  GoogleString* mutable_charset() { return &charset_; }

  const StringVector& media() const { return media_; }
  StringVector* mutable_media() { return &media_; }

  // Intended for access to children; add new children using ExpandChildren.
  const std::vector<CssHierarchy*>& children() const { return children_; }
  std::vector<CssHierarchy*>& children() { return children_; }

  bool flattening_succeeded() const { return flattening_succeeded_; }
  void set_flattening_succeeded(bool ok) { flattening_succeeded_ = ok; }

  bool unparseable_detected() const { return unparseable_detected_; }
  void set_unparseable_detected(bool ok) { unparseable_detected_ = ok; }

  bool flattened_result_limit() const { return flattened_result_limit_; }
  void set_flattened_result_limit(int64 x) { flattened_result_limit_ = x; }

  // If we haven't already, determine the charset of this CSS, then check if
  // it is compatible with the charset of its parent; currently they are
  // compatible if they're exactly the same (ignoring case). The charset of
  // this CSS is taken from resource's headers if specified, else from the
  // @charset rule in the parsed CSS, if any, else from the owning document
  // (our parent). Returns true if the charsets are compatible, false if not.
  // The charset is always determined and set regardless of the return value.
  //
  // TODO(matterbury): A potential future enhancement is to allow 'compatible'
  // charsets, like a US-ASCII child in a UTF-8 parent, since US-ASCII is a
  // subset of UTF-8.
  bool CheckCharsetOk(const ResourcePtr& resource);

  // Parse the input contents into a stylesheet iff it doesn't have one yet,
  // and apply the media applicable to the whole CSS to each ruleset in the
  // stylesheet and delete any rulesets that end up with no applicable media.
  // Returns true if the input contents are successfully parsed, false if not.
  // 'this' will be unchanged if false is returned.
  bool Parse();

  // Expand the imports in our stylesheet, creating the next level of the
  // hierarchy tree by creating a child hierarchy for each import. The
  // expansion of a child can fail because of problems with the imported URL
  // or because of import recursion, in which case the flattening_succeeded
  // flag for that child is set to false. An expanded child might be empty
  // because of disjoint media rules, in which case the child is un-initialized
  // [for example, if a.css is imported with a media rule of 'print' and it
  // imports b.css with a media rule of 'screen' there is no point in expanding
  // b.css because none of it can apply to the 'print' medium]. Returns true
  // if any children were expanded and need rewriting, which can be tested
  // using NeedsRewriting() [it tests both that the child was expanded and
  // that the expansion succeeded].
  bool ExpandChildren();

  // Recursively roll up this CSS's textual form such that minified_contents()
  // returns the flattened version of this CSS with @import's replaced with the
  // contents of the imported file, all @charset rules removed, and the entire
  // result minified. Intended for use by nested hierarchies that need to
  // produce their flattened+minimized CSS for their parent to incorporate
  // into their own flattened+minimized CSS. If anything goes wrong with the
  // rolling up then the minified contents are set to the original contents.
  // If the textual form hasn't yet been parsed this method will do so by
  // invoking Parse, since the parsed form is required for minification.
  // If rolling up succeeds, any charset and imports are removed from the
  // parsed stylesheet, to match the flattened+minimized CSS for the input
  // contents (without charset/imports), and to help speed up the ultimate
  // call to RollUpStylesheets().
  void RollUpContents();

  // Recursively roll up this CSS's parsed form such that stylesheet() returns
  // the flattened version of it, with child CSSs' rulesets merged into this
  // one's and all imports and charsets removed. It is a pre-requisite that
  // any *children* have had RollUpContents() invoked on them; it is *not*
  // required that it has been invoked on 'this' but it is OK if it has. It is
  // also a pre-requisite that if the CSS has not yet been parsed then it must
  // not contain any @import rules, rather it must be the already-flattened
  // CSS text, because we use the existence of @import rules to tell that we
  // have already tried and failed to parse and flatten the CSS. This method
  // is intended to be invoked only on the root CSS since there is no need to
  // roll up intermediate/nested stylesheets; only their contents need to be
  // rolled up. Returns false if the CSS was not already parsed and the call
  // to Parse() failed, in which case rolling up has not been performed and
  // 'this' is unchanged.
  bool RollUpStylesheets();

 private:
  friend class CssHierarchyTest;

  // Initialize state from the given values; for use by nested levels that
  // are initialized from their parent's state. A StringPiece reference to
  // import_url is made so it must remain valid for the life of this object.
  void InitializeNested(const CssHierarchy& parent,
                        const GoogleUrl& import_url);

  // Resize to the specified number of children.
  void ResizeChildren(int n);

  // Determine whether this CSS is a recusrive import by checking if any CSS
  // in the hierarchy is handling our url already. This is to cater for things
  // like a.css @import'ing itself.
  bool IsRecursive() const;

  // Determine the media applicable to this CSS as the intersection of the
  // set of media applicable to the containing CSS and the set of media
  // applicable to this CSS as a whole, and save that intersection in this
  // CSS's media attribute. If the resulting media is empty then this CSS
  // doesn't have to be processed at all so return false, otherwise true.
  bool DetermineImportMedia(const StringVector& containing_media,
                            const StringVector& import_media);

  // Determine the media applicable to a ruleset as the intersection of the
  // set of media that apply just to the ruleset and the set of media that
  // apply to this CSS (as determined by DetermineImportMedia above), and
  // edits ruleset_media in place. If the intersection is empty, false is
  // returned and the ruleset doesn't have to be processed at all (it can
  // be omitted), else true is returned.
  bool DetermineRulesetMedia(StringVector* ruleset_media);

  // The filter that owns us, used for recording statistics.
  CssFilter* filter_;

  // The URL of the stylesheet being represented; in the case of inline CSS
  // this will be a data URL.
  StringPiece url_;

  // The base for any relative URLs in the input CSS.
  GoogleUrl css_base_url_;

  // The base of the output URL which is used to trim absolutified URLs back
  // to relative URLs in the output CSS.
  GoogleUrl css_trim_url_;

  // A pointer to the representation of the parent CSS that imports this CSS;
  // for the top-level CSS only this will be NULL.
  const CssHierarchy* parent_;

  // An array of pointers to the child representations of the CSS's that this
  // CSS imports, one array element per import, in the order they are imported;
  // for leaf CSS's this will be empty.
  std::vector<CssHierarchy*> children_;

  // The text form of the input CSS.
  StringPiece input_contents_;

  // The text form of the output (flattened) CSS.
  GoogleString minified_contents_;

  // The parsed form of the CSS, in various states of transformation. Created
  // from the input text form by Parse, mutated by RollUpContents and
  // RollUpStylesheets - see their description for details.
  scoped_ptr<Css::Stylesheet> stylesheet_;

  // The charset for this CSS as specified by HTTP headers, or a charset
  // attribute, or an @charset rule, or inherited from the parent.
  GoogleString charset_;

  // The collection of media for which this CSS applies; an empty collection
  // means all media. CSS in or linked from HTML can specify this using a media
  // attribute, @import'd CSS can specify it on the @import rule. Note that
  // this is NOT media from @media rules, it is only media that applies to the
  // *whole* CSS document. Note that media expressions (CSS3) are NOT handled.
  StringVector media_;

  // An indication of the success or failure of the flattening process, which
  // can fail for various reasons, and any failure propagates up the hierarchy
  // to the root CSS and eventually stops the process.
  bool flattening_succeeded_;

  // An indication of whether anything unparseable was detected in this CSS.
  bool unparseable_detected_;

  // The limit to the size of the result of flattening (0 means no limit).
  // If the flattened result would be this much or more, flattening will be
  // aborted. TODO(matterbury): Investigate whether we can, or ought to,
  // flatten nested @imports that do fit within the limit [eg. a.css imports
  // b.css then has a load of CSS; b.css imports c.ss then some CSS; say the
  // flattened version of b.css fits in the limit, but the flattened version
  // of a.css does not; we could flatten b.css then change the @import in
  // a.css to import the flattened version, saving the fetch of c.css].
  int64 flattened_result_limit_;

  // For logging messages.
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(CssHierarchy);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_HIERARCHY_H_
