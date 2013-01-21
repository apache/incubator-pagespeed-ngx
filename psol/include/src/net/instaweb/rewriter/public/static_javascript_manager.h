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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_STATIC_JAVASCRIPT_MANAGER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_STATIC_JAVASCRIPT_MANAGER_H_

#include <map>
#include <utility>
#include <vector>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class Hasher;
class HtmlElement;
class MessageHandler;
class RewriteDriver;
class RewriteOptions;
class UrlNamer;

// Composes URLs for the javascript files injected by the various PSA filters.
// TODO(ksimbili): Refactor out the common base class to serve the static files
// of type css, images or html etc.
class StaticJavascriptManager {
 public:
  static const char kGStaticBase[];
  static const char kDefaultLibraryUrlPrefix[];
  static const char kBlinkGstaticSuffix[];
  static const char kDeferJsGstaticSuffix[];
  static const char kBlinkJsFileName[];
  static const char kDeferJsFileName[];
  static const char kDeferJsDebugFileName[];
  static const char kJsExtension[];

  enum JsModule {
    kAddInstrumentationJs,
    kClientDomainRewriter,
    kDeferIframe,
    kDeferJs,
    kDelayImagesJs,
    kDelayImagesInlineJs,
    kDeterministicJs,
    kLazyloadImagesJs,
    kDetectReflowJs,
    kLocalStorageCacheJs,
    kEndOfModules,  // Keep this as the last enum value.
  };

  StaticJavascriptManager(UrlNamer* url_namer, Hasher* hasher,
                          MessageHandler* message_handler);

  ~StaticJavascriptManager();

  // Returns the blink js url based on the value of debug filter and the value
  // of serve_js_from_gstatic flag.
  const GoogleString& GetBlinkJsUrl(const RewriteOptions* options) const;

  // Returns the defer js url based on the value of debug filter and the value
  // of serve_js_from_gstatic flag.
  const GoogleString& GetDeferJsUrl(const RewriteOptions* options) const;

  const char* GetJsSnippet(JsModule module,
                           const RewriteOptions* options);

  // Get the Js snippet to be served as external file for the file names
  // file_name. The snippet is returned as 'content' and cache-control headers
  // is set into cache_header. If the hash matches, then ttl is set to 1 year,
  // or else set to 'private max-age=300'.
  // Returns true iff the content for filename is found.
  bool GetJsSnippet(StringPiece file_name, StringPiece* content,
                    StringPiece* cache_header);

  // Add a CharacterNode to an already created script element, properly escaping
  // the text with CDATA tags is necessary. The script element should be added
  // already, say with a call to InsertElementBeforeElement.
  void AddJsToElement(StringPiece js, HtmlElement* script,
                      RewriteDriver* driver);


  // Set the GStatic blink js hash.
  void set_gstatic_blink_hash(const GoogleString& hash);

  // Set the GStatic defer js hash.
  void set_gstatic_defer_js_hash(const GoogleString& hash);

  // Set serve_js_from_gstatic_ to serve the files from gstatic.
  void set_serve_js_from_gstatic(bool serve_js_from_gstatic) {
    serve_js_from_gstatic_ = serve_js_from_gstatic;
  }

  // Set the url prefix for outlining js.
  void set_library_url_prefix(const StringPiece& url_prefix) {
    url_prefix.CopyToString(&library_url_prefix_);
    InitBlink();
    InitDeferJs();
  }

 private:
  // The ownership of following StringPeice is with the object that creates the
  // pair. In this case it lies with file_name_to_js_map_ member variable.
  typedef std::pair<StringPiece, GoogleString> JsSnippetHashPair;
  typedef std::map<GoogleString, JsSnippetHashPair> FileNameToStringsMap;
  typedef std::vector<const char*> StaticJsVector;
  // Uses enum JsModule as the key.
  StaticJsVector opt_js_vector_;
  StaticJsVector debug_js_vector_;

  void InitializeFileNameToJsStringMap();

  // Composes the URL for blink javascript.
  void InitBlink();

  // Composes the URL for deferjs javascript.
  void InitDeferJs();

  void InitializeJsStrings();

  // Set in the constructor, this class does not own the following objects.
  UrlNamer* url_namer_;
  Hasher* hasher_;
  MessageHandler* message_handler_;

  bool serve_js_from_gstatic_;
  GoogleString blink_javascript_gstatic_url_;
  GoogleString blink_javascript_handler_url_;
  GoogleString defer_javascript_url_;
  GoogleString defer_javascript_debug_url_;
  GoogleString library_url_prefix_;
  GoogleString cache_header_with_long_ttl_;
  GoogleString cache_header_with_private_ttl_;
  FileNameToStringsMap file_name_to_js_map_;

  DISALLOW_COPY_AND_ASSIGN(StaticJavascriptManager);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_STATIC_JAVASCRIPT_MANAGER_H_
