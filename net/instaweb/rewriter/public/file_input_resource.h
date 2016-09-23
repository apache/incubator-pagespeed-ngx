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

// Author: sligocki@google.com (Shawn Ligocki)
//
// Input resource created based on a local file.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_FILE_INPUT_RESOURCE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_FILE_INPUT_RESOURCE_H_

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

struct ContentType;
class InputInfo;
class MessageHandler;
class ResponseHeaders;
class RewriteDriver;

class FileInputResource : public Resource {
 public:
  FileInputResource(const RewriteDriver* driver,
                    const ContentType* type,
                    StringPiece url,
                    StringPiece filename);
  virtual ~FileInputResource();

  // Uses default no-op Freshen implementation because file-based resources
  // are fetched each time they are needed.

  virtual bool IsValidAndCacheable() const;

  // Set OutputPartition's input info used for expiration validation.
  virtual void FillInPartitionInputInfo(HashHint include_content_hash,
                                        InputInfo* input);

  virtual GoogleString url() const { return url_; }

  virtual bool UseHttpCache() const { return false; }

 protected:
  void SetDefaultHeaders(const ContentType* content_type,
                         ResponseHeaders* header, MessageHandler* handler);

  virtual void LoadAndCallback(NotCacheablePolicy not_cacheable_policy,
                               const RequestContextPtr& request_context,
                               AsyncCallback* callback);

 private:
  GoogleString url_;
  GoogleString filename_;
  int64 last_modified_time_sec_;  // Loaded from file mtime.
  int64 max_file_size_;
  int64 load_from_file_cache_ttl_ms_;
  bool load_from_file_ttl_set_;

  DISALLOW_COPY_AND_ASSIGN(FileInputResource);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FILE_INPUT_RESOURCE_H_
