/*
 * Copyright 2014 Google Inc.
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

// Author: jefftk@google.com (Jeff Kaufman)

#include "net/instaweb/util/public/gtest.h"

#include "net/instaweb/system/public/system_request_context.h"
#include "third_party/domain_registry_provider/src/domain_registry/domain_registry.h"

namespace net_instaweb {

namespace {

TEST(SystemRequestContext, MinimalPrivateSuffix) {
  InitializeDomainRegistry();
  // com is a public suffix, so both google.com and www.google.com should yield
  // google.com
  EXPECT_EQ("google.com",
            SystemRequestContext::MinimalPrivateSuffix("google.com"));
  EXPECT_EQ("google.com",
            SystemRequestContext::MinimalPrivateSuffix("www.google.com"));

  // We should allow trailing dots, which specify fully-qualified domain names.
  EXPECT_EQ("google.com.",
            SystemRequestContext::MinimalPrivateSuffix("www.google.com."));
  EXPECT_EQ("google.com.",
            SystemRequestContext::MinimalPrivateSuffix("google.com."));

  // But two trailing dots is an error, and on errors we "fail secure" by
  // using the whole string.
  EXPECT_EQ("www.google.com..",
            SystemRequestContext::MinimalPrivateSuffix("www.google.com.."));

  // co.uk is a public suffix, so *google.co.uk just becomes google.uk
  EXPECT_EQ("google.co.uk",
            SystemRequestContext::MinimalPrivateSuffix("google.co.uk"));
  EXPECT_EQ("google.co.uk",
            SystemRequestContext::MinimalPrivateSuffix("www.google.co.uk"));
  EXPECT_EQ("google.co.uk",
            SystemRequestContext::MinimalPrivateSuffix("foo.bar.google.co.uk"));

  // Check that we handle lots of url components properly.
  EXPECT_EQ("l.co.uk", SystemRequestContext::MinimalPrivateSuffix(
      "a.b.c.d.e.f.g.h.i.j.k.l.co.uk"));

  // Check that we handle public suffixes that are not tlds.
  EXPECT_EQ("example.appspot.com",
            SystemRequestContext::MinimalPrivateSuffix("example.appspot.com"));
  EXPECT_EQ(
      "example.appspot.com",
      SystemRequestContext::MinimalPrivateSuffix("www.example.appspot.com"));

  // If a tld doesn't exist, again fail secure.
  EXPECT_EQ(
      "a.b.c.this.doesntexist",
      SystemRequestContext::MinimalPrivateSuffix("a.b.c.this.doesntexist"));

  // Check that we don't give errors on various kinds of invalid hostnames.
  EXPECT_EQ("com", SystemRequestContext::MinimalPrivateSuffix("com"));
  EXPECT_EQ("", SystemRequestContext::MinimalPrivateSuffix(""));
  EXPECT_EQ(".", SystemRequestContext::MinimalPrivateSuffix("."));
  EXPECT_EQ("..", SystemRequestContext::MinimalPrivateSuffix(".."));
  EXPECT_EQ("..doesntexist.",
            SystemRequestContext::MinimalPrivateSuffix("..doesntexist."));
}


}  // namespace

}  // namespace net_instaweb
