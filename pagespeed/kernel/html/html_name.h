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

#ifndef PAGESPEED_KERNEL_HTML_HTML_NAME_H_
#define PAGESPEED_KERNEL_HTML_HTML_NAME_H_

#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

// HTML names are case insensitive.  However, in the parser, we keep
// the original parsed case of the name, in addition to the html
// keyword enumeration, if any.  Thus for both tags and attribute
// names, we have an enum representation which is used in filters
// for scanning, plus we have the original string representation.
class HtmlName {
 public:
  // We keep both attribute names and tag names in the same space
  // for convenience.  This list must be kept in alpha-order and
  // in sync with the static array in html_name.cc.
  //
  // Note that this list does not need to cover all HTML keywords --
  // only the ones that we are interested in for rewriting.
  enum Keyword {
    kXml,  // ?Xml
    kA,
    kAbbr,
    kAction,
    kAddress,
    kAlt,
    kAmp,
    kArea,
    kArticle,
    kAs,
    kAside,
    kAsync,
    kAudio,
    kAutocomplete,
    kAutofocus,
    kAutoplay,
    kB,
    kBackground,
    kBase,
    kBdi,
    kBdo,
    kBlockquote,
    kBody,
    kBr,
    kButton,
    kCaption,
    kCharset,
    kChecked,
    kCite,
    kClass,
    kCode,
    kCol,
    kColgroup,
    kColspan,
    kCommand,
    kContent,
    kControls,
    kData,
    kDatalist,
    kDataActualHeight,
    kDataActualWidth,
    kDataMobileRole,
    kDataPagespeedFlushStyle,
    kDataPagespeedHighResSrc,
    kDataPagespeedHighResSrcset,
    kDataPagespeedHref,
    kDataPagespeedInlineSrc,
    kDataPagespeedLazySrc,
    kDataPagespeedLazySrcset,
    kDataPagespeedLowResSrc,
    kDataPagespeedLscExpiry,
    kDataPagespeedLscHash,
    kDataPagespeedLscUrl,
    kDataPagespeedNoDefer,
    kDataPagespeedNoTransform,
    kDataPagespeedOrigIndex,
    kDataPagespeedOrigSrc,
    kDataPagespeedOrigType,
    kDataPagespeedPrioritize,
    kDataPagespeedResponsiveTemp,
    kDataPagespeedSize,
    kDataPagespeedUrlHash,
    kDataSrc,
    kDd,
    kDeclare,
    kDefaultchecked,
    kDefaultselected,
    kDefer,
    kDel,
    kDetails,
    kDfn,
    kDir,
    kDisabled,
    kDisplay,
    kDiv,
    kDl,
    kDt,
    kEm,
    kEmbed,
    kEnctype,
    kEvent,
    kFieldset,
    kFont,
    kFooter,
    kFor,
    kForm,
    kFormaction,
    kFormnovalidate,
    kFrame,
    kFrameborder,
    kH1,
    kH2,
    kH3,
    kH4,
    kH5,
    kH6,
    kHead,
    kHeader,
    kHeight,
    kHgroup,
    kHr,
    kHref,
    kHtml,
    kHttpEquiv,
    kI,
    kIcon,
    kId,
    kIframe,
    kImg,
    kIndeterminate,
    kIns,
    kInput,
    kIsmap,
    kItemProp,
    kKbd,
    kKeygen,
    kKeytype,
    kLang,
    kLanguage,
    kLegend,
    kLi,
    kLink,
    kLongdesc,
    kLoop,
    kMain,
    kManifest,
    kMap,
    kMark,
    kMarquee,
    kMedia,
    kMenu,
    kMeta,
    kMethod,
    kMultiple,
    kMuted,
    kName,
    kNav,
    kNoembed,
    kNoframes,
    kNohref,
    kNoresize,
    kNoscript,
    kNovalidate,
    kObject,
    kOl,
    kOnclick,
    kOnerror,
    kOnload,
    kOpen,
    kOptgroup,
    kOption,
    kOther,
    kP,
    kPagespeedIframe,
    kPagespeedNoDefer,
    kPagespeedNoTransform,
    kParam,
    kPoster,
    kPre,
    kProfile,
    kQ,
    kReadonly,
    kRel,
    kRequired,
    kReversed,
    kRole,
    kRowspan,
    kRp,
    kRt,
    kRuby,
    kS,
    kSamp,
    kScoped,
    kScript,
    kScrolling,
    kSeamless,
    kSection,
    kSelect,
    kSelected,
    kShape,
    kSmall,
    kSource,
    kSpan,
    kSrc,
    kSrcset,
    kStrong,
    kStyle,
    kSub,
    kTable,
    kTag,
    kTbody,
    kTd,
    kTest,
    kTextarea,
    kTfoot,
    kTh,
    kThead,
    kTime,
    kTitle,
    kTr,
    kTrack,
    kType,
    kU,
    kUl,
    kValue,
    kValuetype,
    kVar,
    kVideo,
    kWbr,
    kWidth,
    kWrap,
    kXmp,
    kNotAKeyword
  };


  // HtmlName's should be normally constructed using HtmlParse::MakeName

  // Returns the keyword enumeration for this HTML Name.  Note that
  // keyword lookup is case-insensitive.
  Keyword keyword() const { return keyword_; }

  // Return the atom string, which may not be case folded.
  StringPiece value() const { return *str_; }

  // Limited iterator (not an STL iterator).  Example usage:
  //    for (HtmlName::Iterator iter; !iter.AtEnd(); iter.Next()) {
  //      use(iter.keyword(), iter.name());
  //    }
  class Iterator {
   public:
    Iterator() : index_(-1) { Next(); }
    bool AtEnd() const;
    void Next();
    Keyword keyword() const;
    const char* name() const;

   private:
    int index_;

    // Implicit copy and assign ok.  The members can be safely copied by bits.
  };

  static int num_keywords();
  static Keyword Lookup(const StringPiece& name);

 private:
  // Constructs an HTML name given a keyword, which can be
  // HtmlName::kNotAKeyword, and 'StringPiece* str'.  'str'
  // is used to retain the case-sensitive spelling of the
  // keyword.  The storage for 'str' must be managed, and
  // must be guaranteed valid throughout the life of the HtmlName.
  HtmlName(Keyword keyword, const StringPiece* str)
      : keyword_(keyword), str_(str) {
  }

  friend class HtmlNameTest;
  friend class HtmlParse;

  Keyword keyword_;
  const StringPiece* str_;

  // Implicit copy and assign ok.  The members can be safely copied by bits.
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTML_HTML_NAME_H_
