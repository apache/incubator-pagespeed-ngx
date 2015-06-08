#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DATA_URL_INPUT_RESOURCE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DATA_URL_INPUT_RESOURCE_H_

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

// Author: jmaessen@google.com (Jan Maessen)
//
// An input resource representing a data: url.  This is uncommon in web
// pages, but we generate these urls as a result of image inlining and
// this confuses subsequent filters in certain cases.

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/data_url.h"

namespace net_instaweb {

class InputInfo;
class RewriteDriver;
struct ContentType;

class DataUrlInputResource : public Resource {
 public:
  // We expose a factory; parse failure returns NULL.
  static ResourcePtr Make(const StringPiece& url, const RewriteDriver* driver) {
    ResourcePtr resource;
    const ContentType* type;
    Encoding encoding;
    StringPiece encoded_contents;
    // We create the local copy of the url early, because
    // encoded_contents will in general be a substring of this
    // local copy and must have the same lifetime.
    GoogleString* url_copy = new GoogleString();
    url.CopyToString(url_copy);
    if (ParseDataUrl(*url_copy, &type, &encoding, &encoded_contents)) {
      resource.reset(new DataUrlInputResource(url_copy, encoding, type,
                                              encoded_contents, driver));
    }
    return resource;
  }

  virtual ~DataUrlInputResource();

  virtual bool IsValidAndCacheable() const;

  // Set OutputPartition's input info used for expiration validation.
  virtual void FillInPartitionInputInfo(HashHint include_content_hash,
                                        InputInfo* input);

  virtual GoogleString url() const { return *url_.get(); }

  virtual bool UseHttpCache() const { return false; }

 protected:
  virtual void LoadAndCallback(NotCacheablePolicy not_cacheable_policy,
                               const RequestContextPtr& request_context,
                               AsyncCallback* callback);

 private:
  DataUrlInputResource(const GoogleString* url,
                       Encoding encoding,
                       const ContentType* type,
                       const StringPiece& encoded_contents,
                       const RewriteDriver* driver);

  scoped_ptr<const GoogleString> url_;
  const Encoding encoding_;
  const StringPiece encoded_contents_;  // substring of url.
  GoogleString decoded_contents_;

  DISALLOW_COPY_AND_ASSIGN(DataUrlInputResource);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DATA_URL_INPUT_RESOURCE_H_
