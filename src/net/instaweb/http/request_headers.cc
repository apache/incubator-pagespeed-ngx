// Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/http/public/request_headers.h"

#include <vector>
#include "base/logging.h"
#include "net/instaweb/http/http.pb.h"  // for HttpRequestHeaders
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/string_multi_map.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

RequestHeaders::RequestHeaders() {
  proto_.reset(new HttpRequestHeaders);
}

void RequestHeaders::Clear() {
  Headers<HttpRequestHeaders>::Clear();
  proto_->clear_method();
}

void RequestHeaders::CopyFrom(const RequestHeaders& other) {
  map_.reset(NULL);
  *(proto_.get()) = *(other.proto_.get());
}

std::string RequestHeaders::ToString() const {
  std::string str;
  StringWriter writer(&str);
  WriteAsHttp("", &writer, NULL);
  return str;
}

// To avoid having every transitive dependency pull in the generated
// protobuf header file during compilation, we have a distinct enum
// for the RequestHeaders interface class.  We translate with switch
// statements rather than array lookups just so we don't have to bother
// initializing the array.
void RequestHeaders::set_method(Method method) {
  switch (method) {
    case kOptions:     proto_->set_method(HttpRequestHeaders::OPTIONS); break;
    case kGet:         proto_->set_method(HttpRequestHeaders::GET);     break;
    case kHead:        proto_->set_method(HttpRequestHeaders::HEAD);    break;
    case kPost:        proto_->set_method(HttpRequestHeaders::POST);    break;
    case kPut:         proto_->set_method(HttpRequestHeaders::PUT);     break;
    case kDelete:      proto_->set_method(HttpRequestHeaders::DELETE);  break;
    case kTrace:       proto_->set_method(HttpRequestHeaders::TRACE);   break;
    case kConnect:     proto_->set_method(HttpRequestHeaders::CONNECT); break;
  }
}

RequestHeaders::Method RequestHeaders::method() const {
  switch (proto_->method()) {
    case HttpRequestHeaders::OPTIONS:     return kOptions;
    case HttpRequestHeaders::GET:         return kGet;
    case HttpRequestHeaders::HEAD:        return kHead;
    case HttpRequestHeaders::POST:        return kPost;
    case HttpRequestHeaders::PUT:         return kPut;
    case HttpRequestHeaders::DELETE:      return kDelete;
    case HttpRequestHeaders::TRACE:       return kTrace;
    case HttpRequestHeaders::CONNECT:     return kConnect;
  }
  DCHECK(false) << "Invalid method";
  return kGet;
}

const char* RequestHeaders::method_string() const {
  switch (proto_->method()) {
    case HttpRequestHeaders::OPTIONS:     return "OPTIONS";
    case HttpRequestHeaders::GET:         return "GET";
    case HttpRequestHeaders::HEAD:        return "HEAD";
    case HttpRequestHeaders::POST:        return "POST";
    case HttpRequestHeaders::PUT:         return "PUT";
    case HttpRequestHeaders::DELETE:      return "DELETE";
    case HttpRequestHeaders::TRACE:       return "TRACE";
    case HttpRequestHeaders::CONNECT:     return "CONNECT";
  }
  DCHECK(false) << "Invalid method";
  return NULL;
}

// Serialize meta-data to a binary stream.
bool RequestHeaders::WriteAsHttp(
    const StringPiece& url, Writer* writer, MessageHandler* handler) const {
  bool ret = true;
  std::string buf = StringPrintf("%s %s HTTP/%d.%d\r\n",
                                  method_string(), url.as_string().c_str(),
                                  major_version(), minor_version());
  ret &= writer->Write(buf, handler);
  ret &= Headers<HttpRequestHeaders>::WriteAsHttp(writer, handler);
  return ret;
}

bool RequestHeaders::AcceptsGzip() const {
  StringStarVector v;
  if (Lookup(HttpAttributes::kAcceptEncoding, &v)) {
    for (int i = 0, nv = v.size(); i < nv; ++i) {
      std::vector<StringPiece> encodings;
      SplitStringPieceToVector(*(v[i]), ",", &encodings, true);
      for (int j = 0, nencodings = encodings.size(); j < nencodings; ++j) {
        if (StringCaseEqual(encodings[j], HttpAttributes::kGzip)) {
          return true;
        }
      }
    }
  }
  return false;
}

}  // namespace net_instaweb
