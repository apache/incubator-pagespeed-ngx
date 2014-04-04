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
//         nforman@google.com  (Naomi Forman)

#include "pagespeed/kernel/http/google_url.h"

#include <cstddef>
#include <string>

#include "base/logging.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/query_params.h"

namespace net_instaweb {

const size_t GoogleUrl::npos = std::string::npos;

GoogleUrl::GoogleUrl()
    : gurl_() {
  Init();
}

GoogleUrl::GoogleUrl(const GURL& gurl)
    : gurl_(gurl) {
  Init();
}

GoogleUrl::GoogleUrl(const GoogleString& spec)
    : gurl_(spec) {
  Init();
}

GoogleUrl::GoogleUrl(const StringPiece& sp)
    : gurl_(sp.as_string()) {
  Init();
}

GoogleUrl::GoogleUrl(const char* str)
    : gurl_(str) {
  Init();
}

// The following three constructors create a new GoogleUrl by resolving the
// String(Piece) against the base.
GoogleUrl::GoogleUrl(const GoogleUrl& base, const GoogleString& str) {
  Reset(base, str);
}

GoogleUrl::GoogleUrl(const GoogleUrl& base, const StringPiece& sp) {
  Reset(base, sp);
}

GoogleUrl::GoogleUrl(const GoogleUrl& base, const char* str) {
  Reset(base, str);
}

void GoogleUrl::Swap(GoogleUrl* google_url) {
  gurl_.Swap(&google_url->gurl_);
  bool old_is_web_valid = is_web_valid_;
  bool old_is_web_or_data_valid = is_web_or_data_valid_;
  is_web_valid_ = google_url->is_web_valid_;
  is_web_or_data_valid_ = google_url->is_web_or_data_valid_;
  google_url->is_web_valid_ = old_is_web_valid;
  google_url->is_web_or_data_valid_ = old_is_web_or_data_valid;
}

void GoogleUrl::Init() {
  is_web_valid_ = gurl_.is_valid() && (SchemeIs("http") || SchemeIs("https"));
  is_web_or_data_valid_ =
      is_web_valid_ || (gurl_.is_valid() && SchemeIs("data"));
}

bool GoogleUrl::ResolveHelper(const GURL& base, const std::string& url) {
  gurl_ = base.Resolve(url);
  Init();
  return gurl_.is_valid();
}

bool GoogleUrl::Reset(const GoogleUrl& base, const GoogleString& str) {
  return ResolveHelper(base.gurl_, str);
}

bool GoogleUrl::Reset(const GoogleUrl& base, const StringPiece& sp) {
  return ResolveHelper(base.gurl_, sp.as_string());
}

bool GoogleUrl::Reset(const GoogleUrl& base, const char* str) {
  return ResolveHelper(base.gurl_, str);
}

bool GoogleUrl::Reset(const StringPiece& new_value) {
  gurl_ = GURL(new_value.as_string());
  Init();
  return gurl_.is_valid();
}

bool GoogleUrl::Reset(const GoogleUrl& new_value) {
  gurl_ = GURL(new_value.gurl_);
  Init();
  return gurl_.is_valid();
}

void GoogleUrl::Clear() {
  gurl_ = GURL();
  Init();
}

bool GoogleUrl::IsWebValid() const {
  DCHECK(is_web_valid_ ==
         (gurl_.is_valid() && (SchemeIs("http") || SchemeIs("https"))));
  return is_web_valid_;
}

bool GoogleUrl::IsWebOrDataValid() const {
  DCHECK(is_web_or_data_valid_ ==
         (gurl_.is_valid() && (SchemeIs("http") || SchemeIs("https") ||
                               SchemeIs("data"))));
  return is_web_or_data_valid_;
}

bool GoogleUrl::IsAnyValid() const {
  return gurl_.is_valid();
}

GoogleUrl* GoogleUrl::CopyAndAddEscapedQueryParam(
    const StringPiece& name, const StringPiece& escaped_value) const {
  QueryParams query_params;
  query_params.Parse(Query());
  query_params.AddEscaped(name, escaped_value);
  GoogleString query_params_string = query_params.ToEscapedString();
  url_canon::Replacements<char> replace_query;
  url_parse::Component query;
  query.len = query_params_string.size();
  replace_query.SetQuery(query_params_string.c_str(), query);
  GoogleUrl* result =
      new GoogleUrl(gurl_.ReplaceComponents(replace_query));
  return result;
}

size_t GoogleUrl::LeafEndPosition(const GURL& gurl) {
  url_parse::Parsed parsed = gurl.parsed_for_possibly_invalid_spec();
  if (parsed.path.is_valid()) {
    return parsed.path.end();
  }
  if (parsed.port.is_valid()) {
    return parsed.port.end();
  }
  if (parsed.host.is_valid()) {
    return parsed.host.end();
  }
  if (parsed.password.is_valid()) {
    return parsed.password.end();
  }
  if (parsed.username.is_valid()) {
    return parsed.username.end();
  }
  if (parsed.scheme.is_valid()) {
    return parsed.scheme.end();
  }
  return npos;
}

// Returns the offset at which the leaf ends in valid url spec.
// If there is no path, steps backward until valid end is found.
size_t GoogleUrl::LeafEndPosition() const {
  return LeafEndPosition(gurl_);
}

size_t GoogleUrl::LeafStartPosition(const GURL& gurl) {
  url_parse::Parsed parsed = gurl.parsed_for_possibly_invalid_spec();
  size_t start_reverse_search_from = npos;
  if (parsed.query.is_valid() && (parsed.query.begin > 0)) {
    // query includes '?', so start the search from the character
    // before it.
    start_reverse_search_from = parsed.query.begin - 1;
  }
  return gurl.possibly_invalid_spec().rfind('/', start_reverse_search_from);
}

// Returns the offset at which the leaf starts in the fully
// qualified spec.
size_t GoogleUrl::LeafStartPosition() const {
  return LeafStartPosition(gurl_);
}

size_t GoogleUrl::PathStartPosition(const GURL& gurl) {
  const std::string& spec = gurl.spec();
  url_parse::Parsed parsed = gurl.parsed_for_possibly_invalid_spec();
  size_t origin_size = parsed.path.begin;
  if (!parsed.path.is_valid()) {
    origin_size = spec.size();
  }
  CHECK_LT(0, static_cast<int>(origin_size));
  CHECK_LE(origin_size, spec.size());
  return origin_size;
}

// Find the start of the path, includes '/'
size_t GoogleUrl::PathStartPosition() const {
  return PathStartPosition(gurl_);
}

StringPiece GoogleUrl::AllExceptQuery() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return StringPiece();
  }

  const std::string& spec = gurl_.possibly_invalid_spec();
  size_t leaf_end = LeafEndPosition();
  if (leaf_end == npos) {
    return StringPiece();
  } else {
    return StringPiece(spec.data(), leaf_end);
  }
}

