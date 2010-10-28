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

#include "net/instaweb/rewriter/public/css_combine_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/url_partnership.h"
#include "net/instaweb/rewriter/rewrite.pb.h"  // for CssCombineUrl
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"

namespace {

// names for Statistics variables.
const char kCssFileCountReduction[] = "css_file_count_reduction";

}  // namespace

namespace net_instaweb {

class CssCombineFilter::Partnership : public UrlPartnership {
 public:
  Partnership(DomainLawyer* domain_lawyer, const GURL& gurl)
      : UrlPartnership(domain_lawyer, gurl) {
  }

  bool AddElement(HtmlElement* element, const StringPiece& href,
                  MessageHandler* handler) {
    bool added = AddUrl(href, handler);
    if (added) {
      css_elements_.push_back(element);
    }
    return added;
  }

  HtmlElement* element(int i) { return css_elements_[i]; }

 private:
  std::vector<HtmlElement*> css_elements_;
};

// TODO(jmarantz) We exhibit zero intelligence about which css files to
// combine; we combine whatever is possible.  This can reduce performance
// by combining highly cacheable shared resources with transient ones.
//
// TODO(jmarantz): We do not recognize IE directives as spriting boundaries.
// We should supply a meaningful IEDirective method as a boundary.
//
// TODO(jmarantz): allow combining of CSS elements found in the body, whether
// or not the head has already been flushed.

CssCombineFilter::CssCombineFilter(RewriteDriver* driver,
                                   const char* filter_prefix)
    : RewriteFilter(driver, filter_prefix),
      html_parse_(driver->html_parse()),
      resource_manager_(driver->resource_manager()),
      css_tag_scanner_(html_parse_),
      css_file_count_reduction_(NULL) {
  s_link_ = html_parse_->Intern("link");
  s_href_ = html_parse_->Intern("href");
  s_type_ = html_parse_->Intern("type");
  s_rel_  = html_parse_->Intern("rel");
  s_media_ = html_parse_->Intern("media");
  s_style_ = html_parse_->Intern("style");
  Statistics* stats = resource_manager_->statistics();
  if (stats != NULL) {
    css_file_count_reduction_ = stats->GetVariable(kCssFileCountReduction);
  }
}

CssCombineFilter::~CssCombineFilter() {
}

void CssCombineFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kCssFileCountReduction);
}

void CssCombineFilter::StartDocument() {
  // This should already be clear, but just in case.
  partnership_.reset(NULL);
}

void CssCombineFilter::EndElement(HtmlElement* element) {
  HtmlElement::Attribute* href;
  const char* media;
  if (css_tag_scanner_.ParseCssElement(element, &href, &media)) {
    // We only want to combine CSS files with the same media type to avoid
    // loading unneeded content. So, if the media changes, we'll emit what we
    // have and start over.
    if (partnership_.get() != NULL && combine_media_ != media) {
      EmitCombinations();
    }
    if (partnership_.get() == NULL) {
      partnership_.reset(new Partnership(
          resource_manager_->domain_lawyer(),
          html_parse_->gurl()));
      combine_media_ = media;
    }
    MessageHandler* handler = html_parse_->message_handler();
    if (!partnership_->AddElement(element, href->value(), handler)) {
      // It's invalid, so just skip it a la IEDirective below.
      EmitCombinations();
    }
  } else if (element->tag() == s_style_) {
    // We can't reorder styles on a page, so if we are only combining <link>
    // tags, we can't combine them across a <style> tag.
    // TODO(sligocki): Maybe we should just combine <style>s too?
    // We can run outline_css first for now to make all <style>s into <link>s.
    EmitCombinations();
  }
}

// An IE directive that includes any stylesheet info should be a barrier
// for css combining.  It's OK to emit the combination we've seen so far.
void CssCombineFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  // TODO(sligocki): Figure out how to safely parse IEDirectives, for now we
  // just consider them black boxes / solid barriers.
  EmitCombinations();
}

void CssCombineFilter::Flush() {
  EmitCombinations();
}

