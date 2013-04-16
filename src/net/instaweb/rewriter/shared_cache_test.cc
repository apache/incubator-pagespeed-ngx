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

// Author: matterbury@google.com (Matt Atterbury)

// Unit-test the interaction of shared cache (e.g. memcached) & LoadFromFile.

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"

#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/rewrite_context_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/delay_cache.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class CacheInterface;

// Reproduce MPS issue 488 emulating memcached with LRUCache. We use the
// 2 server contexts to emulate different servers, each with their own
// filesystem and with a shared http/metadata cache.
class SharedCacheTest : public RewriteContextTestBase {
 protected:
  virtual void SetUp() {
    RewriteContextTestBase::SetUp();

    options()->file_load_policy()->Associate(kTestDomain, "/test/");
    other_options()->file_load_policy()->Associate(kTestDomain, "/test/");
    InitTrimFilters(kRewrittenResource);

    server1_ = server_context();
    server2_ = other_server_context();
    filesystem1_ = server1_->file_system();
    filesystem2_ = server2_->file_system();

    EXPECT_EQ(NULL, server1_->filesystem_metadata_cache());
    EXPECT_EQ(NULL, server2_->filesystem_metadata_cache());

    kRewrittenHref = Encode(kTestDomain, "tw", "0", kOriginalHref, "css");

    // Make the metadata and HTTP caches the same for this test.
    factory1_ = factory();
    factory2_ = other_factory();
    server2_->set_http_cache(new HTTPCache(factory1_->delay_cache(),
                                          factory1_->timer(),
                                          factory1_->hasher(),
                                          factory1_->statistics()));
    server2_->set_metadata_cache(factory1_->delay_cache());

    // The metadata cache and the HTTP cache share an underlying LRU cache at
    // the bottom, so the stats for them are combined into this:
    shared_cache_ = lru_cache();

    // Set up each file system with the same file, same mtime.
    EXPECT_TRUE(filesystem1_->WriteFile(kFilename, kContents,
                                       message_handler()));
    EXPECT_TRUE(filesystem2_->WriteFile(kFilename, kContents,
                                       message_handler()));
    int64 mtime1, mtime2;
    EXPECT_TRUE(filesystem1_->Mtime(kFilename, &mtime1, message_handler()));
    EXPECT_TRUE(filesystem2_->Mtime(kFilename, &mtime2, message_handler()));
    EXPECT_EQ(mtime1, mtime2);

    // Initialize all the counts.
    metadata_num_hits_ = 0;
    metadata_num_misses_ = 0;
    shared_num_hits_ = 0;
    shared_num_misses_ = 0;
    shared_num_inserts_ = 0;
    shared_num_reinserts_ = 0;
    filesystem_num_opens_ = 0;

    validation_ctx_ = rewrite_driver()->request_context();
  }

  void SetUpFilesystemMetadataCaches() {
    // Add a filesystem metadata cache to each server.
    CacheInterface* cache = new LRUCache(10000);
    server1_->set_filesystem_metadata_cache(cache);
    server1_->DeleteCacheOnDestruction(cache);
    cache = new LRUCache(10000);
    server2_->set_filesystem_metadata_cache(cache);
    server1_->DeleteCacheOnDestruction(cache);
  }

  void ValidateRewrite(StringPiece id,
                       int num_new_metadata_hits,
                       int num_new_metadata_misses,
                       int num_new_shared_hits,
                       int num_new_shared_misses,
                       int num_new_shared_inserts,
                       int num_new_shared_reinserts,
                       int num_new_filesystem_opens,
                       StringPiece expected_contents) {
    // Restore request context (FetchResourceUrl sets a new one on
    // rewrite_driver).
    rewrite_driver()->set_request_context(validation_ctx_);
    ValidateExpected(id,
                     CssLinkHref(kOriginalHref),
                     CssLinkHref(kRewrittenHref));

    // Update and check the expected counts.
    EXPECT_EQ(num_new_metadata_hits,
              validation_ctx_->log_record()->logging_info()->
                  metadata_cache_info().num_hits() -
              metadata_num_hits_);
    metadata_num_hits_ = logging_info()->metadata_cache_info().num_hits();

    EXPECT_EQ(num_new_metadata_misses,
              validation_ctx_->log_record()->logging_info()->
                  metadata_cache_info().num_misses() -
              metadata_num_misses_) << id;
    metadata_num_misses_ = logging_info()->metadata_cache_info().num_misses();

    EXPECT_EQ(num_new_shared_hits,
              shared_cache_->num_hits() - shared_num_hits_) << id;
    shared_num_hits_ = shared_cache_->num_hits();

    EXPECT_EQ(num_new_shared_misses,
              shared_cache_->num_misses() - shared_num_misses_) << id;
    shared_num_misses_ = shared_cache_->num_misses();

    EXPECT_EQ(num_new_shared_inserts,
              shared_cache_->num_inserts() - shared_num_inserts_) << id;
    shared_num_inserts_ = shared_cache_->num_inserts();

    EXPECT_EQ(num_new_shared_reinserts,
              shared_cache_->num_identical_reinserts() - shared_num_reinserts_)
        << id;
    shared_num_reinserts_ = shared_cache_->num_identical_reinserts();

    EXPECT_EQ(num_new_filesystem_opens,
              file_system()->num_input_file_opens() - filesystem_num_opens_)
        << id;
    filesystem_num_opens_ = file_system()->num_input_file_opens();

    // Check the rewritten content then check that we got it from the cache.
    GoogleString output;
    EXPECT_TRUE(FetchResourceUrl(kRewrittenHref, &output));
    EXPECT_EQ(expected_contents, output);
    EXPECT_EQ(1, shared_cache_->num_hits() - shared_num_hits_);
    ++shared_num_hits_;
  }

