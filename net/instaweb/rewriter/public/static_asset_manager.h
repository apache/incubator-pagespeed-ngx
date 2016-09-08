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

// Author: guptaa@google.com (Ashish Gupta)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_STATIC_ASSET_MANAGER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_STATIC_ASSET_MANAGER_H_

#include <map>
#include <vector>
#include <cstddef>                     // for size_t

#include "net/instaweb/rewriter/static_asset_config.pb.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

class Hasher;
class MessageHandler;
class RewriteOptions;

// Composes URLs for the javascript files injected by the various PSA filters.
// TODO(ksimbili): Refactor out the common base class to serve the static files
// of type css, images or html etc.
// TODO(xqyin): Refactor out StaticAssetManager to have shared infrastructure
// used by both RewriteStaticAssetManager and SystemStaticAssetManager. Now the
// JS files in system/ are done directly in AdminSite.
class StaticAssetManager {
 public:
  static const char kGStaticBase[];
  static const char kDefaultLibraryUrlPrefix[];

  enum ConfigurationMode {
    kInitialConfiguration,
    kUpdateConfiguration
  };

  // static_asset_base is path on this host we serve resources from.
  StaticAssetManager(const GoogleString& static_asset_base,
                     ThreadSystem* threads,
                     Hasher* hasher,
                     MessageHandler* message_handler);

  ~StaticAssetManager();

  // Determines whether the specified index is a valid asset enum.
  bool IsValidIndex(size_t i) const {
    lock_->Lock();
    bool valid = (i <= assets_.size()) && (assets_[i]->file_name != nullptr);
    lock_->Unlock();
    return valid;
  }

  // Returns the url based on the value of debug filter and the value of
  // serve_asset_from_gstatic flag.
  const GoogleString& GetAssetUrl(StaticAssetEnum::StaticAsset module,
                                  const RewriteOptions* options) const;

  // Returns the contents of the asset.
  const char* GetAsset(StaticAssetEnum::StaticAsset module,
                       const RewriteOptions* options) const;

  // Get the asset to be served as external file for the file names file_name.
  // The snippet is returned as 'content' and cache-control headers is set into
  // cache_header. If the hash matches, then ttl is set to 1 year, or else set
  // to 'private max-age=300'.
  // Returns true iff the content for filename is found.
  bool GetAsset(StringPiece file_name, StringPiece* content,
                ContentType* content_type, StringPiece* cache_header) const;

  // If serve_assets_from_gstatic_ is true, update the URL for module to use
  // gstatic. This sets both debug and release versions, and is meant to be
  // used to simplify tests.
  void SetGStaticHashForTest(StaticAssetEnum::StaticAsset module,
                             const GoogleString& hash);

  // Sets serve_assets_from_gstatic_ to true, enabling serving of files from
  // gstatic, and configures the base URL. Note that files won't actually get
  // served from gstatic until you also configure the particular assets this
  // should apply to via SetGStaticHashForTest or ApplyGStaticConfiguration.
  void ServeAssetsFromGStatic(StringPiece gstatic_base) {
    ScopedMutex write_lock(lock_.get());
    serve_assets_from_gstatic_ = true;
    gstatic_base.CopyToString(&gstatic_base_);
  }

  void DoNotServeAssetsFromGStatic() {
    ScopedMutex write_lock(lock_.get());
    serve_assets_from_gstatic_ = false;
    gstatic_base_.clear();
  }

  // If serve_assets_from_gstatic_ is true, uses information in config to
  // set up serving urls.
  // mode == kInitialConfiguration will always overwrite settings.
  // mode == kUpdateConfiguration will only update those which have a matching
  // value of release_label, and expect a previous call with
  // kInitialConfiguration.
  //
  // Note that the computed config is always based on the last call with
  // update mode applied on top of the initial config; multiple calls of
  // update are not concatenated together.
  void ApplyGStaticConfiguration(const StaticAssetConfig& config,
                                 ConfigurationMode mode);

  // If serve_assets_from_gstatic_ is true, reset configuration to what was last
  // set by ApplyGStaticConfiguration with mod == kInitialConfiguration.
  // Precondition: ApplyGStaticConfiguration(kInitialConfiguration) must have
  // been called.
  void ResetGStaticConfiguration();

  // Set the prefix for the URLs of assets.
  void set_library_url_prefix(const StringPiece& url_prefix) {
    ScopedMutex write_lock(lock_.get());
    url_prefix.CopyToString(&library_url_prefix_);
    InitializeAssetUrls();
  }

  void set_static_asset_base(const StringPiece& x) {
    ScopedMutex write_lock(lock_.get());
    x.CopyToString(&static_asset_base_);
    InitializeAssetUrls();
  }

 private:
  // TODO(jud): Refactor this struct so that each static type served
  // (js, images, etc.) has it's own implementation.
  struct Asset {
    const char* file_name;
    GoogleString js_optimized;
    GoogleString js_debug;
    GoogleString js_opt_hash;
    GoogleString js_debug_hash;
    GoogleString opt_url;
    GoogleString debug_url;
    GoogleString release_label;
    ContentType content_type;
  };

  typedef std::map<GoogleString, StaticAssetEnum::StaticAsset>
      FileNameToModuleMap;

  void InitializeAssetStrings();
  void InitializeAssetUrls() EXCLUSIVE_LOCKS_REQUIRED(lock_);


  // Backend for ApplyGStaticConfiguration and ResetGStaticConfiguration;
  // the 'config' parameter is the appropriate composition of initial
  // plus config.
  void ApplyGStaticConfigurationImpl(const StaticAssetConfig& config,
                                     ConfigurationMode mode)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  GoogleString static_asset_base_;
  // Set in the constructor, this class does not own the following objects.
  Hasher* hasher_;
  MessageHandler* message_handler_;

  scoped_ptr<ThreadSystem::RWLock> lock_;
  std::vector<Asset*> assets_ GUARDED_BY(lock_);
  FileNameToModuleMap file_name_to_module_map_ GUARDED_BY(lock_);

  bool serve_assets_from_gstatic_ GUARDED_BY(lock_);
  GoogleString gstatic_base_ GUARDED_BY(lock_);
  scoped_ptr<StaticAssetConfig> initial_gstatic_config_ GUARDED_BY(lock_);
  GoogleString library_url_prefix_ GUARDED_BY(lock_);
  GoogleString cache_header_with_long_ttl_ GUARDED_BY(lock_);
  GoogleString cache_header_with_private_ttl_ GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(StaticAssetManager);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_STATIC_ASSET_MANAGER_H_
