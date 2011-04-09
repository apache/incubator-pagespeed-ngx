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

// Copyright 2006 Google Inc. All Rights Reserved.
// Author: mec@google.com  (Michael Chastain)
// Author: dpeng@google.com (Daniel Peng)

#ifndef WEBUTIL_HTML_HTMLTAGENUM_H__
#define WEBUTIL_HTML_HTMLTAGENUM_H__

#include <string>
#include "string_using.h"

// This is public at the top level because I think a lot of people
// will want to use it.
//
// NOTE: These values may be stored in proto buffers.  Do not change or remove
// any existing values.  If you want to add support for a new tag, use the
// next available value (as specified by kHtmlTagBuiltinMax), and increment
// kHtmlTagBuiltinMax.  Also make sure to add the new tag to HtmlTagEnumNames
// in htmltagenum.cc.
//
// This tag list came from:
//   http://www.w3.org/TR/REC-html40/index/elements.html
// With additional tags used in:
//   repository/lexer/html_lexer.cc
//   repository/parsers/base/handler-parser.cc
// And some additional legacy backwards-compatible tags from HTML5:
//   http://whatwg.org/specs/web-apps/current-work/#stack
//   [accessed 2006-10-10]
// Plus some Netscape Navigator 4.0 tags from:
//   http://devedge-temp.mozilla.org/library/manuals/1998/htmlguide/
//   [accessed 2006-11-21]
// The !doctype tag defines a reference to DTD (doctype type definition).
// It is a SGML tag and is somehow mentioned in the HTML 4.01 spec.
//   http://www.w3.org/TR/html401/intro/sgmltut.html
// The noindex tag is a non-standard tag used mostly by Russian sites:
//   http://ru.wikipedia.org/wiki/Noindex
//   http://translate.google.com/translate?u=http://ru.wikipedia.org/wiki/Noindex&sl=ru&tl=en
// Added HTML5 tags, per 2011-01-13 working draft:
//   http://www.w3.org/TR/html5/