  void SetActiveServer(ActiveServerFlag server_to_use) {
    RewriteTestBase::SetActiveServer(server_to_use);
    // Reset this since we've just changed the file_system() return value.
    filesystem_num_opens_ = file_system()->num_input_file_opens();
  }

  void WriteNewContents(TestRewriteDriverFactory* factory,
                        FileSystem* filesystem,
                        int64 delta_ms) {
    // Advance time for the given factory then write new contents to the
    // test file in the given filesystem.
    factory->AdvanceTimeMs(delta_ms);
    EXPECT_TRUE(filesystem->WriteFile(kFilename, kNewContents,
                                      message_handler()));
    int64 mtime1, mtime2;
    EXPECT_TRUE(filesystem1_->Mtime(kFilename, &mtime1, message_handler()));
    EXPECT_TRUE(filesystem2_->Mtime(kFilename, &mtime2, message_handler()));
    EXPECT_NE(mtime1, mtime2);
  }

 public:
  static const char kFilename[];
  static const char kContents[];
  static const char kNewContents[];
  static const char kTrimmed[];
  static const char kNewTrimmed[];
  static const char kOriginalHref[];
  GoogleString kRewrittenHref;

  ServerContext* server1_;
  ServerContext* server2_;
  FileSystem* filesystem1_;
  FileSystem* filesystem2_;
  TestRewriteDriverFactory* factory1_;
  TestRewriteDriverFactory* factory2_;
  LRUCache* shared_cache_;

  RequestContextPtr validation_ctx_;

  int metadata_num_hits_;
  int metadata_num_misses_;
  int shared_num_hits_;
  int shared_num_misses_;
  int shared_num_inserts_;
  int shared_num_reinserts_;
  int filesystem_num_opens_;
};

const char SharedCacheTest::kFilename[] = "/test/a.css";
const char SharedCacheTest::kContents[] = " foo b ar ";
const char SharedCacheTest::kNewContents[] = " bar fo o ";
const char SharedCacheTest::kTrimmed[]  = "foo b ar";
const char SharedCacheTest::kNewTrimmed[] = "bar fo o";
const char SharedCacheTest::kOriginalHref[]  = "a.css";

