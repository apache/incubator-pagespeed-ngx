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

#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class ContentType;
class InputResource;
class MessageHandler;
class MetaData;
class OutputResource;
class Statistics;

class ResourceManager {
 public:
  virtual ~ResourceManager();

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
  virtual OutputResource* GenerateOutputResource(
      const StringPiece& filter_prefix, const ContentType& type) = 0;


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
  virtual OutputResource* NamedOutputResource(const StringPiece& filter_prefix,
                                              const StringPiece& name,
                                              const ContentType& type) = 0;

  // Returns the named output resource, if it exists, or returns NULL.
  virtual OutputResource* FindNamedOutputResource(
      const StringPiece& filter_prefix,
      const StringPiece& name,
      const StringPiece& ext) const = 0;

  virtual InputResource* CreateInputResource(const StringPiece& url,
                                             MessageHandler* handler) = 0;

  // Set up a basic header for a given content_type.
  virtual void SetDefaultHeaders(const ContentType& content_type,
                                 MetaData* header) = 0;

  // Call when resources are not needed anymore to free up memory.
  virtual void CleanupResources() = 0;

  virtual StringPiece file_prefix() const = 0;
  virtual StringPiece url_prefix() const = 0;
  virtual std::string base_url() const = 0;

  virtual void set_file_prefix(const StringPiece& file_prefix) = 0;
  virtual void set_url_prefix(const StringPiece& url_prefix) = 0;
  virtual void set_base_url(const StringPiece& url) = 0;
  virtual void set_statistics(Statistics* stats) = 0;
  virtual Statistics* statistics() const = 0;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_MANAGER_H_
