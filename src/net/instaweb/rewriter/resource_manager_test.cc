// Copyright 2010 and onwards Google Inc.
// Author: abliss@google.com (Adam Bliss)

// Unit-test the resource manager

#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/content_type.h"

namespace net_instaweb {

class ResourceManagerTest : public ResourceManagerTestBase {
 protected:
  ResourceManagerTest() { }
};

TEST_F(ResourceManagerTest, TestNamed) {
  scoped_ptr<ResourceManager> resource_manager(
      NewResourceManager(&mock_hasher_));
  const char* filter_prefix = "fp";
  const char* name = "name";
  scoped_ptr<OutputResource> nor(resource_manager->CreateNamedOutputResource(
      filter_prefix, name, &kContentTypeText, &message_handler_));
  EXPECT_TRUE(nor.get() != NULL);
}

}  // namespace net_instaweb