enum HtmlTagEnum {
  // Unknown tag: must be 0
     kHtmlTagUnknown = 0,
  // From html 4.01 spec
     kHtmlTagA = 1,
     kHtmlTagAbbr = 2,
     kHtmlTagAcronym = 3,
     kHtmlTagAddress = 4,
     kHtmlTagApplet = 5,
     kHtmlTagArea = 6,
     kHtmlTagB = 7,
     kHtmlTagBase = 8,
     kHtmlTagBasefont = 9,
     kHtmlTagBdo = 10,
     kHtmlTagBig = 11,
     kHtmlTagBlockquote = 12,
     kHtmlTagBody = 13,
     kHtmlTagBr = 14,
     kHtmlTagButton = 15,
     kHtmlTagCaption = 16,
     kHtmlTagCenter = 17,
     kHtmlTagCite = 18,
     kHtmlTagCode = 19,
     kHtmlTagCol = 20,
     kHtmlTagColgroup = 21,
     kHtmlTagDd = 22,
     kHtmlTagDel = 23,
     kHtmlTagDfn = 24,
     kHtmlTagDir = 25,
     kHtmlTagDiv = 26,
     kHtmlTagDl = 27,
     kHtmlTagDt = 28,
     kHtmlTagEm = 29,
     kHtmlTagFieldset = 30,
     kHtmlTagFont = 31,
     kHtmlTagForm = 32,
     kHtmlTagFrame = 33,
     kHtmlTagFrameset = 34,
     kHtmlTagH1 = 35,
     kHtmlTagH2 = 36,
     kHtmlTagH3 = 37,
     kHtmlTagH4 = 38,
     kHtmlTagH5 = 39,
     kHtmlTagH6 = 40,
     kHtmlTagHead = 41,
     kHtmlTagHr = 42,
     kHtmlTagHtml = 43,
     kHtmlTagI = 44,
     kHtmlTagIframe = 45,
     kHtmlTagImg = 46,
     kHtmlTagInput = 47,
     kHtmlTagIns = 48,
     kHtmlTagIsindex = 49,
     kHtmlTagKbd = 50,
     kHtmlTagLabel = 51,
     kHtmlTagLegend = 52,
     kHtmlTagLi = 53,
     kHtmlTagLink = 54,
     kHtmlTagMap = 55,
     kHtmlTagMenu = 56,
     kHtmlTagMeta = 57,
     kHtmlTagNoframes = 58,
     kHtmlTagNoscript = 59,
     kHtmlTagObject = 60,
     kHtmlTagOl = 61,
     kHtmlTagOptgroup = 62,
     kHtmlTagOption = 63,
     kHtmlTagP = 64,
     kHtmlTagParam = 65,
     kHtmlTagPre = 66,
     kHtmlTagQ = 67,
     kHtmlTagS = 68,
     kHtmlTagSamp = 69,
     kHtmlTagScript = 70,
     kHtmlTagSelect = 71,
     kHtmlTagSmall = 72,
     kHtmlTagSpan = 73,
     kHtmlTagStrike = 74,
     kHtmlTagStrong = 75,
     kHtmlTagStyle = 76,
     kHtmlTagSub = 77,
     kHtmlTagSup = 78,
     kHtmlTagTable = 79,
     kHtmlTagTbody = 80,
     kHtmlTagTd = 81,
     kHtmlTagTextarea = 82,
     kHtmlTagTfoot = 83,
     kHtmlTagTh = 84,
     kHtmlTagThead = 85,
     kHtmlTagTitle = 86,
     kHtmlTagTr = 87,
     kHtmlTagTt = 88,
     kHtmlTagU = 89,
     kHtmlTagUl = 90,
     kHtmlTagVar = 91,
  // Empty tag
     kHtmlTagZeroLength = 92,
  // Used in repository/lexer/html_lexer.cc
     kHtmlTagBangDashDash = 93,
     kHtmlTagBlink = 94,
  // Used in repository/parsers/base/handler-parser.cc
     kHtmlTagEmbed = 95,
     kHtmlTagMarquee = 96,
  // Legacy backwards-compatible tags mentioned in HTML5.
     kHtmlTagNobr = 97,
     kHtmlTagWbr = 98,
     kHtmlTagBgsound = 99,
     kHtmlTagImage = 100,
     kHtmlTagListing = 101,
     kHtmlTagNoembed = 102,
     kHtmlTagPlaintext = 103,
     kHtmlTagSpacer = 104,
     kHtmlTagXmp = 105,
  // From Netscape Navigator 4.0
     kHtmlTagIlayer = 106,
     kHtmlTagKeygen = 107,
     kHtmlTagLayer = 108,
     kHtmlTagMulticol = 109,
     kHtmlTagNolayer = 110,
     kHtmlTagServer = 111,
  // !doctype from SGML and also from HTML 4.01 spec.
     kHtmlTagBangDoctype = 112,
  // Legacy tag used mostly by Russian sites.
     kHtmlTagNoindex = 113,
  // Anything starts with ! (except those marked above) or ?
     kHtmlTagBogusComment = 114,
  // New tags in HTML5.
     kHtmlTagArticle = 115,
     kHtmlTagAside = 116,
     kHtmlTagAudio = 117,
     kHtmlTagBdi = 118,
     kHtmlTagCanvas = 119,
     kHtmlTagCommand = 120,
     kHtmlTagDatalist = 121,
     kHtmlTagDetails = 122,
     kHtmlTagFigcaption = 123,
     kHtmlTagFigure = 124,
     kHtmlTagFooter = 125,
     kHtmlTagHeader = 126,
     kHtmlTagHgroup = 127,
     kHtmlTagMark = 128,
     kHtmlTagMeter = 129,
     kHtmlTagNav = 130,
     kHtmlTagOutput = 131,
     kHtmlTagProgress = 132,
     kHtmlTagRp = 133,
     kHtmlTagRt = 134,
     kHtmlTagRuby = 135,
     kHtmlTagSection = 136,
     kHtmlTagSource = 137,
     kHtmlTagSummary = 138,
     kHtmlTagTime = 139,
     kHtmlTagTrack = 140,
     kHtmlTagVideo = 141,

  // Add new tag values here.  Make sure you also add new tags to
  // HtmlTagEnumNames in htmltagenum.cc and update kHtmlTagBuiltinMax.

  // Sentinel.
     kHtmlTagBuiltinMax = 142
};

// NULL if tag >= kHtmlTagBuiltinMax.
extern const char* HtmlTagName(HtmlTagEnum tag);

// StringPrintf("UNKNOWN%d", tag) if tag >= kHtmlTag
extern string HtmlTagNameOrUnknown(int i);

#endif  // WEBUTIL_HTML_HTMLTAGENUM_H__