void CssCombineFilter::EmitCombinations() {
  if (partnership_.get() == NULL) {
    return;
  }
  partnership_->Resolve();
  MessageHandler* handler = html_parse_->message_handler();

  // It's possible that we'll have found 2 css files to combine, but one
  // of them became non-rewritable due to a flush, and thus we'll wind
  // up spriting just one file, so do a first pass counting rewritable
  // css links.  Also, load the CSS content in this pass.  We will only
  // do a combine if we have more than one css element that successfully
  // loaded.
  std::vector<HtmlElement*> combine_elements;
  ResourceVector combine_resources;
  for (int i = 0, n = partnership_->num_urls(); i < n; ++i) {
    HtmlElement* element = partnership_->element(i);
    const char* media;
    HtmlElement::Attribute* href;
    if (css_tag_scanner_.ParseCssElement(element, &href, &media) &&
        html_parse_->IsRewritable(element)) {
      CHECK(combine_media_ == media);
      // TODO(jmarantz): consider async loads; exclude css file
      // from the combination that are not yet loaded.  For now, our
      // loads are blocking.  Need to understand Apache module
      // TODO(jmaessen, jmarantz): use partnership url data here,
      // hand off to CreateInputResourceGURL.
      Resource* css_resource =
          resource_manager_->CreateInputResource(html_parse_->gurl(),
                                                 href->value(), handler);
      if ((css_resource != NULL) &&
          resource_manager_->ReadIfCached(css_resource, handler) &&
          css_resource->ContentsValid()) {
        if (i != 0 &&
            CssTagScanner::HasImport(css_resource->contents(), handler)) {
          // If any stylesheet after the first has imports, don't combine.
          // TODO(sligocki): We could try to flatten imports.
          // TODO(sligocki): We could combine all the sheets up to here.
          // It's unclear how often this happens -> how valueable this would be.
          break;
        }
        combine_resources.push_back(css_resource);

        // Try to add this resource to the combination.  We are not yet
        // committed to the combination because we haven't written the
        // contents to disk yet, so don't mutate the DOM but keep
        // track of which elements will be involved
        combine_elements.push_back(element);
      } else {
        handler->Message(kWarning, "Failed to create or read input resource %s",
                         href->value());
      }
    }
  }

  if (combine_elements.size() > 1) {
    // Ideally like to have a data-driven service tell us which elements should
    // be combined together.  Note that both the resources and the elements
    // are managed, so we don't delete them even if the spriting fails.

    // First, compute the name of the new resource based on the names of
    // the CSS files.
    CssCombineUrl css_combine_url;
    std::string url_safe_id;
    for (int i = 0, n = combine_resources.size(); i < n; ++i) {
      Resource* css_resource = combine_resources[i];
      // Note that css_resource has been checked for eligibility for rewriting
      // in advance, and it's url is now absolute.  For the moment we're just
      // gathering up absolute urls.
      // TODO(jmaessen): Use relative urls as soon as we understand the
      // semantics.
      css_combine_url.add_element_url(css_resource->url());
    }
    Encode(css_combine_url, &url_safe_id);

    HtmlElement* combine_element = html_parse_->NewElement(NULL, s_link_);
    combine_element->AddAttribute(s_rel_, "stylesheet", "\"");
    combine_element->AddAttribute(s_type_, "text/css", "\"");
    if (!combine_media_.empty()) {
      combine_element->AddAttribute(s_media_, combine_media_, "\"");
    }

    // Start building up the combination.  At this point we are still
    // not committed to the combination, because the 'write' can fail.
    // TODO(jmaessen, jmarantz): encode based on partnership
    scoped_ptr<OutputResource> combination(
        resource_manager_->CreateNamedOutputResource(
            filter_prefix_, url_safe_id, &kContentTypeCss, handler));
    bool written = combination->IsWritten() ||
        WriteCombination(combine_resources, combination.get(), handler);

    // We've collected at least two CSS files to combine, and whose
    // HTML elements are in the current flush window.  Last step
    // is to write the combination.
    if (written && combination->IsWritten()) {
      // Commit by adding combine element ...
      combine_element->AddAttribute(s_href_, combination->url(), "\"");
      // TODO(sligocki): Put at top of head/flush-window.
      // Right now we're putting it where the first original element used to be.
      html_parse_->InsertElementBeforeElement(partnership_->element(0),
                                              combine_element);
      // ... and removing originals from the DOM.
      for (size_t i = 0; i < combine_elements.size(); ++i) {
        html_parse_->DeleteElement(combine_elements[i]);
      }
      html_parse_->InfoHere("Combined %d CSS files into one",
                            static_cast<int>(combine_elements.size()));
      if (css_file_count_reduction_ != NULL) {
        css_file_count_reduction_->Add(combine_elements.size() - 1);
      }
    }
  }
  STLDeleteContainerPointers(combine_resources.begin(),
                             combine_resources.end());

  partnership_.reset(NULL);
}

