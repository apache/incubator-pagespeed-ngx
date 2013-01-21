// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PAGESPEED_TESTING_PAGESPEED_TEST_H_
#define PAGESPEED_TESTING_PAGESPEED_TEST_H_

#include <map>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/scoped_ptr.h"
#include "pagespeed/core/image_attributes.h"
#include "pagespeed/core/pagespeed_input.h"
#include "pagespeed/core/resource.h"
#include "pagespeed/core/result_provider.h"
#include "pagespeed/core/rule.h"
#include "pagespeed/core/rule_input.h"
#include "pagespeed/proto/pagespeed_output.pb.h"
#include "pagespeed/proto/pagespeed_proto_formatter.pb.h"
#include "pagespeed/testing/fake_dom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pagespeed {
class Resource;
class TopLevelBrowsingContext;
}  // namespace pagespeed

namespace pagespeed_testing {

void AssertProtoEq(const ::google::protobuf::MessageLite& a,
                   const ::google::protobuf::MessageLite& b);

// Helper method that simply invokes ASSERT_TRUE. Can be used in
// functions that have a non-void return type (ASSERT_TRUE requires
// its containing function to have a void return type).
void AssertTrue(bool condition);

// Read the file at the given path (relative to the root of the source
// tree) into the dest buffer. Uses the --srcroot flag to determine
// the proper location of the root of the source tree.
bool ReadFileToString(const std::string& filename, std::string *dest);

class FakeImageAttributesFactory
    : public pagespeed::ImageAttributesFactory {
 public:
  typedef std::map<const pagespeed::Resource*, std::pair<int,int> >
      ResourceSizeMap;
  explicit FakeImageAttributesFactory(const ResourceSizeMap& resource_size_map)
      : resource_size_map_(resource_size_map) {
  }
  virtual pagespeed::ImageAttributes* NewImageAttributes(
      const pagespeed::Resource* resource) const;
 private:
  ResourceSizeMap resource_size_map_;
};


// Helper method that returns the output from a TextFormatter for
// the given Rule and Results.
std::string DoFormatResultsAsText(pagespeed::Rule* rule,
                                  const pagespeed::RuleResults& rule_results);

void DoFormatResultsAsProto(pagespeed::Rule* rule,
                            const pagespeed::RuleResults& rule_results,
                            pagespeed::FormattedResults* formatted_results);


class PagespeedTest : public ::testing::Test {
 protected:
  // Some sample URLs that tests may choose to use.
  static const char* kUrl1;
  static const char* kUrl2;
  static const char* kUrl3;
  static const char* kUrl4;

  PagespeedTest();
  virtual ~PagespeedTest();

  // Derived classes should not override SetUp and TearDown. Derived
  // classes should override DoSetUp and DoTearDown instead.
  virtual void SetUp();
  virtual void TearDown();

  // Hooks for derived classes to override.
  virtual void DoSetUp();
  virtual void DoTearDown();

  // Freeze the PagespeedInput structure.
  virtual void Freeze();
  virtual void Freeze(bool expected_result);

  // Construct a new HTTP GET Resource with the specified URL and
  // status code, and add that resource to our PagespeedInput.
  // Return NULL if the resource was unable to be created or added to
  // the PagespeedInput.
  pagespeed::Resource* NewResource(const std::string& url, int status_code);

  // Construct the primary resource, an HTTP GET HTML
  // resource with a 200 status code. An associated FakeDomDocument
  // will be created for this resource, which is stored as the DOM
  // document of the PagespeedInput. The FakeDomDocument is available
  // via the document() method. This method must only be called once
  // per test.
  pagespeed::Resource* NewPrimaryResource(const std::string& url);

  // Construct an HTTP GET HTML resource with a 200 status code. An
  // associated FakeDomDocument will be created for this resource,
  // parented under the specified iframe and returned via the
  // out_document parameter, if specified.
  pagespeed::Resource* NewDocumentResource(const std::string& url,
                                           FakeDomElement* iframe = NULL,
                                           FakeDomDocument** out = NULL);

