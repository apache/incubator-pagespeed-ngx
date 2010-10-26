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

#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"

namespace {

const char kResourceUrl[] = "styles/style.css?appearance=reader";
const char kCdnPrefix[] = "http://graphics8.nytimes.com/";
const char kRequestDomain[] = "http://www.nytimes.com";
const char kRequestDomainPort[] = "http://www.nytimes.com:8080";

}  // namespace

namespace net_instaweb {

class DomainLawyerTest : public testing::Test {
 protected:
  DomainLawyerTest()
      : orig_request_("http://www.nytimes.com/index.html"),
        port_request_("http://www.nytimes.com:8080/index.html"),
        https_request_("https://www.nytimes.com/index.html") {
  }
  GURL orig_request_;
  GURL port_request_;
  GURL https_request_;
  DomainLawyer domain_lawyer_;
  GoogleMessageHandler message_handler_;
};

TEST_F(DomainLawyerTest, RelativeDomain) {
  std::string mapped_domain_name;
  ASSERT_TRUE(domain_lawyer_.MapRequestToDomain(
      orig_request_, kResourceUrl, &mapped_domain_name, &message_handler_));
  EXPECT_EQ(kRequestDomain, mapped_domain_name);
}

TEST_F(DomainLawyerTest, AbsoluteDomain) {
  std::string mapped_domain_name;
  ASSERT_TRUE(domain_lawyer_.MapRequestToDomain(
      orig_request_, StrCat(kRequestDomain, "/", kResourceUrl),
      &mapped_domain_name, &message_handler_));
  EXPECT_EQ(kRequestDomain, mapped_domain_name);
}

TEST_F(DomainLawyerTest, ExternalDomainNotDeclared) {
  std::string mapped_domain_name;
  EXPECT_FALSE(domain_lawyer_.MapRequestToDomain(
      orig_request_, StrCat(kCdnPrefix, kResourceUrl), &mapped_domain_name,
      &message_handler_));
}

TEST_F(DomainLawyerTest, ExternalDomainDeclared) {
  StringPiece cdn_domain(kCdnPrefix, sizeof(kCdnPrefix) - 2);
  ASSERT_TRUE(domain_lawyer_.AddDomain(cdn_domain, &message_handler_));
  std::string mapped_domain_name;
  ASSERT_TRUE(domain_lawyer_.MapRequestToDomain(
      orig_request_, StrCat(kCdnPrefix, "/", kResourceUrl), &mapped_domain_name,
      &message_handler_));
  EXPECT_EQ(cdn_domain, mapped_domain_name);
}

TEST_F(DomainLawyerTest, WildcardDomainDeclared) {
  StringPiece cdn_domain(kCdnPrefix, sizeof(kCdnPrefix) - 2);
  ASSERT_TRUE(domain_lawyer_.AddDomain("*.nytimes.com", &message_handler_));
  std::string mapped_domain_name;
  ASSERT_TRUE(domain_lawyer_.MapRequestToDomain(
      orig_request_, StrCat(kCdnPrefix, kResourceUrl), &mapped_domain_name,
      &message_handler_));
  EXPECT_EQ(cdn_domain, mapped_domain_name);
}

TEST_F(DomainLawyerTest, RelativeDomainPort) {
  std::string mapped_domain_name;
  ASSERT_TRUE(domain_lawyer_.MapRequestToDomain(
      port_request_, kResourceUrl, &mapped_domain_name, &message_handler_));
  EXPECT_EQ(kRequestDomainPort, mapped_domain_name);
}

TEST_F(DomainLawyerTest, AbsoluteDomainPort) {
  std::string mapped_domain_name;
  ASSERT_TRUE(domain_lawyer_.MapRequestToDomain(
      port_request_, StrCat(kRequestDomainPort, "/", kResourceUrl),
      &mapped_domain_name, &message_handler_));
  EXPECT_EQ(kRequestDomainPort, mapped_domain_name);
}

TEST_F(DomainLawyerTest, PortExternalDomainNotDeclared) {
  std::string mapped_domain_name;
  EXPECT_FALSE(domain_lawyer_.MapRequestToDomain(
      port_request_, StrCat(kCdnPrefix, kResourceUrl), &mapped_domain_name,
      &message_handler_));
}

TEST_F(DomainLawyerTest, PortExternalDomainDeclared) {
  std::string port_cdn_domain(kCdnPrefix, sizeof(kCdnPrefix) - 2);
  port_cdn_domain += ":8080";
  ASSERT_TRUE(domain_lawyer_.AddDomain(port_cdn_domain, &message_handler_));
  std::string mapped_domain_name;
  ASSERT_TRUE(domain_lawyer_.MapRequestToDomain(
      port_request_, StrCat(port_cdn_domain, "/", kResourceUrl),
      &mapped_domain_name, &message_handler_));
  EXPECT_EQ(port_cdn_domain, mapped_domain_name);
}

TEST_F(DomainLawyerTest, PortWildcardDomainDeclared) {
  std::string port_cdn_domain(kCdnPrefix, sizeof(kCdnPrefix) - 2);
  port_cdn_domain += ":8080";
  ASSERT_TRUE(domain_lawyer_.AddDomain("*.nytimes.com:*", &message_handler_));
  std::string mapped_domain_name;
  ASSERT_TRUE(domain_lawyer_.MapRequestToDomain(
      port_request_, StrCat(port_cdn_domain, "/", kResourceUrl),
      &mapped_domain_name, &message_handler_));
  EXPECT_EQ(port_cdn_domain, mapped_domain_name);
}

TEST_F(DomainLawyerTest, ResourceFromHttpsPage) {
  ASSERT_TRUE(domain_lawyer_.AddDomain("www.nytimes.com", &message_handler_));
  std::string mapped_domain_name;

  // When a relative resource is requested from an https page we will fail.
  ASSERT_FALSE(domain_lawyer_.MapRequestToDomain(
      https_request_, kResourceUrl,
      &mapped_domain_name, &message_handler_));
  ASSERT_TRUE(domain_lawyer_.MapRequestToDomain(
      https_request_, StrCat(kRequestDomain, "/", kResourceUrl),
      &mapped_domain_name, &message_handler_));
}

TEST_F(DomainLawyerTest, AddDomainRedundantly) {
  ASSERT_TRUE(domain_lawyer_.AddDomain("www.nytimes.com", &message_handler_));
  ASSERT_FALSE(domain_lawyer_.AddDomain("www.nytimes.com", &message_handler_));
  ASSERT_TRUE(domain_lawyer_.AddDomain("*", &message_handler_));
  ASSERT_FALSE(domain_lawyer_.AddDomain("*", &message_handler_));
}

}  // namespace net_instaweb