bool CssCombineFilter::WriteCombination(const ResourceVector& combine_resources,
                                        OutputResource* combination,
                                        MessageHandler* handler) {
  bool written = true;
  std::string combined_contents;
  StringWriter writer(&combined_contents);
  int64 min_origin_expiration_time_ms = 0;

  for (int i = 0, n = combine_resources.size(); written && (i < n); ++i) {
    Resource* input = combine_resources[i];
    StringPiece contents = input->contents();
    int64 input_expire_time_ms = input->CacheExpirationTimeMs();
    if ((min_origin_expiration_time_ms == 0) ||
        (input_expire_time_ms < min_origin_expiration_time_ms)) {
      min_origin_expiration_time_ms = input_expire_time_ms;
    }

    // TODO(sligocki): We need a real CSS parser.  But for now we have to make
    // any URLs absolute.
    written = css_tag_scanner_.AbsolutifyUrls(contents, input->url(),
                                              &writer, handler);
  }
  if (written) {
    if (combination->resolved_base().empty()) {
      combination->set_resolved_base(partnership_->ResolvedBase());
    }
    written =
        resource_manager_->Write(
            HttpStatus::kOK, combined_contents, combination,
            min_origin_expiration_time_ms, handler);
  }
  return written;
}

// Callback to run whenever a CSS resource is collected.  This keeps a
// count of the resources collected so far.  When the last one is collected,
// it aggregates the results and calls the final callback with the result.
class CssCombiner : public Resource::AsyncCallback {
 public:
  CssCombiner(CssCombineFilter* filter,
              MessageHandler* handler,
              UrlAsyncFetcher::Callback* callback,
              OutputResource* combination,
              Writer* writer,
              MetaData* response_headers) :
      enable_completion_(false),
      done_count_(0),
      fail_count_(0),
      filter_(filter),
      message_handler_(handler),
      callback_(callback),
      combination_(combination),
      writer_(writer),
      response_headers_(response_headers) {
  }

  virtual ~CssCombiner() {
    STLDeleteContainerPointers(combine_resources_.begin(),
                               combine_resources_.end());
  }

  void AddResource(Resource* resource) {
    combine_resources_.push_back(resource);
  }

  virtual void Done(bool success, Resource* resource) {
    if (!success) {
      ++fail_count_;
    }
    ++done_count_;

    if (Ready()) {
      DoCombination();
    }
  }

  bool Ready() {
    return (enable_completion_ &&
            (done_count_ == combine_resources_.size()));
  }

  void EnableCompletion() {
    enable_completion_ = true;
    if (Ready()) {
      DoCombination();
    }
  }

  void DoCombination() {
    bool ok = fail_count_ == 0;
    for (size_t i = 0; ok && (i < combine_resources_.size()); ++i) {
      Resource* css_resource = combine_resources_[i];
      ok = css_resource->ContentsValid();
    }
    if (ok) {
      if (response_headers_ != NULL) {
        response_headers_->CopyFrom(*combination_->metadata());
      }
      ok = (filter_->WriteCombination(combine_resources_, combination_,
                                      message_handler_) &&
            combination_->IsWritten() &&
            ((writer_ == NULL) ||
             writer_->Write(combination_->contents(), message_handler_)));
    }
    callback_->Done(ok);
    delete this;
  }

 private:
  bool enable_completion_;
  size_t done_count_;
  size_t fail_count_;
  CssCombineFilter* filter_;
  MessageHandler* message_handler_;
  UrlAsyncFetcher::Callback* callback_;
  OutputResource* combination_;
  CssCombineFilter::ResourceVector combine_resources_;
  Writer* writer_;
  MetaData* response_headers_;

  DISALLOW_COPY_AND_ASSIGN(CssCombiner);
};

bool CssCombineFilter::Fetch(OutputResource* combination,
                             Writer* writer,
                             const MetaData& request_header,
                             MetaData* response_headers,
                             UrlAsyncFetcher* fetcher,
                             MessageHandler* message_handler,
                             UrlAsyncFetcher::Callback* callback) {
  bool ret = false;
  StringPiece url_safe_id = combination->name();
  CssCombineUrl css_combine_url;
  if (Decode(url_safe_id, &css_combine_url)) {
    std::string url, decoded_resource;
    ret = true;
    CssCombiner* combiner = new CssCombiner(
        this, message_handler, callback, combination, writer, response_headers);
    for (int i = 0; i < css_combine_url.element_url_size(); ++i)  {
      const std::string& url = css_combine_url.element_url(i);
      Resource* css_resource =
          resource_manager_->CreateInputResourceAbsolute(url, message_handler);
      combiner->AddResource(css_resource);
      resource_manager_->ReadAsync(css_resource, combiner, message_handler);
    }

    // In the case where the first input CSS files is already cached,
    // ReadAsync will directly call the CssCombineCallback, which, if
    // already enabled, would think it was complete and run DoCombination
    // prematurely.  So we wait until the resources are all added before
    // enabling the callback to complete.
    combiner->EnableCompletion();
  } else {
    message_handler->Error(url_safe_id.as_string().c_str(), 0,
                           "Unable to decode resource string");
  }
  return ret;
}

}  // namespace net_instaweb
