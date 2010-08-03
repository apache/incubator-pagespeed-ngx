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

namespace net_instaweb {

MetaData::~MetaData() {
}

void MetaData::CopyFrom(const MetaData& other) {
  set_major_version(other.major_version());
  set_minor_version(other.minor_version());
  set_status_code(other.status_code());
  set_reason_phrase(other.reason_phrase());
  for (int i = 0; i < other.NumAttributes(); ++i) {
    Add(other.Name(i), other.Value(i));
  }
  ComputeCaching();
}

namespace {

const char* GetReasonPhrase(HttpStatus::Code rc) {
  switch (rc) {
    case HttpStatus::CONTINUE                : return "Continue";
    case HttpStatus::SWITCHING_PROTOCOLS     : return "Switching Protocols";

    case HttpStatus::OK                      : return "OK";
    case HttpStatus::CREATED                 : return "Created";
    case HttpStatus::ACCEPTED                : return "Accepted";
    case HttpStatus::NON_AUTHORITATIVE       :
      return "Non-Authoritative Information";
    case HttpStatus::NO_CONTENT              : return "No Content";
    case HttpStatus::RESET_CONTENT           : return "Reset Content";
    case HttpStatus::PARTIAL_CONTENT         : return "Partial Content";

      // 300 range: redirects
    case HttpStatus::MULTIPLE_CHOICES        : return "Multiple Choices";
    case HttpStatus::MOVED_PERMANENTLY       : return "Moved Permanently";
    case HttpStatus::FOUND                   : return "Found";
    case HttpStatus::SEE_OTHER               : return "Not Modified";
    case HttpStatus::USE_PROXY               : return "Use Proxy";
    case HttpStatus::TEMPORARY_REDIRECT      : return "OK";

      // 400 range: client errors
    case HttpStatus::BAD_REQUEST             : return "Bad Request";
    case HttpStatus::UNAUTHORIZED            : return "Unauthorized";
    case HttpStatus::PAYMENT_REQUIRED        : return "Payment Required";
    case HttpStatus::FORBIDDEN               : return "Forbidden";
    case HttpStatus::NOT_FOUND               : return "Not Found";
    case HttpStatus::METHOD_NOT_ALLOWED      : return "Method Not Allowed";
    case HttpStatus::NOT_ACCEPTABLE          : return "Not Acceptable";
    case HttpStatus::PROXY_AUTH_REQUIRED     :
      return "Proxy Authentication Required";
    case HttpStatus::REQUEST_TIMEOUT         : return "Request Time-out";
    case HttpStatus::CONFLICT                : return "Conflict";
    case HttpStatus::GONE                    : return "Gone";
    case HttpStatus::LENGTH_REQUIRED         : return "Length Required";
    case HttpStatus::PRECONDITION_FAILED     : return "Precondition Failed";
    case HttpStatus::ENTITY_TOO_LARGE        :
      return "Request Entity Too Large";
    case HttpStatus::URI_TOO_LONG            : return "Request-URI Too Large";
    case HttpStatus::UNSUPPORTED_MEDIA_TYPE  : return "Unsupported Media Type";
    case HttpStatus::RANGE_NOT_SATISFIABLE   :
      return "Requested range not satisfiable";
    case HttpStatus::EXPECTATION_FAILED      : return "Expectation Failed";

      // 500 range: server errors
    case HttpStatus::INTERNAL_SERVER_ERROR   : return "Internal Server Error";
    case HttpStatus::NOT_IMPLEMENTED         : return "Not Implemented";
    case HttpStatus::BAD_GATEWAY             : return "Bad Gateway";
    case HttpStatus::UNAVAILABLE             : return "Service Unavailable";
    case HttpStatus::GATEWAY_TIMEOUT         : return "Gateway Time-out";

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

}  // namespace net_instaweb
