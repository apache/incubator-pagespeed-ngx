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
// Author: mbelshe@google.com (Mike Belshe)
//         jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/url_to_filename_encoder.h"

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

// Convenience functions.
bool IsHexDigit(char c) {
  return ('0' <= c && c <= '9') ||
         ('A' <= c && c <= 'Z') ||
         ('a' <= c && c <= 'z');
}

}

// The escape character choice is made here -- all code and tests in this
// directory are based off of this constant.  However, lots of tests
// have dependencies on this, so it cannot be changed without re-running those
// tests and fixing them.
const char UrlToFilenameEncoder::kEscapeChar = ',';
const char UrlToFilenameEncoder::kTruncationChar = '-';
const size_t UrlToFilenameEncoder::kMaximumSubdirectoryLength = 128;

void UrlToFilenameEncoder::AppendSegment(GoogleString* segment,
                                         GoogleString* dest) {
  CHECK(!segment->empty());
  if ((*segment == ".") || (*segment == "..")) {
    dest->push_back(kEscapeChar);
    dest->append(*segment);
    segment->clear();
  } else {
    size_t segment_size = segment->size();
    if (segment_size > kMaximumSubdirectoryLength) {
      // We need to inject ",-" at the end of the segment to signify that
      // we are inserting an artificial '/'.  This means we have to chop
      // off at least two characters to make room.
      segment_size = kMaximumSubdirectoryLength - 2;

      // But we don't want to break up an escape sequence that happens to lie at
      // the end.  Escape sequences are at most 2 characters.
      if ((*segment)[segment_size - 1] == kEscapeChar) {
        segment_size -= 1;
      } else if ((*segment)[segment_size - 2] == kEscapeChar) {
        segment_size -= 2;
      }
      dest->append(segment->data(), segment_size);
      dest->push_back(kEscapeChar);
      dest->push_back(kTruncationChar);
      segment->erase(0, segment_size);

      // At this point, if we had segment_size=3, and segment="abcd",
      // then after this erase, we will have written "abc,-" and set segment="d"
    } else {
      dest->append(*segment);
      segment->clear();
    }
  }
}

void UrlToFilenameEncoder::EncodeSegment(const GoogleString& filename_prefix,
                                         const GoogleString& escaped_ending,
                                         char dir_separator,
                                         GoogleString* encoded_filename) {
  GoogleString filename_ending = Unescape(escaped_ending);

  char encoded[3];
  int encoded_len;
  GoogleString segment;

  // TODO(jmarantz): This code would be a bit simpler if we disallowed
  // Instaweb allowing filename_prefix to not end in "/".  We could
  // then change the is routine to just take one input string.
  size_t start_of_segment = filename_prefix.find_last_of(dir_separator);
  if (start_of_segment == GoogleString::npos) {
    segment = filename_prefix;
  } else {
    segment = filename_prefix.substr(start_of_segment + 1);
    *encoded_filename = filename_prefix.substr(0, start_of_segment + 1);
  }

  size_t index = 0;
  // Special case the first / to avoid adding a leading kEscapeChar.
  if (!filename_ending.empty() && (filename_ending[0] == dir_separator)) {
    encoded_filename->append(segment);
    segment.clear();
    encoded_filename->push_back(dir_separator);
    ++index;
  }

  for (; index < filename_ending.length(); ++index) {
    unsigned char ch = static_cast<unsigned char>(filename_ending[index]);

    // Note: instead of outputing an empty segment, we let the second slash
    // be escaped below.
    if ((ch == dir_separator) && !segment.empty()) {
      AppendSegment(&segment, encoded_filename);
      encoded_filename->push_back(dir_separator);
      segment.clear();
    } else {
      // After removing unsafe chars the only safe ones are _.=+- and alphanums.
      if ((ch == '_') || (ch == '.') || (ch == '=') || (ch == '+') ||
          (ch == '-') || (('0' <= ch) && (ch <= '9')) ||
          (('A' <= ch) && (ch <= 'Z')) || (('a' <= ch) && (ch <= 'z'))) {
        encoded[0] = ch;
        encoded_len = 1;
      } else {
        encoded[0] = kEscapeChar;
        encoded[1] = ch / 16;
        encoded[1] += (encoded[1] >= 10) ? 'A' - 10 : '0';
        encoded[2] = ch % 16;
        encoded[2] += (encoded[2] >= 10) ? 'A' - 10 : '0';
        encoded_len = 3;
      }
      segment.append(encoded, encoded_len);

      // If segment is too big, we must chop it into chunks.
      if (segment.size() > kMaximumSubdirectoryLength) {
        AppendSegment(&segment, encoded_filename);
        encoded_filename->push_back(dir_separator);
      }
    }
  }

  // Append "," to the leaf filename so the leaf can also be a branch., e.g.
  // allow http://a/b/c and http://a/b/c/d to co-exist as files "/a/b/c," and
  // /a/b/c/d".  So we will rename the "d" here to "d,".  If doing that pushed
  // us over the 128 char limit, then we will need to append "/" and the
  // remaining chars.
  segment += kEscapeChar;
  AppendSegment(&segment, encoded_filename);
  if (!segment.empty()) {
    // The last overflow segment is special, because we appended in
    // kEscapeChar above.  We won't need to check it again for size
    // or further escaping.
    encoded_filename->push_back(dir_separator);
    encoded_filename->append(segment);
  }
}