TEST_F(SharedCacheTest, LoadFromFileMisbehavesWithoutFilesystemMetadataCache) {
  // With two independent servers, both using load-from-file, both sharing a
  // metadata cache, and neither with a filesystem metadata cache, things
  // sort-of-work, but things misbehave when one server updates its file
  // contents while the other one doesn't.

  // 1. The first rewrite is successful because we read from the filesystem.
  // - The metadata cache gets a miss for the original URL followed later by
  //   an insert of original URL -> rewritten URL + content hash.
  // - The HTTP cache gets a miss then an insert of rewritten URL -> content.
  //   We do NOT store the original URL -> content in there as it's on disk.
  // - We opened the file to read it.
  ValidateRewrite("first_read",
                  /* num_new_metadata_hits = */    0,
                  /* num_new_metadata_misses = */  1,
                  /* num_new_shared_hits = */      0,
                  /* num_new_shared_misses = */    1,  // metadata
                  /* num_new_shared_inserts = */   2,  // metadata + HTTP
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 1,  // HTTP
                  kTrimmed);

  // 2. Rewrite the same HTML but using a different server.
  //    Everything thing we need is in the -shared- cache.
  // - The metadata cache gets a hit for the original URL, which is mapped to
  //   the rewritten URL + the content hash.
  // - The HTTP cache is not read or written because all we need to rewrite the
  //   HTML is the rewritten URL.
  // - No file access required this time. Now see, this is WRONG.
  //   RewriteContext::IsInputValid is checking the metadata cache's timestamp
  //   against server2's timestamp, BUT the metadata cache's timestamp came
  //   from server1, so its value is irrelevant. This is what the filesystem
  //   metadata cache fixes by storing the timestamp in a server private cache.
  SetActiveServer(kSecondary);
  ValidateRewrite("first_cache",
                  /* num_new_metadata_hits = */    1,
                  /* num_new_metadata_misses = */  0,
                  /* num_new_shared_hits = */      1,  // metadata
                  /* num_new_shared_misses = */    0,
                  /* num_new_shared_inserts = */   0,
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 0,
                  kTrimmed);

  // 3. Modify server1's version of the file and fetch it from there again.
  //    We will have to rewrite it again and update the caches.
  // - The metadata cache gets a hit for the original URL since we know about
  //   it, followed later by an upsert to its new rewritten URL + contents hash,
  //   BUT the metadata logging info records a miss because, although a record
  //   was found in the (shared) metadata cache, it isn't valid because of the
  //   change in mtime so a miss is recorded instead.
  // - The HTTP cache gets a miss for the original URL -> content (which we
  //   never cache because it's on disk) followed by an upsert for the rewritten
  //   URL to its new contents (the rewritten URL is unchanged because URL
  //   hashes are always zero in this test program).
  // - And we opened the file again to read it.
  SetActiveServer(kPrimary);
  WriteNewContents(factory1_, filesystem1_, Timer::kSecondMs);  // Update FS #1
  ValidateRewrite("second_read",
                  /* num_new_metadata_hits = */    0,
                  /* num_new_metadata_misses = */  1,
                  /* num_new_shared_hits = */      1,  // metadata
                  /* num_new_shared_misses = */    0,
                  /* num_new_shared_inserts = */   2,  // HTTP + metadata
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 1,
                  kNewTrimmed);                        // NEW content!

  // 4. Rewrite using server2, which has the old contents in its filesystem.
  //    We'll find the resource in the metadata cache but its mtime is wrong so
  //    we can't use the cached value so we have to reread and rewrite again.
  // - The metadata cache gets a hit for the original URL since we know about
  //   it, followed later by an upsert to its new rewritten URL + contents hash,
  //   BUT the metadata logging info records a miss because, although a record
  //   was found in the (shared) metadata cache, it isn't valid because of the
  //   change in mtime so a miss is recorded instead.
  // - The HTTP cache gets a miss for the original URL -> content (which we
  //   never cache because it's on disk) followed by an upsert for the rewritten
  //   URL to its new contents (the rewritten URL is unchanged because URL
  //   hashes are always zero in this test program).
  // - And we had to read the file to get its contents.
  SetActiveServer(kSecondary);
  ValidateRewrite("first_not_cache",
                  /* num_new_metadata_hits = */    0,
                  /* num_new_metadata_misses = */  1,
                  /* num_new_shared_hits = */      1,  // HTTP
                  /* num_new_shared_misses = */    0,
                  /* num_new_shared_inserts = */   2,  // HTTP + metadata
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 1,
                  kTrimmed);                           // OLD content!
}

