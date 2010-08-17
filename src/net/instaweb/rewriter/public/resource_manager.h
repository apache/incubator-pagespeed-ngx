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
//     and sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_H_

#include <map>
#include <vector>
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/http_cache.h"
#include "net/instaweb/util/public/meta_data.h"
#include "net/instaweb/rewriter/public/resource.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

class GURL;

namespace net_instaweb {

class ContentType;
class FileSystem;
class FilenameEncoder;
class HTTPCache;
class HTTPValue;
class Hasher;
class MessageHandler;
class MetaData;
class OutputResource;
class Statistics;
class UrlAsyncFetcher;
class Writer;

class ResourceManager {
 public:
  ResourceManager(const StringPiece& file_prefix,
                  const StringPiece& url_prefix,
                  const int num_shards,
                  FileSystem* file_system,
                  FilenameEncoder* filename_encoder,
                  UrlAsyncFetcher* url_async_fetcher,
                  Hasher* hasher,
                  HTTPCache* http_cache);
  ~ResourceManager();

  // Created resources are managed by ResourceManager and eventually deleted
  // by ResourceManager's destructor.

  // Creates an output resource with a generated name.  Such a
  // resource can only be meaningfully created in a deployment with
  // shared persistent storage, such as a the local disk on a
  // single-server system, or a multi-server configuration with a
  // database, network attached storage, or a shared cache such as
  // memcached.
  //
  // If this is not available in the current deployment, then NULL is returned.
  //
  // Every time this method is called, a new resource is generated.
  //
  // 'type' arg can be null if it's not known, or is not in our ContentType
  // library.
  OutputResource* CreateGeneratedOutputResource(
      const StringPiece& filter_prefix, const ContentType* type,
      MessageHandler* handler);

  // Creates an output resource where the name is provided by the rewriter.
  // The intent is to be able to derive the content from the name, for example,
  // by encoding URLs and metadata.
  //
  // This method is not dependent on shared persistent storage, and always
  // succeeds.
  //
  // This name is prepended with url_prefix for writing hrefs, and
  // file_prefix when working with the file system.  So files are:
  //    $(FILE_PREFIX)$(FILTER_PREFIX).$(HASH).$(NAME).$(CONTENT_TYPE_EXT)
  // and hrefs are:
  //    $(URL_PREFIX)$(FILTER_PREFIX).$(HASH).$(NAME).$(CONTENT_TYPE_EXT)
  //
  // 'type' arg can be null if it's not known, or is not in our ContentType
  // library.
  //
  // TODO(jmarantz): add a new variant which creates an output resource from
  // an input resource, to inherit content type, cache expiration,
  // last-modified, etc.
  OutputResource* CreateNamedOutputResource(
      const StringPiece& filter_prefix, const StringPiece& name,
      const ContentType* type, MessageHandler* handler);

  // Creates a resource based on the fields extracted from a URL.  This
  // is used for serving output resources.
  //
  // 'type' arg can be null if it's not known, or is not in our ContentType
  // library.
  OutputResource* CreateUrlOutputResource(
      const StringPiece& filter_prefix, const StringPiece& name,
      const StringPiece& hash, const ContentType* type);

  Resource* CreateInputResource(const StringPiece& url,
                                MessageHandler* handler);

  // Set up a basic header for a given content_type.
  // If content_type is null, the Content-Type is omitted.
  // This method may only be called once on a header.
  void SetDefaultHeaders(const ContentType* content_type,
                         MetaData* header) const;

  // Changes the content type of a pre-initialized header.
  void SetContentType(const ContentType* content_type, MetaData* header);

  std::string base_url() const;
  StringPiece filename_prefix() const { return file_prefix_; }
  StringPiece url_prefix() const { return url_prefix_; }

  void set_filename_prefix(const StringPiece& file_prefix);
  void set_url_prefix(const StringPiece& url_prefix);
  void set_base_url(const StringPiece& url);
  Statistics* statistics() const { return statistics_; }
  void set_statistics(Statistics* s) { statistics_ = s; }
  void set_relative_path(bool x) { relative_path_ = x; }

  bool FetchOutputResource(
    OutputResource* output_resource,
    Writer* writer, MetaData* response_headers,
    MessageHandler* handler) const;

  // Writes the specified contents into the output resource, retaining
  // both a name->filename map and the filename->contents map.
  //
  // TODO(jmarantz): add last_modified arg.
  bool Write(HttpStatus::Code status_code,
             const StringPiece& contents, OutputResource* output,
             int64 origin_expire_time_ms, MessageHandler* handler);

  // Read resource contents & headers, returning false if the resource
  // is not already cached, in which case an async request is queued.
  bool ReadIfCached(Resource* resource, MessageHandler* message_handler) const;

  // Read contents of resource asynchronously, calling callback when
  // done.  If the resource contents is cached, the callback will
  // be called directly, rather than asynchronously.
  //
  // TODO(jmarantz): currently, ReadAsync does *not* fill in the contents
  // and meta-data in the Resource, because it doesn't assume that the
  // Resource will live till the callback is called.  This is actually
  // harder to use, but was easier to implement.  We will change this in
  // the future so that the burden is placed on the implementation.
  void ReadAsync(Resource* resource, Resource::AsyncCallback* callback,
                 MessageHandler* message_handler);

  // TODO(jmarantz): check thread safety in Apache.
  Hasher* hasher() { return hasher_; }
  FileSystem* file_system() { return file_system_; }
  FilenameEncoder* filename_encoder() { return filename_encoder_; }
  UrlAsyncFetcher* url_async_fetcher() { return url_async_fetcher_; }
  Timer* timer() const { return http_cache_->timer(); }

 private:
  std::string ConstructNameKey(OutputResource* output) const;

  scoped_ptr<GURL> base_url_;  // Base url to resolve relative urls against.
  std::string file_prefix_;
  std::string url_prefix_;
  int num_shards_;   // NYI: For server sharding of Resources.
  int resource_id_;  // Sequential ids for temporary Resource filenames.
  FileSystem* file_system_;
  FilenameEncoder* filename_encoder_;
  UrlAsyncFetcher* url_async_fetcher_;
  Hasher* hasher_;
  Statistics* statistics_;
  HTTPCache* http_cache_;
  bool relative_path_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_H_
