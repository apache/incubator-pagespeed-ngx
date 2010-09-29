// Copyright 2010 and onwards Google Inc.
// Author: abliss@google.com (Adam Bliss)

// Unit-test the resource manager

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/resource_manager_testing_peer.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/meta_data.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

class VerifyContentsCallback : public Resource::AsyncCallback {
 public:
  VerifyContentsCallback(const std::string& contents) :
      contents_(contents), called_(false) {
  }
  virtual void Done(bool success, Resource* resource) {
    EXPECT_EQ(0, contents_.compare(resource->contents().as_string()));
    called_ = true;
  }
  void AssertCalled() {
    EXPECT_TRUE(called_);
  }
  std::string contents_;
  bool called_;
};

class ResourceManagerTest : public ResourceManagerTestBase {
 protected:
  ResourceManagerTest() { }

  // Calls FetchOutputResource with different values of writer and
  // response_headers, to test all branches.  Expects the fetch to succeed all
  // times, and finally returns the contents.
  std::string FetchOutputResource(OutputResource* resource) {
    EXPECT_TRUE(resource_manager_->FetchOutputResource(
        resource, NULL, NULL, &message_handler_));
    SimpleMetaData empty;
    EXPECT_TRUE(resource_manager_->FetchOutputResource(
        resource, NULL, &empty, &message_handler_));
    std::string contents;
    StringWriter writer(&contents);
    EXPECT_TRUE(resource_manager_->FetchOutputResource(
        resource, &writer, resource->metadata(),
        &message_handler_));
    return contents;
  }

  // Asserts that the given url starts with an appropriate prefix;
  // then cuts off that prefix.
  virtual void RemoveUrlPrefix(std::string* url) {
    EXPECT_EQ(0, url->compare(0, url_prefix_.length(), url_prefix_));
    url->erase(0, url_prefix_.length());
  }


  // Tests for the lifecycle and various flows of a named output resource.
  void TestNamed() {
    const char* filter_prefix = "fp";
    const char* name = "name";
    const char* contents = "contents";
    const int64 origin_expire_time_ms = 1000;
    const ContentType* content_type = &kContentTypeText;
    // Create a NamedOutputResource and write it.
    scoped_ptr<OutputResource> nor(resource_manager_->CreateNamedOutputResource(
        filter_prefix, name, content_type, &message_handler_));
    ASSERT_TRUE(nor.get() != NULL);
    EXPECT_FALSE(nor->IsWritten());
    EXPECT_FALSE(ResourceManagerTestingPeer::HasHash(nor.get()));
    EXPECT_FALSE(ResourceManagerTestingPeer::Generated(nor.get()));
    EXPECT_TRUE(resource_manager_->Write(HttpStatus::kOK, contents, nor.get(),
                                         origin_expire_time_ms,
                                         &message_handler_));
    EXPECT_TRUE(nor->IsWritten());
    // Retrieve the same NOR from the cache.
    scoped_ptr<OutputResource> nor2(
        resource_manager_->CreateNamedOutputResource(
            filter_prefix, name, &kContentTypeText, &message_handler_));
    ASSERT_TRUE(nor2.get() != NULL);
    EXPECT_TRUE(ResourceManagerTestingPeer::HasHash(nor.get()));
    EXPECT_FALSE(ResourceManagerTestingPeer::Generated(nor.get()));
    EXPECT_TRUE(nor->IsWritten());

    // Fetch its contents and make sure they match
    std::string newContents;
    EXPECT_EQ(contents, FetchOutputResource(nor2.get()));

    // Try asynchronously too
    VerifyContentsCallback callback(contents);
    resource_manager_->ReadAsync(nor2.get(), &callback, &message_handler_);
    callback.AssertCalled();

    // Grab the URL for later
    EXPECT_TRUE(nor2->HasValidUrl());
    std::string url = nor2->url();
    EXPECT_TRUE(url.length() > 0);

    // Now expire it from the HTTP cache.  Since we don't know its hash,
    // we cannot fetch it (even though the contents are still in the filesystem).
    mock_timer_.advance_ms(2 * origin_expire_time_ms);
    scoped_ptr<OutputResource> nor3(
        resource_manager_->CreateNamedOutputResource(
            filter_prefix, name, &kContentTypeText, &message_handler_));
    EXPECT_FALSE(resource_manager_->FetchOutputResource(
        nor3.get(), NULL, NULL, &message_handler_));

    // But with the URL (which contains the hash), we can retrieve it
    // from the http_cache.
    // first cut off the "http://mysite{,.0,.1}/" from the front.
    RemoveUrlPrefix(&url);

    // TODO(abliss): move this splitting logic into ResourceManager
    const char* separator = RewriteFilter::prefix_separator();
    std::vector<StringPiece> components;
    SplitStringPieceToVector(url, separator, &components, false);
    EXPECT_EQ(content_type, NameExtensionToContentType(url));
    ASSERT_TRUE((content_type != NULL) && (components.size() == 4));
    EXPECT_EQ(filter_prefix, components[0]);
    const StringPiece& hash = components[1];
    EXPECT_EQ(name, components[2]);
    // components[3] = "txt", unused
    scoped_ptr<OutputResource> nor4(
        resource_manager_->CreateUrlOutputResource(filter_prefix,
                                                   name, hash, content_type));
    EXPECT_EQ(nor->url(), nor4->url());
    EXPECT_EQ(contents, FetchOutputResource(nor4.get()));

    // If it's evicted from the http_cache, we can also retrieve it from the
    // filesystem.
    lru_cache_->Clear();
    nor4.reset(resource_manager_->CreateUrlOutputResource(
        filter_prefix, name, hash, content_type));
    EXPECT_EQ(nor->url(), nor4->url());
    EXPECT_EQ(contents, FetchOutputResource(nor4.get()));
    // This also works asynchronously.
    lru_cache_->Clear();
    VerifyContentsCallback callback2(contents);
    resource_manager_->ReadAsync(nor4.get(), &callback2, &message_handler_);
    callback2.AssertCalled();

  }
};

class ResourceManagerShardedTest : public ResourceManagerTest {
 protected:
  ResourceManagerShardedTest() {
    url_prefix_ ="http://mysite.%d/";
    num_shards_ = 2;
  }
  virtual void RemoveUrlPrefix(std::string* url) {
    // One of these two should match.
    StringPiece prefix(url->data(), 16);
    EXPECT_TRUE((prefix == "http://mysite.0/") ||
                (prefix == "http://mysite.1/"));
    // "%d" -> "0" (or "1") loses 1 char.
    url->erase(0, url_prefix_.length() - 1);
  }

};

TEST_F(ResourceManagerTest, TestNamed) {
  TestNamed();
}

TEST_F(ResourceManagerShardedTest, TestNamed) {
  TestNamed();
}

}  // namespace net_instaweb