TEST_F(SharedCacheTest, LoadFromFileSucceedsWithFilesystemMetadataCache) {
  // With 2 independent servers, both using load-from-file, both sharing a
  // metadata cache, and both with a [private] filesystem metadata cache,
  // things really do work correctly.

  SetUpFilesystemMetadataCaches();

  // Same as step 1 of LoadFromFileFailsWithSharedCache above.
  ValidateRewrite("first_read",
                  /* num_new_metadata_hits = */    0,
                  /* num_new_metadata_misses = */  1,
                  /* num_new_shared_hits = */      0,
                  /* num_new_shared_misses = */    1,  // metadata
                  /* num_new_shared_inserts = */   2,  // metadata + HTTP
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 1,  // HTTP
                  kTrimmed);

  // As for step 2 of LoadFromFileFailsWithSharedCache above EXCEPT:
  // - We do file read to compute the filesystem metadata cache's content hash.
  SetActiveServer(kSecondary);
  ValidateRewrite("first_cache",
                  /* num_new_metadata_hits = */    1,
                  /* num_new_metadata_misses = */  0,
                  /* num_new_shared_hits = */      1,  // metadata
                  /* num_new_shared_misses = */    0,
                  /* num_new_shared_inserts = */   0,
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 1,  // FSMDC
                  kTrimmed);

  // As for step 3 of LoadFromFileFailsWithSharedCache above EXCEPT:
  // - We get one file read to compute the filesystem metadata cache's new
  //   content hash, and another file read to re-read-and-rewrite it in the
  //   background.
  SetActiveServer(kPrimary);
  WriteNewContents(factory1_, filesystem1_, Timer::kSecondMs);  // Update FS #1
  ValidateRewrite("first_update",
                  /* num_new_metadata_hits = */    0,
                  /* num_new_metadata_misses = */  1,
                  /* num_new_shared_hits = */      1,  // metadata
                  /* num_new_shared_misses = */    0,
                  /* num_new_shared_inserts = */   2,  // HTTP + metadata
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 2,  // FSMDC + HTTP
                  kNewTrimmed);                        // NEW content!

  // As for step 4 of LoadFromFileFailsWithSharedCache above EXCEPT:
  // - Again we get one file read to compute the filesystem metadata cache's
  //   new content hash, and another file read to re-read-and-rewrite it in the
  //   background.
  SetActiveServer(kSecondary);
  ValidateRewrite("first_not_cache",
                  /* num_new_metadata_hits = */    0,
                  /* num_new_metadata_misses = */  1,
                  /* num_new_shared_hits = */      1,  // HTTP
                  /* num_new_shared_misses = */    0,
                  /* num_new_shared_inserts = */   2,  // HTTP + metadata
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 2,  // FSMDC + HTTP
                  kTrimmed);                           // OLD content!

  // As for steps 3 and 4 above because we will now flip-flop between the two
  // different versions of the file contents.
  SetActiveServer(kPrimary);
  ValidateRewrite("first_flip_flop",
                  /* num_new_metadata_hits = */    0,
                  /* num_new_metadata_misses = */  1,
                  /* num_new_shared_hits = */      1,  // metadata
                  /* num_new_shared_misses = */    0,
                  /* num_new_shared_inserts = */   2,  // HTTP + metadata
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 2,  // FSMDC + HTTP
                  kNewTrimmed);                        // NEW content!
  SetActiveServer(kSecondary);
  ValidateRewrite("second_flip_flop",
                  /* num_new_metadata_hits = */    0,
                  /* num_new_metadata_misses = */  1,
                  /* num_new_shared_hits = */      1,  // metadata
                  /* num_new_shared_misses = */    0,
                  /* num_new_shared_inserts = */   2,  // HTTP + metadata
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 2,  // FSMDC + HTTP
                  kTrimmed);                           // OLD content!
  SetActiveServer(kPrimary);
  ValidateRewrite("third_flip_flop",
                  /* num_new_metadata_hits = */    0,
                  /* num_new_metadata_misses = */  1,
                  /* num_new_shared_hits = */      1,  // metadata
                  /* num_new_shared_misses = */    0,
                  /* num_new_shared_inserts = */   2,  // HTTP + metadata
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 2,  // FSMDC + HTTP
                  kNewTrimmed);                        // NEW content!

  // Now rewrite server2's version of the file to be same as server1's, albeit
  // with a different mtime. We get a metadata hit because the content hashes
  // are now the same, so we can reuse server1's rewritten contents. The end
  // result is that this is exactly the same as our step 2 above.
  SetActiveServer(kSecondary);
  WriteNewContents(factory2_, filesystem2_, 2*Timer::kSecondMs);  // Update FS#2
  ValidateRewrite("second_update",
                  /* num_new_metadata_hits = */    1,
                  /* num_new_metadata_misses = */  0,
                  /* num_new_shared_hits = */      1,  // metadata
                  /* num_new_shared_misses = */    0,
                  /* num_new_shared_inserts = */   0,
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 1,  // FSMDC
                  kNewTrimmed);                        // NEW content!

  // We should stabilize and stop flip-flopping having reloaded from server2.
  SetActiveServer(kPrimary);
  ValidateRewrite("first_stabilize",
                  /* num_new_metadata_hits = */    1,
                  /* num_new_metadata_misses = */  0,
                  /* num_new_shared_hits = */      1,  // metadata
                  /* num_new_shared_misses = */    0,
                  /* num_new_shared_inserts = */   0,
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 0,
                  kNewTrimmed);                        // NEW content!
  SetActiveServer(kSecondary);
  ValidateRewrite("second_stabilize",
                  /* num_new_metadata_hits = */    1,
                  /* num_new_metadata_misses = */  0,
                  /* num_new_shared_hits = */      1,  // metadata
                  /* num_new_shared_misses = */    0,
                  /* num_new_shared_inserts = */   0,
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 0,
                  kNewTrimmed);                        // NEW content!
  SetActiveServer(kPrimary);
  ValidateRewrite("third_stabilize",
                  /* num_new_metadata_hits = */    1,
                  /* num_new_metadata_misses = */  0,
                  /* num_new_shared_hits = */      1,  // metadata
                  /* num_new_shared_misses = */    0,
                  /* num_new_shared_inserts = */   0,
                  /* num_new_shared_reinserts = */ 0,
                  /* num_new_filesystem_opens = */ 0,
                  kNewTrimmed);                        // NEW content!
}

}  // namespace net_instaweb
