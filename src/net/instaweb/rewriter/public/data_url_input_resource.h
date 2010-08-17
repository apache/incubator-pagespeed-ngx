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

// Author: jmaessen@google.com (Jan Maessen)
//
// An input resource representing a data: url.  This is uncommon in web
// pages, but we generate these urls as a result of image inlining and
// this confuses subsequent filters in certain cases.

#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/util/public/data_url.h"

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DATA_URL_INPUT_RESOURCE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DATA_URL_INPUT_RESOURCE_H_

namespace net_instaweb {

class ResourceManager;
class ContentType;
enum Encoding;

class DataUrlInputResource : public Resource {
 public:
  // We expose a factory; parse failure returns NULL.
  static DataUrlInputResource* Make(const StringPiece& url,
                                    ResourceManager* manager) {
    const ContentType* type;
    Encoding encoding;
    StringPiece encoded_contents;
    if (!ParseDataUrl(url, &type, &encoding, &encoded_contents)) {
      return NULL;
    }
    return new DataUrlInputResource(url, encoding, type, encoded_contents,
                                    manager);
  }

  virtual ~DataUrlInputResource() { }

  virtual std::string url() const { return url_; }

 protected:
  // Read complete resource, content is stored in contents_.
  virtual bool ReadIfCached(MessageHandler* message_handler);

 private:
  DataUrlInputResource(const StringPiece& url,
                       Encoding encoding,
                       const ContentType* type,
                       const StringPiece& encoded_contents,
                       ResourceManager* manager)
      : Resource(manager, type),
        url_(url.data(), url.size()),
        encoding_(encoding),
        encoded_contents_(encoded_contents) {
  }

  const std::string url_;
  const Encoding encoding_;
  const StringPiece encoded_contents_;
  std::string decoded_contents_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DATA_URL_INPUT_RESOURCE_H_
