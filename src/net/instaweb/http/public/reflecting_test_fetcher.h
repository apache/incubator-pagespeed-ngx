/*
 * Copyright 2012 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
//
// Contains ReflectingTestFetcher, which just echoes its input. Meant for use in
// unit tests.

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class MessageHandler;

// A fetcher that reflects headers it gets back into response headers,
// and the URL inside body. We use it to test that we are setting proper
// headers when we are generating requests ourselves.
class ReflectingTestFetcher : public UrlAsyncFetcher {
 public:
  ReflectingTestFetcher() {}
  virtual ~ReflectingTestFetcher() {}

  virtual bool Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch) {
    RequestHeaders* in = fetch->request_headers();
    ResponseHeaders* out = fetch->response_headers();
    out->SetStatusAndReason(HttpStatus::kOK);
    for (int i = 0; i < in->NumAttributes(); ++i) {
      out->Add(in->Name(i), in->Value(i));
    }
    fetch->Write(url, message_handler);
    fetch->Done(true);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ReflectingTestFetcher);
};

}  // namespace net_instaweb
