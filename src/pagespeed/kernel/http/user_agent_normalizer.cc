// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: morlovich@google.com (Maksim Orlovich)

#include "pagespeed/kernel/http/user_agent_normalizer.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/util/re2.h"

namespace net_instaweb {

UserAgentNormalizer::~UserAgentNormalizer() {}

GoogleString UserAgentNormalizer::NormalizeWithAll(
    const std::vector<const UserAgentNormalizer*>& ua_normalizers,
    const GoogleString& ua_in) {
  GoogleString ua = ua_in;
  for (int i = 0, n = ua_normalizers.size(); i < n; ++i) {
    ua = ua_normalizers[i]->Normalize(ua);
  }
  return ua;
}

// Samples:
// Dalvik/1.4.0 (Linux; U; Android 2.3.7; M5 Build/GRK39F)
// Mozilla/5.0 (Linux; Android 4.1.1; Nexus 7 Build/JRO03L) AppleWebKit/537.31 (KHTML, like Gecko) Chrome/26.0.1410.58 Safari/537.31
// Mozilla/5.0 (Linux; Android 4.2.2; Nexus 4 Build/JDQ39) AppleWebKit/537.31 (KHTML, like Gecko) Chrome/26.0.1410.58 Mobile Safari/537.31
// Some of Samsung's phones also seem to throw in Version/1.0 before Chrome/
// Mozilla/5.0 (Linux; U; Android 4.1.2; ar-ae; GT-I9300 Build/JZO54K) AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30"

AndroidUserAgentNormalizer::AndroidUserAgentNormalizer()
    : dalvik_ua_("(Dalvik/[\\d\\.]+ \\(Linux; U; Android "
                 "[^\\s;]+)[\\s;][^)]+\\)"),
      chrome_android_ua_(
          "(Mozilla/5.0 \\(Linux; Android [\\d\\.]+; )[^)]+(\\) "
          "AppleWebKit/[\\d\\.]+ \\(KHTML, like Gecko\\) )"
          "(?:Version/[\\d\\.]+ )?"
          "(Chrome/[\\d\\.]+(?: Mobile)?[ ]+Safari/[\\d\\.]+)"),
      android_browser_ua_(
          "(Mozilla/5.0 \\(Linux;(?: U;)? Android [\\d\\.]+; )[^)]+(\\) "
          "AppleWebKit/[\\d\\.\\+]+ \\(KHTML, like Gecko\\) "
          "Version/[\\d\\.]+(?: Mobile)? Safari/[\\d\\.]+)") {
  CHECK(dalvik_ua_.ok()) << dalvik_ua_.error();
  CHECK(chrome_android_ua_.ok()) << chrome_android_ua_.error();
  CHECK(android_browser_ua_.ok()) << android_browser_ua_.error();
}

AndroidUserAgentNormalizer::~AndroidUserAgentNormalizer() {
}

GoogleString AndroidUserAgentNormalizer::Normalize(
    const GoogleString& in) const {
  Re2StringPiece match, match2, match3;
  if (RE2::FullMatch(in, dalvik_ua_, &match)) {
    return StrCat(Re2ToStringPiece(match), ")");
  }
  if (RE2::FullMatch(in, chrome_android_ua_, &match, &match2, &match3)) {
    return StrCat(Re2ToStringPiece(match), Re2ToStringPiece(match2),
                  Re2ToStringPiece(match3));
  }
  if (RE2::FullMatch(in, android_browser_ua_, &match, &match2)) {
    return StrCat(Re2ToStringPiece(match), Re2ToStringPiece(match2));
  }
  return in;
}

// Samples:
// "Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; Trident/4.0; SV1; SE 2.X MetaSr 1.0)", 69838, 0.75695
// "Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.2; WOW64; Trident/6.0; Touch)", 48308, 0.777381

IEUserAgentNormalizer::IEUserAgentNormalizer()
    : ie_ua_("(Mozilla/\\d.0 \\(compatible; MSIE [\\d\\.]+)"
             "([^)]+)\\)") {
  CHECK(ie_ua_.ok()) << ie_ua_.error();
}

IEUserAgentNormalizer::~IEUserAgentNormalizer() {
}

GoogleString IEUserAgentNormalizer::Normalize(const GoogleString& in) const {
  Re2StringPiece match, match2;
  if (RE2::FullMatch(in, ie_ua_, &match, &match2)) {
    // IE UA strings enumerate things like installed .NET versions which
    // blow up their variety. We keep only parts that talk about the
    // renderer or platform
    GoogleString out;
    match.CopyToString(&out);
    StringPieceVector fragments;
    SplitStringUsingSubstr(Re2ToStringPiece(match2), "; ", &fragments);
    for (int i = 0, n = fragments.size(); i < n; ++i) {
      StringPiece fragment = fragments[i];
      if (HasPrefixString(fragment, "Trident") ||
          HasPrefixString(fragment, "Windows ") ||
          HasPrefixString(fragment, "WOW64 ") ||
          HasPrefixString(fragment, "chromeframe") ||
          HasPrefixString(fragment, "IEMobile") ||
          HasPrefixString(fragment, "Media Center PC")) {
        StrAppend(&out, "; ", fragment);
      }
    }
    StrAppend(&out, ")");
    return out;
  }
  return in;
}

}  // namespace net_instaweb
