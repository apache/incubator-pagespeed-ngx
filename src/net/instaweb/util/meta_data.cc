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

#include "net/instaweb/util/public/meta_data.h"

#include <stdio.h>
#include "net/instaweb/util/public/time_util.h"
#include "pagespeed/core/resource_util.h"

namespace net_instaweb {

const char HttpAttributes::kAcceptEncoding[] = "Accept-Encoding";
const char HttpAttributes::kCacheControl[] = "Cache-Control";
const char HttpAttributes::kContentEncoding[] = "Content-Encoding";
const char HttpAttributes::kContentLength[] = "Content-Length";
const char HttpAttributes::kContentType[] = "Content-Type";
const char HttpAttributes::kDate[] = "Date";
const char HttpAttributes::kDeflate[] = "deflate";
const char HttpAttributes::kEtag[] = "Etag";
const char HttpAttributes::kExpires[] = "Expires";
const char HttpAttributes::kGzip[] = "gzip";
const char HttpAttributes::kLastModified[] = "Last-Modified";
const char HttpAttributes::kLocation[] = "Location";
const char HttpAttributes::kServer[] = "Server";
const char HttpAttributes::kSetCookie[] = "Set-Cookie";
const char HttpAttributes::kTransferEncoding[] = "Transfer-Encoding";
const char HttpAttributes::kUserAgent[] = "User-Agent";

MetaData::~MetaData() {
}

void MetaData::CopyFrom(const MetaData& other) {
  set_major_version(other.major_version());
  set_minor_version(other.minor_version());
  set_status_code(other.status_code());
  set_reason_phrase(other.reason_phrase());
  set_headers_complete(other.headers_complete());
  for (int i = 0; i < other.NumAttributes(); ++i) {
    Add(other.Name(i), other.Value(i));
  }
  ComputeCaching();
}

namespace {

const char* GetReasonPhrase(HttpStatus::Code rc) {
  switch (rc) {
    case HttpStatus::kContinue                : return "Continue";
    case HttpStatus::kSwitchingProtocols      : return "Switching Protocols";

    case HttpStatus::kOK                      : return "OK";
    case HttpStatus::kCreated                 : return "Created";
    case HttpStatus::kAccepted                : return "Accepted";
    case HttpStatus::kNonAuthoritative        :
      return "Non-Authoritative Information";
    case HttpStatus::kNoContent               : return "No Content";
    case HttpStatus::kResetContent            : return "Reset Content";
    case HttpStatus::kPartialContent          : return "Partial Content";

      // 300 range: redirects
    case HttpStatus::kMultipleChoices         : return "Multiple Choices";
    case HttpStatus::kMovedPermanently        : return "Moved Permanently";
    case HttpStatus::kFound                   : return "Found";
    case HttpStatus::kSeeOther                : return "Not Modified";
    case HttpStatus::kUseProxy                : return "Use Proxy";
    case HttpStatus::kTemporaryRedirect       : return "OK";

      // 400 range: client errors
    case HttpStatus::kBadRequest              : return "Bad Request";
    case HttpStatus::kUnauthorized            : return "Unauthorized";
    case HttpStatus::kPaymentRequired         : return "Payment Required";
    case HttpStatus::kForbidden               : return "Forbidden";
    case HttpStatus::kNotFound                : return "Not Found";
    case HttpStatus::kMethodNotAllowed        : return "Method Not Allowed";
    case HttpStatus::kNotAcceptable           : return "Not Acceptable";
    case HttpStatus::kProxyAuthRequired       :
      return "Proxy Authentication Required";
    case HttpStatus::kRequestTimeout          : return "Request Time-out";
    case HttpStatus::kConflict                : return "Conflict";
    case HttpStatus::kGone                    : return "Gone";
    case HttpStatus::kLengthRequired          : return "Length Required";
    case HttpStatus::kPreconditionFailed      : return "Precondition Failed";
    case HttpStatus::kEntityTooLarge          :
      return "Request Entity Too Large";
    case HttpStatus::kUriTooLong              : return "Request-URI Too Large";
    case HttpStatus::kUnsupportedMediaType    : return "Unsupported Media Type";
    case HttpStatus::kRangeNotSatisfiable     :
      return "Requested range not satisfiable";
    case HttpStatus::kExpectationFailed       : return "Expectation Failed";

      // 500 range: server errors
    case HttpStatus::kInternalServerError     : return "Internal Server Error";
    case HttpStatus::kNotImplemented          : return "Not Implemented";
    case HttpStatus::kBadGateway              : return "Bad Gateway";
    case HttpStatus::kUnavailable             : return "Service Unavailable";
    case HttpStatus::kGatewayTimeout          : return "Gateway Time-out";

    default:
      // We don't have a name for this response code, so we'll just
      // take the blame
      return "Internal Server Error";
  }
  return "";
}

}  // namespace

void MetaData::SetStatusAndReason(HttpStatus::Code code) {
  set_status_code(code);
  set_reason_phrase(GetReasonPhrase(code));
}

bool MetaData::ParseTime(const char* time_str, int64* time_ms) {
  return pagespeed::resource_util::ParseTimeValuedHeader(time_str, time_ms);
}

bool MetaData::IsGzipped() const {
  CHECK(headers_complete());
  CharStarVector v;
  return (Lookup(HttpAttributes::kContentEncoding, &v) && (v.size() == 1) &&
          (strcmp(v[0], HttpAttributes::kGzip) == 0));
}

bool MetaData::AcceptsGzip() const {
  CharStarVector v;
  if (Lookup(HttpAttributes::kAcceptEncoding, &v)) {
    for (int i = 0, nv = v.size(); i < nv; ++i) {
      std::vector<StringPiece> encodings;
      SplitStringPieceToVector(v[i], ",", &encodings, true);
      for (int j = 0, nencodings = encodings.size(); j < nencodings; ++j) {
        if (strcasecmp(encodings[j].as_string().c_str(),
                       HttpAttributes::kGzip) == 0) {
          return true;
        }
      }
    }
  }
  return false;
}

bool MetaData::ParseDateHeader(const char* attr, int64* date_ms) const {
  CharStarVector values;
  return (Lookup(attr, &values) &&
          (values.size() == 1) &&
          ConvertStringToTime(values[0], date_ms));
}

void MetaData::UpdateDateHeader(const char* attr, int64 date_ms) {
  RemoveAll(attr);
  std::string buf;
  ConvertTimeToString(date_ms, &buf);
  Add(attr, buf.c_str());
}

void MetaData::DebugPrint() const {
  fprintf(stderr, "%s\n", ToString().c_str());
}

}  // namespace net_instaweb