  // Construct a new HTTP GET Resource with the specified URL and
  // a 200 status code, and add that resource to our PagespeedInput.
  pagespeed::Resource* New200Resource(const std::string& url);

  // Construct a new HTTP GET redirect (302) Resource with the
  // specified source and destination URLs, and add that resource
  // to our PagespeedInput.
  pagespeed::Resource* New302Resource(const std::string& source,
                                      const std::string& destination);

  // Construct a new HTTP GET image (PNG) resource, and add that
  // resource to our PagespeedInput. Also create an associated DOM
  // node, parented under the specified parent, and returned via the
  // out parameter, if specified.
  pagespeed::Resource* NewPngResource(const std::string& url,
                                      FakeDomElement* parent = NULL,
                                      FakeDomElement** out = NULL);

  // Much like NewPngResource, but creates two resources -- a redirect from
  // url1 to url2, and a PNG at url2 -- and creates an IMG element with
  // src=url1.  This is useful for testing that a rule is able to get the
  // content/dimensions/etc. of the image even though the DOM node refers to
  // the URL of the redirect rather than the actual image resource.
  pagespeed::Resource* NewRedirectedPngResource(const std::string& url1,
                                                const std::string& url2,
                                                FakeDomElement* parent = NULL,
                                                FakeDomElement** out = NULL);

  // Construct a new HTTP GET script resource, and add that
  // resource to our PagespeedInput. Also create an associated DOM
  // node, parented under the specified parent, and returned via the
  // out parameter, if specified.
  pagespeed::Resource* NewScriptResource(const std::string& url,
                                         FakeDomElement* parent = NULL,
                                         FakeDomElement** out = NULL);

  // Construct a new HTTP GET CSS resource, and add that
  // resource to our PagespeedInput. Also create an associated DOM
  // node, parented under the specified parent, and returned via the
  // out parameter, if specified.
  pagespeed::Resource* NewCssResource(const std::string& url,
                                      FakeDomElement* parent = NULL,
                                      FakeDomElement** out = NULL);

  // Set the top-level browsing context.
  bool SetTopLevelBrowsingContext(pagespeed::TopLevelBrowsingContext* context);

  // Create a new TopLevelBrowsingContext with the specified document,
  // and transfer its ownership to the PagespeedInput. If you call
  // this method, SetTopLevelBrowsingContext() is called for you
  // internally for the returned context.
  pagespeed::TopLevelBrowsingContext* NewTopLevelBrowsingContext(
      const pagespeed::Resource* document_resource);

  // Construct default html, head, and body DOM elements under the
  // document. NewPrimaryResource() must be called prior to calling
  // this method, in order to create a root document that these
  // elements can be parented under.
  void CreateHtmlHeadBodyElements();

  // Adds an ImageAttributesFactory to the PagespeedInput that can
  // returns ImageAttributes according to the ResourceSizeMap.
  bool AddFakeImageAttributesFactory(
      const FakeImageAttributesFactory::ResourceSizeMap& map);

  // Adds an InstrumentationData to the set of InstrumentationData for
  // the PagespeedInput. Can be called multiple times to add multiple
  // root InstrumentationData instances. Ownership of the
  // InstrumentationData instances is transferred to this object.
  void AddInstrumentationData(const pagespeed::InstrumentationData* data);

  bool SetOnloadTimeMillis(int onload_millis) {
    return pagespeed_input_->SetOnloadTimeMillis(onload_millis);
  }

  bool SetViewportWidthAndHeight(int width, int height) {
    return pagespeed_input_->SetViewportWidthAndHeight(width, height);
  }

  const pagespeed::PagespeedInput* pagespeed_input() {
    return pagespeed_input_.get();
  }
  pagespeed::Resource* primary_resource() const { return primary_resource_; }
  FakeDomDocument* document() { return document_; }
  FakeDomElement* html() { return html_; }
  FakeDomElement* head() { return head_; }
  FakeDomElement* body() { return body_; }

  // Add a resource. Do not call this method for resources constructed
  // using New*Resource, as those resources have already been added to
  // the PagespeedInput. Use this method only for backward
  // compatibility with tests that don't use the New*Resource()
  // methods to construct resouces.
  bool AddResource(pagespeed::Resource* resource);

