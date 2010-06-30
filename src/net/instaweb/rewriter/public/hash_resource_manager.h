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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_HASH_RESOURCE_MANAGER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_HASH_RESOURCE_MANAGER_H_

#include <map>
#include <vector>
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

class GURL;

namespace net_instaweb {

class ContentType;
class FileSystem;
class FilenameEncoder;
class Hasher;
class HashOutputResource;
class InputResource;
class MetaData;
class OutputResource;
class MessageHandler;
class UrlFetcher;

// Note: the inheritance is mostly to consolidate code.
class HashResourceManager : public ResourceManager {
 public:
  explicit HashResourceManager(const StringPiece& file_prefix,
                               const StringPiece& url_prefix,
                               const int num_shards,
                               FileSystem* file_system,
                               FilenameEncoder* filename_encoder,
                               UrlFetcher* url_fetcher,
                               Hasher* hasher);
  virtual ~HashResourceManager();

  virtual OutputResource* GenerateOutputResource(
      const StringPiece& filter_prefix, const ContentType& type);

  virtual OutputResource* NamedOutputResource(const StringPiece& filter_prefix,
                                              const StringPiece& name,
                                              const ContentType& type);

  virtual OutputResource* FindNamedOutputResource(
      const StringPiece& filter_prefix,
      const StringPiece& name,
      const StringPiece& ext) const;

  virtual InputResource* CreateInputResource(const StringPiece& url,
                                             MessageHandler* handler);

  virtual void SetDefaultHeaders(const ContentType& content_type,
                                 MetaData* header);

  // Call when resources are not needed anymore to delete them and free memory.
  virtual void CleanupResources();

  virtual StringPiece file_prefix() const { return file_prefix_; }
  virtual StringPiece url_prefix() const { return url_prefix_; }
  virtual std::string base_url() const;

  virtual void set_file_prefix(const StringPiece& file_prefix);
  virtual void set_url_prefix(const StringPiece& url_prefix);
  virtual void set_base_url(const StringPiece& url);
  virtual Statistics* statistics() const { return statistics_; }
  virtual void set_statistics(Statistics* s) { statistics_ = s; }

 private:
  inline OutputResource* FindNamedOutputResourceInternal(
      const StringPiece& filter_prefix,
      const StringPiece& name,
      const StringPiece& ext,
      std::string* cache_key) const;

  std::vector<OutputResource*> output_resources_;
  std::vector<InputResource*> input_resources_;

  scoped_ptr<GURL> base_url_;  // Base url to resolve relative urls against.
  std::string file_prefix_;
  std::string url_prefix_;
  int num_shards_;   // NYI: For server sharding of OutputResources.
  int resource_id_;  // Sequential ids for temporary OutputResource filenames.
  FileSystem* file_system_;
  FilenameEncoder* filename_encoder_;
  UrlFetcher* url_fetcher_;
  Hasher* hasher_;
  Statistics* statistics_;

  typedef std::map<std::string, OutputResource*> ResourceMap;
  ResourceMap resource_map_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_HASH_RESOURCE_MANAGER_H_