// Note: this decoder is not the exact inverse of the EncodeSegment above,
// because it does not take into account a prefix.
bool UrlToFilenameEncoder::Decode(const GoogleString& encoded_filename,
                                  char dir_separator,
                                  GoogleString* decoded_url) {
  enum State {
    kStart,
    kEscape,
    kFirstDigit,
    kTruncate,
    kEscapeDot
  };
  State state = kStart;
  int char_code = 0;
  char hex_buffer[3] = { '\0', '\0', '\0' };
  for (size_t i = 0; i < encoded_filename.size(); ++i) {
    char ch = encoded_filename[i];
    switch (state) {
      case kStart:
        if (ch == kEscapeChar) {
          state = kEscape;
        } else if (ch == dir_separator) {
          decoded_url->push_back('/');  // URLs only use '/' not '\\'
        } else {
          decoded_url->push_back(ch);
        }
        break;
      case kEscape:
        if (IsHexDigit(ch)) {
          hex_buffer[0] = ch;
          state = kFirstDigit;
        } else if (ch == kTruncationChar) {
          state = kTruncate;
        } else if (ch == '.') {
          decoded_url->push_back('.');
          state = kEscapeDot;  // Look for at most one more dot.
        } else if (ch == dir_separator) {
          // Consider url "//x".  This was once encoded to "/,/x,".
          // This code is what skips the first Escape.
          decoded_url->push_back('/');  // URLs only use '/' not '\\'
          state = kStart;
        } else {
          return false;
        }
        break;
      case kFirstDigit:
        if (IsHexDigit(ch)) {
          hex_buffer[1] = ch;
          uint32 hex_value = 0;
          bool ok = AccumulateHexValue(hex_buffer[0], &hex_value);
          ok = ok && AccumulateHexValue(hex_buffer[1], &hex_value);
          DCHECK(ok) << "Should not have gotten here unless both were hex";
          decoded_url->push_back(static_cast<char>(hex_value));
          char_code = 0;
          state = kStart;
        } else {
          return false;
        }
        break;
      case kTruncate:
        if (ch == dir_separator) {
          // Skip this separator, it was only put in to break up long
          // path segments, but is not part of the URL.
          state = kStart;
        } else {
          return false;
        }
        break;
      case kEscapeDot:
        decoded_url->push_back(ch);
        state = kStart;
        break;
    }
  }

  // All legal encoded filenames end in kEscapeChar.
  return (state == kEscape);
}

// Escape the given input |path| and chop any individual components
// of the path which are greater than kMaximumSubdirectoryLength characters
// into two chunks.
//
// This legacy version has several issues with aliasing of different URLs,
// inability to represent both /a/b/c and /a/b/c/d, and inability to decode
// the filenames back into URLs.
//
// But there is a large body of slurped data which depends on this format.
GoogleString UrlToFilenameEncoder::LegacyEscape(const GoogleString& path) {
  GoogleString output;

  // Note:  We also chop paths into medium sized 'chunks'.
  //        This is due to the incompetence of the windows
  //        filesystem, which still hasn't figured out how
  //        to deal with long filenames.
  int last_slash = 0;
  for (size_t index = 0; index < path.length(); index++) {
    char ch = path[index];
    if (ch == 0x5C)
      last_slash = index;
    if ((ch == 0x2D) ||                    // hyphen
        (ch == 0x5C) || (ch == 0x5F) ||    // backslash, underscore
        ((0x30 <= ch) && (ch <= 0x39)) ||  // Digits [0-9]
        ((0x41 <= ch) && (ch <= 0x5A)) ||  // Uppercase [A-Z]
        ((0x61 <= ch) && (ch <= 0x7A))) {  // Lowercase [a-z]
      output.push_back(path[index]);
    } else {
      char encoded[3];
      encoded[0] = 'x';
      encoded[1] = ch / 16;
      encoded[1] += (encoded[1] >= 10) ? 'A' - 10 : '0';
      encoded[2] = ch % 16;
      encoded[2] += (encoded[2] >= 10) ? 'A' - 10 : '0';
      output.append(encoded, 3);
    }
    if (index - last_slash > kMaximumSubdirectoryLength) {
#ifdef WIN32
      char slash = '\\';
#else
      char slash = '/';
#endif
      output.push_back(slash);
      last_slash = index;
    }
  }
  return output;
}