 private:
  base::AtExitManager at_exit_manager_;
  pagespeed::InstrumentationDataVector instrumentation_data_;
  scoped_ptr<pagespeed::PagespeedInput> pagespeed_input_;
  pagespeed::Resource* primary_resource_;
  FakeDomDocument* document_;
  FakeDomElement* html_;
  FakeDomElement* head_;
  FakeDomElement* body_;
};

// A base testing class for use when writing rule tests.
template <class RULE> class PagespeedRuleTest : public PagespeedTest {
 protected:
  PagespeedRuleTest()
      : rule_(new RULE()), provider_(*rule_.get(), &rule_results_, 0) {
    rule_results_.set_rule_name(rule_->name());
  }

  const pagespeed::RuleInput* rule_input() { return rule_input_.get(); }
  const pagespeed::RuleResults& rule_results() const { return rule_results_; }
  const int num_results() const { return rule_results_.results_size(); }
  const pagespeed::Result& result(int i) const {
    return rule_results_.results(i);
  }
  const std::string& results_rule_name() const {
    return rule_results_.rule_name();
  }
  // Get a details instance of the specified type.
  template <class DETAILS> const DETAILS& details(int i) const {
    const pagespeed::ResultDetails& details = result(i).details();
    AssertTrue(details.HasExtension(DETAILS::message_set_extension));
    return details.GetExtension(DETAILS::message_set_extension);
  }

  virtual void SetUp() {
    PagespeedTest::SetUp();
  }

  virtual void TearDown() {
    rule_input_.reset();
    PagespeedTest::TearDown();
  }

  virtual void Freeze(bool expected_result) {
    PagespeedTest::Freeze(expected_result);
    rule_input_.reset(new pagespeed::RuleInput(*pagespeed_input()));
    rule_input_->Init();
  }

  virtual void Freeze() {
    Freeze(true);
  }

  bool AppendResults() {
    return rule_->AppendResults(*rule_input(), &provider_);
  }

  void CheckNoViolations() {
    Freeze();
    ASSERT_TRUE(AppendResults());
    ASSERT_EQ(0, num_results());
  }

  void CheckOneUrlViolation(const std::string& violation_url) {
    std::vector<std::string> expected;
    expected.push_back(violation_url);
    CheckExpectedUrlViolations(expected);
  }

  void CheckTwoUrlViolations(const std::string& violation_url1,
                             const std::string& violation_url2) {
    std::vector<std::string> expected;
    expected.push_back(violation_url1);
    expected.push_back(violation_url2);
    CheckExpectedUrlViolations(expected);
  }

  void CheckExpectedUrlViolations(const std::vector<std::string>& expected) {
    Freeze();
    ASSERT_TRUE(AppendResults());
    ASSERT_EQ(num_results(), static_cast<int>(expected.size()));

    for (size_t idx = 0; idx < expected.size(); ++idx) {
      const pagespeed::Result& res = result(idx);
      ASSERT_EQ(res.resource_urls_size(), 1);
      EXPECT_EQ(expected[idx], res.resource_urls(0));
    }
  }

  // TODO(mdsteele): Rename this to FormatResultsAsText
  std::string FormatResults() {
    return DoFormatResultsAsText(rule_.get(), rule_results_);
  }

  void FormatResultsAsProto(pagespeed::FormattedResults* formatted_results) {
    return DoFormatResultsAsProto(rule_.get(), rule_results_,
                                  formatted_results);
  }

  int ComputeScore() {
    return rule_->ComputeScore(*pagespeed_input()->input_information(),
                               rule_results_);
  }

  double ComputeRuleImpact() {
    return rule_->ComputeRuleImpact(*pagespeed_input()->input_information(),
                                    rule_results_);
  }

 private:
  scoped_ptr<pagespeed::RuleInput> rule_input_;
  scoped_ptr<pagespeed::Rule> rule_;
  pagespeed::RuleResults rule_results_;
  pagespeed::ResultProvider provider_;
};

}  // namespace pagespeed_testing

#endif  // PAGESPEED_TESTING_PAGESPEED_TEST_H_
