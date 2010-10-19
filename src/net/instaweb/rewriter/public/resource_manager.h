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
#include "base/basictypes.h"
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
class ResourceNamer;
class Statistics;
class UrlAsyncFetcher;
class UrlEscaper;
class Writer;

class ResourceManager {
 public:
  static const int kNotSharded;

  ResourceManager(const StringPiece& file_prefix,
                  const StringPiece& url_prefix_pattern,
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
  // If this is not available in the current deployment, it is illegal
  // to call this routine.
  // TODO(jmarantz): enforce this with a check.
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
  OutputResource* CreateUrlOutputResource(const ResourceNamer& resource_name,
                                          const ContentType* type);

  // Creates an input resource with the url evaluated based on input_url
  // which may need to be absolutified relative to base_url.
  Resource* CreateInputResource(const StringPiece& base_url,
                                const StringPiece& input_url,
                                MessageHandler* handler);

  Resource* CreateInputResourceAbsolute(const StringPiece& absolute_url,
                                        MessageHandler* handler);

  // Set up a basic header for a given content_type.
  // If content_type is null, the Content-Type is omitted.
  // This method may only be called once on a header.
  void SetDefaultHeaders(const ContentType* content_type,
                         MetaData* header) const;

  // Changes the content type of a pre-initialized header.
  void SetContentType(const ContentType* content_type, MetaData* header);

  StringPiece filename_prefix() const { return file_prefix_; }

  // Sets the URL prefix pattern.  The pattern must have exactly one %d
  // in it, if num_shards is not 0.  If num shards is 0, then it should
  // not have any % characters in it.
  void SetUrlPrefixPattern(const StringPiece& url_prefix_pattern);

  void set_filename_prefix(const StringPiece& file_prefix);
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
  // The Resource remains owned by the caller.
  bool ReadIfCached(Resource* resource, MessageHandler* message_handler) const;

  // Read contents of resource asynchronously, calling callback when
  // done.  If the resource contents is cached, the callback will
  // be called directly, rather than asynchronously.  The Resource
  // will be passed to the callback, which will be responsible for
  // ultimately freeing the resource.  The Resource will have its
  // contents and headers filled in.
  //
  // The resource can be deleted only after the callback is called.
  void ReadAsync(Resource* resource, Resource::AsyncCallback* callback,
                 MessageHandler* message_handler);

  // TODO(jmarantz): check thread safety in Apache.
  Hasher* hasher() { return hasher_; }
  FileSystem* file_system() { return file_system_; }
  FilenameEncoder* filename_encoder() { return filename_encoder_; }
  UrlAsyncFetcher* url_async_fetcher() { return url_async_fetcher_; }
  Timer* timer() { return http_cache_->timer(); }
  HTTPCache* http_cache() { return http_cache_; }
  UrlEscaper* url_escaper() { return url_escaper_.get(); }
  int num_shards() const { return num_shards_; }

  // Generates a URL for a name, sharding based on num shards and a hash
  // of the name.
  std::string GenerateUrl(const StringPiece& key) const;

  // Splits the shard number from a possibly sharded URL, based on the
  // current prefix.  If successful, returns the resource -- the tail
  // of the URL following the prefix, otherwise returns NULL.  If sharding
  // is enabled, the shard number is returned in *shard, otherwise,
  // *shard is set to kNotSharded.
  const char* SplitUrl(const char* url, int* shard) const;

  // Whether or not resources should hit the filesystem.
  bool store_outputs_in_file_system() { return store_outputs_in_file_system_; }
  void set_store_outputs_in_file_system(bool store) {
    store_outputs_in_file_system_ = store;
  }
 private:
  Resource* CreateInputResourceGURL(const GURL& url, MessageHandler* handler);
  std::string ConstructNameKey(const OutputResource* output) const;
  void ValidateShardsAgainstUrlPrefixPattern();

  std::string file_prefix_;
  std::string url_prefix_pattern_;
  const int num_shards_;
  int resource_id_;  // Sequential ids for temporary Resource filenames.
  FileSystem* file_system_;
  FilenameEncoder* filename_encoder_;
  UrlAsyncFetcher* url_async_fetcher_;
  Hasher* hasher_;
  Statistics* statistics_;
  HTTPCache* http_cache_;
  scoped_ptr<UrlEscaper> url_escaper_;
  bool relative_path_;
  bool store_outputs_in_file_system_;
  DISALLOW_COPY_AND_ASSIGN(ResourceManager);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_H_
