/*
 * Copyright 2011 Google Inc.
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

#ifndef NET_INSTAWEB_AUTOMATIC_PUBLIC_STATIC_REWRITER_H_
#define NET_INSTAWEB_AUTOMATIC_PUBLIC_STATIC_REWRITER_H_

#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_gflags.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CacheInterface;
class FileSystem;
class Hasher;
class MessageHandler;
class ResourceManager;
class Statistics;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;
class Writer;

// Implements a baseline RewriteDriverFactory with the simplest possible
// options for cache, fetchers, & system interface.
//
// TODO(jmarantz): fill out enough functionality so that this will be
// a functional static rewriter that could optimize an HTML file
// passed as a command-line parameter or via stdin.
class FileRewriter : public net_instaweb::RewriteDriverFactory {
 public:
  FileRewriter(const net_instaweb::RewriteGflags* gflags,
               bool echo_errors_to_stdout);
  virtual ~FileRewriter();
  virtual net_instaweb::Hasher* NewHasher();
  virtual net_instaweb::UrlFetcher* DefaultUrlFetcher();
  virtual net_instaweb::UrlAsyncFetcher* DefaultAsyncUrlFetcher();
  virtual net_instaweb::MessageHandler* DefaultHtmlParseMessageHandler();
  virtual net_instaweb::MessageHandler* DefaultMessageHandler();
  virtual net_instaweb::FileSystem* DefaultFileSystem();
  virtual net_instaweb::Timer* DefaultTimer();
  virtual net_instaweb::CacheInterface* DefaultCacheInterface();
  virtual net_instaweb::Statistics* statistics();

 private:
  const net_instaweb::RewriteGflags* gflags_;
  net_instaweb::SimpleStats simple_stats_;
  bool echo_errors_to_stdout_;

  DISALLOW_COPY_AND_ASSIGN(FileRewriter);
};

// Encapsulates the instantiation of a FileRewriter & a simple one-shot
// interface to rewrite some HTML text.
class StaticRewriter {
 public:
  StaticRewriter(int* argc, char*** argv);
  StaticRewriter();
  ~StaticRewriter();

  bool ParseText(const StringPiece& text,
                 const StringPiece& url,
                 const StringPiece& id,
                 const StringPiece& output_dir,
                 Writer* writer);

  FileSystem* file_system();
  MessageHandler* message_handler();

 private:
  RewriteGflags gflags_;
  FileRewriter file_rewriter_;
  ResourceManager* resource_manager_;

  DISALLOW_COPY_AND_ASSIGN(StaticRewriter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_AUTOMATIC_PUBLIC_STATIC_REWRITER_H_