StringPiece GoogleUrl::AllAfterQuery() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return StringPiece();
  }

  const std::string& spec = gurl_.possibly_invalid_spec();
  url_parse::Parsed parsed = gurl_.parsed_for_possibly_invalid_spec();
  size_t query_end;
  if (gurl_.has_query()) {
    query_end = parsed.query.end();
  } else {
    query_end = LeafEndPosition();
  }
  if (query_end == npos) {
    return StringPiece();
  } else {
    return StringPiece(spec.data() + query_end, spec.size() - query_end);
  }
}

// Find the last slash before the question-mark, if any.  See
// http://en.wikipedia.org/wiki/URI_scheme -- the query-string
// syntax is not well-defined.  But the query-separator is well-defined:
// it's a ? so I believe this implies that the first ? has to delimit
// the query string.
StringPiece GoogleUrl::AllExceptLeaf() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return StringPiece();
  }

  size_t last_slash = LeafStartPosition();
  if (last_slash == npos) {
    // No leaf found.
    return StringPiece();
  } else {
    size_t after_last_slash = last_slash + 1;
    return StringPiece(gurl_.spec().data(), after_last_slash);
  }
}

StringPiece GoogleUrl::LeafWithQuery() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return StringPiece();
  }

  size_t last_slash = LeafStartPosition();
  if (last_slash == npos) {
    // No slashes found.
    return StringPiece();
  } else {
    size_t after_last_slash = last_slash + 1;
    const std::string& spec = gurl_.spec();
    return StringPiece(spec.data() + after_last_slash,
                       spec.size() - after_last_slash);
  }
}

