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

#include "net/instaweb/util/public/url_escaper.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

// Firefox converts ^ to a % sequence.
// Apache rejects requests with % sequences it does not understand.
// So limit the pass-through characters as follows, and use ',' as
// an escaper.
//
// Unfortunately this makes longer filenames because ',' is also used
// in the filenam encoder.
//
// TODO(jmarantz): Pass through '.', and exploit '/' as a legal character
// in URLs.  This requires redefining the constraints of a 'segment', which
// currently excludes both '.' and '/' due to rules enforced primarily
// in net/instaweb/rewriter/resource_manager.cc, but are distributed a bit
// more widely.
const char kPassThroughChars[] = "_=+-&?";

// Checks for 'search' at start of 'src'.  If found, appends
// 'replacement' into 'out', and advances the start-point in 'src'
// past the search string, returning true.
bool ReplaceSubstring(const StringPiece& search, const char* replacement,
                      StringPiece* src, std::string* out) {
  bool ret = false;
  if ((src->size() >= search.size()) &&
      (memcmp(src->data(), search.data(), search.size()) == 0)) {
    out->append(replacement);
    *src = src->substr(search.size());
    ret = true;
  }
  return ret;
}

}  // namespace

void UrlEscaper::EncodeToUrlSegment(const StringPiece& in,
                                    std::string* url_segment) {
  for (StringPiece src = in; src.size() != 0; ) {
    // We need to check for common prefixes that begin with pass-through
    // characters before doing the isalnum check.
    if (!ReplaceSubstring("http://", ",h", &src, url_segment) &&
        !ReplaceSubstring("www.", ",w", &src, url_segment)) {
      char c = src[0];
      if (isalnum(c) || (strchr(kPassThroughChars, c) != NULL)) {
        url_segment->append(1, c);
        src = src.substr(1);
      } else if (
          // TODO(jmarantz): put these in a static table and generate
          // an FSM so we don't have so much lookahed scanning, and we
          // don't have to work hard to keep the encoder and decoder
          // in sync.
          !ReplaceSubstring(".com", ",c", &src, url_segment) &&
          !ReplaceSubstring(".css", ",s", &src, url_segment) &&
          !ReplaceSubstring(".edu", ",e", &src, url_segment) &&
          !ReplaceSubstring(".gif", ",g", &src, url_segment) &&
          !ReplaceSubstring(".html", ",t", &src, url_segment) &&
          !ReplaceSubstring(".jpeg", ",k", &src, url_segment) &&
          !ReplaceSubstring(".jpg", ",j", &src, url_segment) &&
          !ReplaceSubstring(".js", ",l", &src, url_segment) &&
          !ReplaceSubstring(".net", ",n", &src, url_segment) &&
          !ReplaceSubstring(".png", ",p", &src, url_segment) &&
          !ReplaceSubstring(".", ",o", &src, url_segment) &&
          !ReplaceSubstring("^", ",u", &src, url_segment) &&
          !ReplaceSubstring("%", ",P", &src, url_segment) &&
          !ReplaceSubstring("/", ",_", &src, url_segment) &&
          !ReplaceSubstring("\\", ",-", &src, url_segment) &&
          !ReplaceSubstring(",", ",,", &src, url_segment)) {
        url_segment->append(StringPrintf(",%02X",
                                         static_cast<unsigned char>(c)));
        src = src.substr(1);
      }
    }
  }
}

bool UrlEscaper::DecodeFromUrlSegment(const StringPiece& url_segment,
                                      std::string* out) {
  int remaining = url_segment.size();
  for (const char* p = url_segment.data(); remaining != 0; ++p, --remaining) {
    char c = *p;
    if (isalnum(c) || (strchr(kPassThroughChars, c) != NULL)) {
      out->append(&c, 1);
    } else if ((c != ',') || (remaining < 2)) {
      return false;  // unknown char or trailing ,; this is an invalid encoding.
    } else {
      ++p;
      --remaining;
      switch (*p) {
        case '_': *out += "/"; break;
        case '-': *out += "\\"; break;
        case ',': *out += ","; break;
        case 'c': *out += ".com"; break;
        case 's': *out += ".css"; break;
        case 'e': *out += ".edu"; break;
        case 'g': *out += ".gif"; break;
        case 'h': *out += "http://"; break;
        case 'k': *out += ".jpeg"; break;
        case 'j': *out += ".jpg"; break;
        case 'l': *out += ".js"; break;
        case 'n': *out += ".net"; break;
        case 'o': *out += "."; break;
        case 'p': *out += ".png"; break;
        case 'P': *out += "%"; break;
        case 't': *out += ".html"; break;
        case 'u': *out += "^"; break;
        case 'w': *out += "www."; break;
        default:
          if (remaining < 2) {
            return false;
          }
          --remaining;
          int char_val = 0;
          if (AccumulateHexValue(*p++, &char_val) &&
              AccumulateHexValue(*p, &char_val)) {
            out->append(1, static_cast<char>(char_val));
          } else {
            return false;
          }
          break;
      }
    }
  }
  return true;
}

}  // namespace net_instaweb
