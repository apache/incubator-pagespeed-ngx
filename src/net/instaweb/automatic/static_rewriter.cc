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

#include <cstdio>

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/http/public/wget_url_fetcher.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_gflags.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/google_timer.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

class CacheInterface;
class Hasher;
class MessageHandler;
class Statistics;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;

}  // namespace net_instaweb

// The purpose of this class is to help us test that pagespeed_automatic.a
// contains all that's needed to successfully link a rewriter using standard
// g++, without using the gyp flow.
//
// TODO(jmarantz): fill out enough functionality so that this will be
// a functional static rewriter that could optimize an HTML file
// passed as a command-line parameter or via stdin.
class FileRewriter : public net_instaweb::RewriteDriverFactory {
 public:
  explicit FileRewriter(const net_instaweb::RewriteGflags* gflags)
      : gflags_(gflags) {
    net_instaweb::RewriteDriverFactory::Initialize(&simple_stats_);
    SetStatistics(&simple_stats_);
  }

  virtual net_instaweb::Hasher* NewHasher() {
    return new net_instaweb::MD5Hasher;
  }
  virtual net_instaweb::UrlFetcher* DefaultUrlFetcher() {
    return new net_instaweb::WgetUrlFetcher;
  }
  virtual net_instaweb::UrlAsyncFetcher* DefaultAsyncUrlFetcher() {
    return new net_instaweb::FakeUrlAsyncFetcher(ComputeUrlFetcher());
  }
  virtual net_instaweb::MessageHandler* DefaultHtmlParseMessageHandler() {
    return new net_instaweb::GoogleMessageHandler;
  }
  virtual net_instaweb::MessageHandler* DefaultMessageHandler() {
    return new net_instaweb::GoogleMessageHandler();
  }
  virtual net_instaweb::FileSystem* DefaultFileSystem() {
    return new net_instaweb::StdioFileSystem;
  }
  virtual net_instaweb::Timer* DefaultTimer() {
    return new net_instaweb::GoogleTimer;
  }
  virtual net_instaweb::CacheInterface* DefaultCacheInterface() {
    return new net_instaweb::LRUCache(gflags_->lru_cache_size_bytes());
  }
  virtual net_instaweb::Statistics* statistics() {
    return &simple_stats_;
  }

 private:
  const net_instaweb::RewriteGflags* gflags_;
  net_instaweb::SimpleStats simple_stats_;
};

int main(int argc, char** argv) {
  net_instaweb::RewriteGflags gflags(argv[0], &argc, &argv);
  FileRewriter file_rewriter(&gflags);

  // Having stripped all the flags, there should be exactly 3
  // arguments remaining:
  //
  //   input_directory:   The directory where the origin web site is stored
  //   output_directory:  The directory where the rewritten web site is written
  //   URL:               The URL of HTML to rewrite.
  if (argc != 4) {
    fprintf(stderr, "Usage: [options] %s input_dir output_dir url.\n", argv[0]);
    fprintf(stderr, "Type '%s --help' to see the options\n", argv[0]);
    return 1;
  }
  const char* input_dir = argv[1];
  const char* output_dir = argv[2];
  const char* html_name = argv[3];

  file_rewriter.set_filename_prefix(output_dir);

  GoogleString url = net_instaweb::StrCat("http://test.com/", html_name);
  GoogleString input_file_path = net_instaweb::StrCat(input_dir, "/",
                                                      html_name);
  GoogleString output_file_path = net_instaweb::StrCat(output_dir, "/",
                                                       html_name);
  GoogleString html_input_buffer, html_output_buffer;
  if (!file_rewriter.file_system()->ReadFile(input_file_path.c_str(),
                                             &html_input_buffer,
                                             file_rewriter.message_handler())) {
    fprintf(stderr, "failed to read file %s\n", input_file_path.c_str());
    return 1;
  }

  net_instaweb::ResourceManager* resource_manager =
      file_rewriter.CreateResourceManager();
  if (!gflags.SetOptions(&file_rewriter, resource_manager->global_options())) {
    return 1;
  }

  // For this simple file transformation utility we always want to perform
  // any optimizations we can, so we wait until everything is done rather
  // than using a deadline, the way a server deployment would.
  resource_manager->set_block_until_completion_in_render(true);

  net_instaweb::RewriteDriver* driver = resource_manager->NewRewriteDriver();

  // Set up a Writer callback to serialize rewritten output to a string buffer.
  // A network Writer can be defined that will stream directly to a network
  // port without extra copying of bytes.
  net_instaweb::StringWriter writer(&html_output_buffer);
  driver->SetWriter(&writer);
  if (!driver->StartParseId(url, input_file_path,
                            net_instaweb::kContentTypeHtml)) {
    fprintf(stderr, "StartParseId failed on url %s\n", url.c_str());
    return 1;
  }

  // Note that here we are sending the entire buffer into the parser
  // in one chunk, but it's also fine to break up the calls to
  // driver->ParseText as data streams in.  It's up to the caller when
  // to call driver->Flush().  If no calls are ever made to
  // driver->Flush(), then no HTML will be serialized until the end of
  // the document is reached, but rewriters tha work over document
  // structure will have the maximum benefit.
  driver->ParseText(html_input_buffer);
  driver->FinishParse();

  if (!file_rewriter.file_system()->WriteFile(
          output_file_path.c_str(), html_output_buffer,
          file_rewriter.message_handler())) {
    fprintf(stderr, "failed to write file %s\n", output_file_path.c_str());
    return 1;
  }

  // TODO(jmarantz): set up a file-based fetcher that will allow us to
  // rewrite resources in HTML files in this demonstration.

  return 0;
}