StringPiece GoogleUrl::LeafSansQuery() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return StringPiece();
  }

  size_t leaf_start = LeafStartPosition();
  if (leaf_start == npos) {
    return StringPiece();
  }
  size_t after_last_slash = leaf_start + 1;
  const std::string& spec = gurl_.spec();
  size_t leaf_length = spec.size() - after_last_slash;
  if (!gurl_.has_query()) {
    return StringPiece(spec.data() + after_last_slash, leaf_length);
  }
  url_parse::Parsed parsed = gurl_.parsed_for_possibly_invalid_spec();
  if (!parsed.query.is_valid()) {
    return StringPiece();
  } else {
    // parsed.query.len doesn't include the '?'
    return StringPiece(spec.data() + after_last_slash,
                       leaf_length - (parsed.query.len + 1));
  }
}

// For "http://a.com/b/c/d?e=f/g returns "http://a.com" without trailing slash
StringPiece GoogleUrl::Origin() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return StringPiece();
  }

  size_t origin_size = PathStartPosition();
  if (origin_size == npos) {
    return StringPiece();
  } else {
    return StringPiece(gurl_.spec().data(), origin_size);
  }
}

// For "http://a.com/b/c/d?e=f/g returns "/b/c/d?e=f/g" including leading slash
StringPiece GoogleUrl::PathAndLeaf() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return StringPiece();
  }

  size_t origin_size = PathStartPosition();
  if (origin_size == npos) {
    return StringPiece();
  } else {
    const std::string& spec = gurl_.spec();
    return StringPiece(spec.data() + origin_size, spec.size() - origin_size);
  }
}

// For "http://a.com/b/c/d/g.html?q=v" returns "/b/c/d/" including leading and
// trailing slashes.
StringPiece GoogleUrl::PathSansLeaf() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return StringPiece();
  }

  size_t path_start = PathStartPosition();
  size_t leaf_start = LeafStartPosition();
  if (path_start == npos || leaf_start == npos) {
    // Things like data: URLs do not have leaves, etc.
    return StringPiece();
  } else {
    size_t after_last_slash = leaf_start + 1;
    return StringPiece(gurl_.spec().data() + path_start,
                       after_last_slash - path_start);
  }
}

StringPiece GoogleUrl::NetPath() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return StringPiece();
  }

  if (!gurl_.has_scheme()) {
    return Spec();
  }
  const std::string& spec = gurl_.possibly_invalid_spec();
  url_parse::Parsed parsed = gurl_.parsed_for_possibly_invalid_spec();
  // Just remove scheme and : from beginning of URL.
  return StringPiece(spec.data() + parsed.scheme.end() + 1,
                     spec.size() - parsed.scheme.end() - 1);
}

// Extracts the filename portion of the path and returns it. The filename
// is everything after the last slash in the path. This may be empty.
GoogleString GoogleUrl::ExtractFileName() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return "";
  }

  return gurl_.ExtractFileName();
}

StringPiece GoogleUrl::Host() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return StringPiece();
  }

  if (!gurl_.has_host()) {
    return StringPiece();
  }
  url_parse::Parsed parsed = gurl_.parsed_for_possibly_invalid_spec();
  return StringPiece(gurl_.spec().data() + parsed.host.begin,
                     parsed.host.len);
}

StringPiece GoogleUrl::HostAndPort() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return StringPiece();
  }

  if (!gurl_.has_host()) {
    return StringPiece();
  }
  url_parse::Parsed parsed = gurl_.parsed_for_possibly_invalid_spec();
  return StringPiece(gurl_.spec().data() + parsed.host.begin,
                     parsed.host.len + parsed.port.len + 1);  // Yes, it works.
}

StringPiece GoogleUrl::PathSansQuery() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return StringPiece();
  }

  url_parse::Parsed parsed = gurl_.parsed_for_possibly_invalid_spec();
  size_t path_start = PathStartPosition();
  if (path_start == npos || !parsed.path.is_valid()) {
    return StringPiece();
  } else {
    return StringPiece(gurl_.spec().data() + path_start, parsed.path.len);
  }
}

