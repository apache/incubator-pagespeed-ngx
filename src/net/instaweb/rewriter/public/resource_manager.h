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
#include "net/instaweb/util/public/url_segment_encoder.h"

class GURL;

namespace net_instaweb {

class ContentType;
class DomainLawyer;
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
class Variable;
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
                  HTTPCache* http_cache,
                  DomainLawyer* domain_lawyer);
  ~ResourceManager();

  // Initialize statistics gathering.
  static void Initialize(Statistics* statistics);

  // Created resources are managed by ResourceManager and eventually deleted by
  // ResourceManager's destructor.  Every time a Create...Resource... method is
  // called, a fresh Resource object is generated (or the creation fails and
  // NULL is returned).  All content_type arguments can be NULL if the content
  // type isn't known or isn't covered by the ContentType library.  Where
  // necessary, the extension is used to infer a content type if one is needed
  // and none is provided.  It is faster and more reliable to provide one
  // explicitly when it is known.

  // Constructs an output resource corresponding to the specified input resource
  // and encoded using the provided encoder.  Assumes permissions checking
  // occurred when the input resource was constructed, and does not do it again.
  // To avoid if-chains, tolerates a NULL input_resource (by returning NULL).
  // TODO(jmaessen, jmarantz): Do we want to permit NULL input_resources here?
  // jmarantz has evinced a distaste.
  OutputResource* CreateOutputResourceFromResource(
      const StringPiece& filter_prefix,
      const ContentType* content_type,
      UrlSegmentEncoder* encoder,
      Resource* input_resource,
      MessageHandler* handler);

  // Constructs and permissions-checks an output resource for the specified url,
  // which occurs in the context of document_gurl.  Returns NULL on failure.
  // The content_type argument cannot be NULL.  The resource name will be
  // encoded using the provided encoder.
  OutputResource* CreateOutputResourceForRewrittenUrl(
      const GURL& document_gurl,
      const StringPiece& filter_prefix,
      const StringPiece& resource_url,
      const ContentType* content_type,
      UrlSegmentEncoder* encoder,
      MessageHandler* handler);

  // Creates an output resource where the name is provided by the rewriter.
  // The intent is to be able to derive the content from the name, for example,
  // by encoding URLs and metadata.
  //
  // This method is not dependent on shared persistent storage, and always
  // succeeds.
  //
  // This name is prepended with path for writing hrefs, and the resulting url
  // is encoded and stored at file_prefix when working with the file system.  So
  // hrefs are:
  //    $(PATH)/$(FILTER_PREFIX).$(HASH).$(NAME).$(CONTENT_TYPE_EXT)
  //
  // 'type' arg can be null if it's not known, or is not in our ContentType
  // library.
  OutputResource* CreateOutputResourceWithPath(
      const StringPiece& path, const StringPiece& filter_prefix,
      const StringPiece& name,  const ContentType* type,
      MessageHandler* handler);

  // Creates a resource based on a URL.  This is used for serving rewritten
  // resources.  No permission checks are performed on the url, though it
  // is parsed to see if it looks like the url of a generated resource (which
  // should mean checking the hash to ensure we generated it ourselves).
  // TODO(jmaessen): add url hash & check thereof.
  OutputResource* CreateOutputResourceForFetch(
      const StringPiece& url,
      MessageHandler* handler);

  // Creates an input resource with the url evaluated based on input_url
  // which may need to be absolutified relative to base_url.  Returns NULL if
  // the input resource url isn't valid, or can't legally be rewritten in the
  // context of this page.
  Resource* CreateInputResource(const GURL& base_url,
                                const StringPiece& input_url,
                                MessageHandler* handler);

  // Create input resource from input_url, if it is legal in the context of
  // base_gurl, and if the resource can be read from cache.  If it's not in
  // cache, initiate an asynchronous fetch so it will be on next access.  This
  // is a common case for filters.
  Resource* CreateInputResourceAndReadIfCached(const GURL& base_gurl,
                                               const StringPiece& input_url,
                                               MessageHandler* handler);

  // Create an input resource by decoding output_resource using the given
  // encoder.  Assures legality by checking hash signatures, rather than
  // explicitly permission-checking the result.
  Resource* CreateInputResourceFromOutputResource(
    UrlSegmentEncoder* encoder,
    OutputResource* output_resource,
    MessageHandler* handler);

  // Creates an input resource from the given absolute url.  Requires that the
  // provided url has been checked, and can legally be rewritten in the current
  // page context.  If you have a GURL, prefer CreateInputResourceUnchecked,
  // otherwise use this.
  Resource* CreateInputResourceAbsolute(const StringPiece& absolute_url,
                                        MessageHandler* handler);

  // Creates an input resource with the given gurl, already absolute and valid.
  // Use only for resource fetches that lack a page context, or in places where
  // permission checking has been done explicitly on the caller side (for
  // example css_combine_filter, which constructs its own url_partnership).
  Resource* CreateInputResourceUnchecked(const GURL& gurl,
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
  void set_statistics(Statistics* s) {
    statistics_ = s;
    resource_url_domain_rejections_ = NULL;  // Lazily initialized.
  }
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

  // Load the resource if it is cached (or if it can be fetched quickly).
  // If not send off an asynchronous fetch and store the result in the cache.
  //
  // Returns true if the resource is loaded.
  //
  // The resource remains owned by the caller.
  bool ReadIfCached(Resource* resource, MessageHandler* message_handler) const;

  // Loads contents of resource asynchronously, calling callback when
  // done.  If the resource contents is cached, the callback will
  // be called directly, rather than asynchronously.  The resource
  // will be passed to the callback, which will be responsible for
  // ultimately freeing the resource.  The resource will have its
  // contents and headers filled in.
  //
  // The resource can be deleted only after the callback is called.
  void ReadAsync(Resource* resource, Resource::AsyncCallback* callback,
                 MessageHandler* message_handler);

  // TODO(jmarantz): check thread safety in Apache.
  Hasher* hasher() { return hasher_; }
  // This setter should probably only be used in testing.
  void set_hasher(Hasher* hasher) { hasher_ = hasher; }

  FileSystem* file_system() { return file_system_; }
  FilenameEncoder* filename_encoder() const { return filename_encoder_; }
  UrlAsyncFetcher* url_async_fetcher() { return url_async_fetcher_; }
  Timer* timer() { return http_cache_->timer(); }
  HTTPCache* http_cache() { return http_cache_; }
  UrlEscaper* url_escaper() { return url_escaper_.get(); }
  int num_shards() const { return num_shards_; }

  // Given a ResourceNamer, generates the prefix (everything but the file name)
  // for the corresponding URL.
  std::string UrlPrefixFor(const ResourceNamer& namer) const;

  // Whether or not resources should hit the filesystem.
  bool store_outputs_in_file_system() { return store_outputs_in_file_system_; }
  void set_store_outputs_in_file_system(bool store) {
    store_outputs_in_file_system_ = store;
  }

  DomainLawyer* domain_lawyer() { return domain_lawyer_; }
  const DomainLawyer* domain_lawyer() const { return domain_lawyer_; }

  int max_url_segment_size() const { return max_url_segment_size_; }
  int max_url_size() const { return max_url_size_; }

 private:
  inline void IncrementResourceUrlDomainRejections();
  void ValidateShardsAgainstUrlPrefixPattern();
  std::string CanonicalizeBase(const StringPiece& base, int* shard) const;

  std::string file_prefix_;
  std::string url_prefix_pattern_;
  const int num_shards_;
  int resource_id_;  // Sequential ids for temporary Resource filenames.
  FileSystem* file_system_;
  FilenameEncoder* filename_encoder_;
  UrlAsyncFetcher* url_async_fetcher_;
  Hasher* hasher_;
  Statistics* statistics_;
  Variable* resource_url_domain_rejections_;
  HTTPCache* http_cache_;
  scoped_ptr<UrlEscaper> url_escaper_;
  bool relative_path_;
  bool store_outputs_in_file_system_;
  DomainLawyer* domain_lawyer_;
  int max_url_segment_size_;  // for http://a/b/c.d, this is == strlen("c.d")
  int max_url_size_;          // but this is strlen("http://a/b/c.d")

  DISALLOW_COPY_AND_ASSIGN(ResourceManager);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_H_
