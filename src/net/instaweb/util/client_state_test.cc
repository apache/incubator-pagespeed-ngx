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

// Author: mdw@google.com (Matt Welsh)

#include <cstddef>                     // for size_t

#include "base/scoped_ptr.h"
#include "net/instaweb/util/client_state.pb.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/client_state.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class AbstractMutex;

namespace {

const size_t kMaxCacheSizeBytes = 20000;

static const char kLongUrl[] =
    "http://metrics.apple.com/b/ss/appleglobal,applehome/1/H.22.1/s5764156"
    "9965053?AQB=1&ndh=1&t=26%2F0%2F2012%209%3A31%3A37%204%20480&pageName="
    "apple%20-%20index%2Ftab%20(us)&g=http%3A%2F%2Fwww.apple.com%2F&cc=USD"
    "&ch=www.us.homepage&server=new%20approach&h1=www.us.homepage&c4=D%3Dg";

}  // namespace

// Tests for ClientState.
class ClientStateTest : public testing::Test {
 protected:
  ClientStateTest()
      : timer_(MockTimer::kApr_5_2010_ms),
        lru_cache_(kMaxCacheSizeBytes),
        thread_system_(ThreadSystem::CreateThreadSystem()),
        property_cache_("test/", &lru_cache_, &timer_, thread_system_.get()) {
    cohort_ = property_cache_.AddCohort(ClientState::kClientStateCohort);
  }

  MockTimer timer_;
  ClientState client_state_;
  LRUCache lru_cache_;
  scoped_ptr<ThreadSystem> thread_system_;
  PropertyCache property_cache_;
  const PropertyCache::Cohort* cohort_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientStateTest);
};

class MockPage : public PropertyPage {
 public:
  explicit MockPage(AbstractMutex* mutex)
      : PropertyPage(mutex),
        called_(false),
        valid_(false) {}
  virtual ~MockPage() {}
  virtual void Done(bool valid) {
    called_ = true;
    valid_ = valid;
  }
  bool called() const { return called_; }
  bool valid() const { return valid_; }

 private:
  bool called_;
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(MockPage);
};

TEST_F(ClientStateTest, TestBasicOperations) {
  // Test basic Set, InCache, and Clear operations.
  EXPECT_FALSE(client_state_.InCache("http://anyurl.com"));

  client_state_.Set("http://someurl.com", 0);
  EXPECT_FALSE(client_state_.InCache("http://someurl.com"));

  client_state_.Set("http://someurl.com",
                    ClientState::kClientStateExpiryTimeThresholdMs);
  EXPECT_TRUE(client_state_.InCache("http://someurl.com"));

  // Test a long hairy URL.
  client_state_.Set(kLongUrl,
                    ClientState::kClientStateExpiryTimeThresholdMs);
  EXPECT_TRUE(client_state_.InCache("http://someurl.com"));
  EXPECT_TRUE(client_state_.InCache(kLongUrl));

  client_state_.Clear();
  EXPECT_FALSE(client_state_.InCache("http://someurl.com"));
  EXPECT_FALSE(client_state_.InCache(kLongUrl));
}

TEST_F(ClientStateTest, PackUnpackWorks) {
  // Test that Pack and Unpack operations work, with two cases:
  // (1) Pack serializes the cache state correctly.
  // (2) Unpack deserializes a protobuf correctly.

  client_state_.Set("http://someurl.com",
                    ClientState::kClientStateExpiryTimeThresholdMs);
  client_state_.client_id_ = "fakeclient_id";
  client_state_.create_time_ms_ = MockTimer::kApr_5_2010_ms;
  ClientStateMsg proto;
  client_state_.Pack(&proto);
  EXPECT_EQ(MockTimer::kApr_5_2010_ms, proto.create_time_ms());
  EXPECT_TRUE(proto.has_client_id());
  EXPECT_EQ("fakeclient_id", proto.client_id());

  ClientState new_clientstate;
  new_clientstate.Unpack(proto);
  EXPECT_EQ("fakeclient_id", new_clientstate.ClientId());
  EXPECT_TRUE(new_clientstate.InCache("http://someurl.com"));
}

TEST_F(ClientStateTest, PropertyCacheWorks) {
  // Test that property cache operations work as expected.
  GoogleString client_id1 = "fakeclient_id";
  client_state_.client_id_ = client_id1;

  // Prime the PropertyCache with an initial read.
  scoped_ptr<MockPage> page1(new MockPage(thread_system_->NewMutex()));
  property_cache_.Read(client_id1, page1.get());
  PropertyValue* property = page1.get()->GetProperty(
      cohort_, ClientState::kClientStatePropertyValue);
  EXPECT_FALSE(property->has_value());

  // Manually write the ClientState to the PropertyCache.
  ClientStateMsg proto;
  client_state_.Pack(&proto);
  GoogleString bytes;
  EXPECT_TRUE(proto.SerializeToString(&bytes));
  property_cache_.UpdateValue(bytes, property);
  property_cache_.WriteCohort(client_id1, cohort_, page1.get());
  EXPECT_TRUE(property->has_value());

  // Read it back and test that we got the right thing.
  MockPage* page2 = new MockPage(thread_system_->NewMutex());
  property_cache_.Read(client_id1, page2);
  ClientState new_clientstate;
  new_clientstate.InitFromPropertyCache(
      client_id1, &property_cache_, page2, &timer_);
  EXPECT_EQ(client_id1, new_clientstate.ClientId());

  // Now test that UnpackFromPropertyCache returns a fresh ClientState
  // when the pcache read fails. Still need to prime the PropertyCache with
  // an initial read.
  GoogleString client_id2 = "client_id2";
  MockPage* page4 = new MockPage(thread_system_->NewMutex());
  property_cache_.Read(client_id2, page4);
  property = page4->GetProperty(
      cohort_, ClientState::kClientStatePropertyValue);
  EXPECT_FALSE(property->has_value());

  ClientState new_clientstate2;
  new_clientstate2.InitFromPropertyCache(
      client_id2, &property_cache_, page4, &timer_);
  EXPECT_EQ(client_id2, new_clientstate2.ClientId());
}

}  // namespace net_instaweb