StringPiece GoogleUrl::Query() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return StringPiece();
  }

  if (!gurl_.has_query()) {
    return StringPiece();
  }
  url_parse::Parsed parsed = gurl_.parsed_for_possibly_invalid_spec();
  return StringPiece(gurl_.spec().data() + parsed.query.begin,
                     parsed.query.len);
}

StringPiece GoogleUrl::Scheme() const {
  if (!gurl_.is_valid()) {
    LOG(DFATAL) << "Invalid URL: " << gurl_.possibly_invalid_spec();
    return StringPiece();
  }

  if (!gurl_.has_scheme()) {
    return StringPiece();
  }
  url_parse::Parsed parsed = gurl_.parsed_for_possibly_invalid_spec();
  return StringPiece(gurl_.spec().data() + parsed.scheme.begin,
                     parsed.scheme.len);
}

StringPiece GoogleUrl::Spec() const {
  const std::string& spec = gurl_.spec();
  return StringPiece(spec.data(), spec.size());
}

StringPiece GoogleUrl::UncheckedSpec() const {
  const std::string& spec = gurl_.possibly_invalid_spec();
  return StringPiece(spec.data(), spec.size());
}

UrlRelativity GoogleUrl::FindRelativity(StringPiece url) {
  GoogleUrl temp(url);
  if (temp.IsAnyValid()) {
    return kAbsoluteUrl;
  } else if (url.starts_with("//")) {
    return kNetPath;
  } else if (url.starts_with("/")) {
    return kAbsolutePath;
  } else {
    return kRelativePath;
  }
}

StringPiece GoogleUrl::Relativize(UrlRelativity url_relativity,
                                  const GoogleUrl& base_url) const {
  // Default, in case we cannot relativize appropriately.
  StringPiece result = Spec();

  switch (url_relativity) {
    case kRelativePath: {
      StringPiece url_spec = Spec();
      StringPiece relative_path = base_url.AllExceptLeaf();
      if (url_spec.starts_with(relative_path)) {
        result = url_spec.substr(relative_path.size());
      }
      break;  // TODO(sligocki): Should we fall through here?
    }
    case kAbsolutePath:
      if (Origin() == base_url.Origin()) {
        result = PathAndLeaf();
      }
      break;
    case kNetPath:
      if (Scheme() == base_url.Scheme()) {
        result = NetPath();
      }
      break;
    case kAbsoluteUrl:
      result = Spec();
      break;
  }

  // There are several corner cases that the naive algorithm above fails on.
  // Ex: http://foo.com/?bar or http://foo.com//bar relative to
  // http://foo.com/bar.html. Check if result resolves correctly and if not,
  // return absolute URL.
  GoogleUrl resolved_result(base_url, result);
  if (resolved_result != *this) {
    result = Spec();
  }

  return result;
}

namespace {

// Parsing states for GoogleUrl::Unescape
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

GoogleString GoogleUrl::UnescapeHelper(const StringPiece& escaped_url,
                                       bool convert_plus_to_space) {
  GoogleString unescaped_url, escape_text;
  unsigned char escape_value;
  UnescapeState state = NORMAL;
  int iter = 0;
  int n = escaped_url.size();
  while (iter < n) {
    char c = escaped_url[iter];
    switch (state) {
      case NORMAL:
        if (c == '%') {
          escape_text.clear();
          state = ESCAPE1;
        } else {
          if ((c == '+') && convert_plus_to_space) {
            c = ' ';
          }
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

GoogleString GoogleUrl::Escape(const StringPiece& unescaped) {
  GoogleString escaped;
  for (const char* p = unescaped.data(), *e = p + unescaped.size();
       p < e; ++p) {
    // See http://en.wikipedia.org/wiki/Query_string#URL_encoding
    char c = *p;
    if (IsAsciiAlphaNumeric(c) || (c == '.') || (c == '~') || (c == '_') ||
        (c == '-')) {
      escaped.push_back(c);
    } else if (c == ' ') {
      escaped.push_back('+');
    } else {
      StrAppend(&escaped, StringPrintf(
          "%%%02x", static_cast<unsigned int>(static_cast<unsigned char>(c))));
    }
  }
  return escaped;
}

}  // namespace net_instaweb
