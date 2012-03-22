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

#include "net/instaweb/util/public/url_escaper.h"

#include <cstddef>
#include <cctype>
#include "net/instaweb/util/public/string.h"
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
const char kPassThroughChars[] = "._=+-";

// Checks for 'search' at start of 'src'.  If found, appends
// 'replacement' into 'out', and advances the start-point in 'src'
// past the search string, returning true.
bool ReplaceSubstring(const StringPiece& search, const char* replacement,
                      StringPiece* src, GoogleString* out) {
  bool ret = false;
  if (src->starts_with(search)) {
    out->append(replacement);
    src->remove_prefix(search.size());
    ret = true;
  }
  return ret;
}

}  // namespace

void UrlEscaper::EncodeToUrlSegment(const StringPiece& in,
                                    GoogleString* url_segment) {
  for (StringPiece src = in; src.size() != 0; ) {
    char c = src[0];
    src.remove_prefix(1);
    // TODO(jmarantz): put these in a static table, to make it
    // faster and so we don't have to work hard to keep the encoder
    // and decoder in sync.
    switch (c) {
      case '^':
        url_segment->append(",u");
        break;
      case '%':
        url_segment->append(",P");
        break;
      case '/':
        url_segment->append(",_");
        break;
      case '\\':
        url_segment->append(",-");
        break;
      case ',':
        url_segment->append(",,");
        break;
      case '?':
        url_segment->append(",q");
        break;
      case '&':
        url_segment->append(",a");
        break;
      case 'h':
        if (!ReplaceSubstring("ttp://", ",h", &src, url_segment)) {
          // Just pass-through 'h'
          url_segment->push_back('h');
        }
        break;
      case '.':
        // . is a passthrough char, but .pagespeed. is special
        if (!ReplaceSubstring("pagespeed.", ",M", &src, url_segment)) {
          url_segment->push_back('.');
        }
        break;
      default:
        if (isalnum(c) || (strchr(kPassThroughChars, c) != NULL)) {
          url_segment->push_back(c);
        } else {
          StringAppendF(url_segment, ",%02X", static_cast<unsigned char>(c));
        }
    }
  }
}


namespace {

// DecodeHexEncoding assumes that buffer[pos, pos+1] is of the form "xx" are
// hexadecimal digits.  It constructs a char from these characters, or returns
// false to indicate encoding failure.
bool DecodeHexEncoding(const StringPiece& buffer, size_t i, char* result) {
  int char_val = 0;
  if ((i + 1 < buffer.size()) &&
      AccumulateHexValue(buffer[i], &char_val) &&
      AccumulateHexValue(buffer[i+1], &char_val)) {
    *result = static_cast<char>(char_val);
    return true;
  }
  return false;
}

}  // namespace


bool UrlEscaper::DecodeFromUrlSegment(const StringPiece& url_segment,
                                      GoogleString* out) {
  size_t size = url_segment.size();
  for (size_t i = 0; i < size; ++i) {
    char c = url_segment[i];
    if (isalnum(c) || (strchr(kPassThroughChars, c) != NULL)) {
      out->push_back(c);
      continue;
    }
    // We ought to have a ',' or a '%' to decode (or a bad encoding)
    ++i;  // i points to first char of encoding
    if (i >= size) {
      // No space for encoded data
      return false;
    }
    if (c != ',') {
      if ((c == '%') && DecodeHexEncoding(url_segment, i, &c)) {
        ++i;  // i points to last char of encoding
        // Rare corner case: there exist browsers that percent-encode + to %20
        // (space), which is supposed to be illegal except after ? (in query
        // params).
        if (c == ' ') {
          c = '+';
        }
        if (c != ',') {
          out->push_back(c);
          continue;
        }
        // We found a %-encoded ,
        ++i;  // Make i point to first char of , encoding
        if (i >= size) {
          // trailing %-encoded ,
          return false;
        }
        // Fall through and decode the ,
      } else {
        return false;  // unknown char; this is an invalid encoding.
      }
    }
    // At this point we know we're decoding a , encoding.
    // TODO(jmaessen): Worry about %-encoding here, if that ever comes up.
    // To our knowledge it never has.
    switch (url_segment[i]) {
      case '_': *out += "/"; break;
      case '-': *out += "\\"; break;
      case ',': *out += ","; break;
      case 'a': *out += "&"; break;
      case 'M': *out += ".pagespeed."; break;
      case 'P': *out += "%"; break;
      case 'q': *out += "?"; break;
      case 'u': *out += "^"; break;

      // The following legacy encodings are no longer made.  However we should
      // continue to decode what we previously encoded in November 2010 to
      // avoid (for example) breaking image search.
      case 'c': *out += ".com"; break;
      case 'e': *out += ".edu"; break;
      case 'g': *out += ".gif"; break;
      case 'h': *out += "http://"; break;
      case 'j': *out += ".jpg"; break;
      case 'k': *out += ".jpeg"; break;
      case 'l': *out += ".js"; break;
      case 'n': *out += ".net"; break;
      case 'o': *out += "."; break;
      case 'p': *out += ".png"; break;
      case 's': *out += ".css"; break;
      case 't': *out += ".html"; break;
      case 'w': *out += "www."; break;

      default:
        if (DecodeHexEncoding(url_segment, i, &c)) {
          ++i;
          out->push_back(c);
        } else {
            return false;
        }
        break;
    }
    // At this point i points to last char of just-decoded encoding.
  }
  return true;
}

}  // namespace net_instaweb