GoogleString UrlToFilenameEncoder::GetUrlHost(const GoogleString& url) {
  size_t b = url.find("//");
  if (b == GoogleString::npos)
    b = 0;
  else
    b += 2;
  size_t next_slash = url.find_first_of('/', b);
  size_t next_colon = url.find_first_of(':', b);
  if (next_slash != GoogleString::npos
      && next_colon != GoogleString::npos
      && next_colon < next_slash) {
    return GoogleString(url, b, next_colon - b);
  }
  if (next_slash == GoogleString::npos) {
    if (next_colon != GoogleString::npos) {
      return GoogleString(url, b, next_colon - b);
    } else {
      next_slash = url.size();
    }
  }
  return GoogleString(url, b, next_slash - b);
}

GoogleString UrlToFilenameEncoder::GetUrlHostPath(const GoogleString& url) {
  size_t b = url.find("//");
  if (b == GoogleString::npos)
    b = 0;
  else
    b += 2;
  return GoogleString(url, b);
}

GoogleString UrlToFilenameEncoder::GetUrlPath(const GoogleString& url) {
  size_t b = url.find("//");
  if (b == GoogleString::npos)
    b = 0;
  else
    b += 2;
  b = url.find("/", b);
  if (b == GoogleString::npos)
    return "/";

  size_t e = url.find("#", b+1);
  if (e != GoogleString::npos)
    return GoogleString(url, b, (e - b));
  return GoogleString(url, b);
}

namespace {

// Parsing states for UrlToFilenameEncoder::Unescape
enum UnescapeState {
  NORMAL,   // We are not in the middle of parsing an escape.
  ESCAPE1,  // We just parsed % .
  ESCAPE2   // We just parsed %X for some hex digit X.
};

int HexStringToInt(const GoogleString& value) {
  uint32 good_val = 0;
  for (int c = 0, n = value.size(); c < n; ++c) {
    bool ok = AccumulateHexValue(value[c], &good_val);
    if (!ok) {
      return -1;
    }
  }
  return static_cast<int>(good_val);
}

}  // namespace

GoogleString UrlToFilenameEncoder::Unescape(const GoogleString& escaped_url) {
  GoogleString unescaped_url, escape_text;
  unsigned char escape_value;
  UnescapeState state = NORMAL;
  GoogleString::const_iterator iter = escaped_url.begin();
  while (iter < escaped_url.end()) {
    char c = *iter;
    switch (state) {
      case NORMAL:
        if (c == '%') {
          escape_text.clear();
          state = ESCAPE1;
        } else {
          unescaped_url.push_back(c);
        }
        ++iter;
        break;
      case ESCAPE1:
        if (IsHexDigit(c)) {
          escape_text.push_back(c);
          state = ESCAPE2;
          ++iter;
        } else {
          // Unexpected, % followed by non-hex chars, pass it through.
          unescaped_url.push_back('%');
          state = NORMAL;
        }
        break;
      case ESCAPE2:
        if (IsHexDigit(c)) {
          escape_text.push_back(c);
          escape_value = HexStringToInt(escape_text);
          unescaped_url.push_back(escape_value);
          state = NORMAL;
          ++iter;
        } else {
          // Unexpected, % followed by non-hex chars, pass it through.
          unescaped_url.push_back('%');
          unescaped_url.append(escape_text);
          state = NORMAL;
        }
        break;
    }
  }
  // Unexpected, % followed by end of string, pass it through.
  if (state == ESCAPE1 || state == ESCAPE2) {
    unescaped_url.push_back('%');
    unescaped_url.append(escape_text);
  }
  return unescaped_url;
}

}  // namespace net_instaweb
