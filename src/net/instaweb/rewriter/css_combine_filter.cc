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

} // namespace

namespace net_instaweb {

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
  s_head_ = html_parse_->Intern("head");
  s_link_ = html_parse_->Intern("link");
  s_href_ = html_parse_->Intern("href");
  s_type_ = html_parse_->Intern("type");
  s_rel_  = html_parse_->Intern("rel");
  head_element_ = NULL;
  Statistics* stats = resource_manager_->statistics();
  if (stats != NULL) {
    css_file_count_reduction_ = stats->GetVariable(kCssFileCountReduction);
  }
}

void CssCombineFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kCssFileCountReduction);
}

void CssCombineFilter::StartDocument() {
  head_element_ = NULL;
}

void CssCombineFilter::StartElement(HtmlElement* element) {
  if (element->tag() == s_head_) {
    head_element_ = element;
  }
}

void CssCombineFilter::EndElement(HtmlElement* element) {
  HtmlElement::Attribute* href;
  const char* media;
  if (css_tag_scanner_.ParseCssElement(element, &href, &media)) {
    css_elements_.push_back(element);
  } else if (element->tag() == s_head_) {
    EmitCombinations(element);
  }
}

// An IE directive that includes any stylesheet info should be a barrier
// for css spriting.  It's OK to emit the spriting we've seen so far.
void CssCombineFilter::IEDirective(const std::string& directive) {
  // TODO(jmarantz): consider recursively invoking the parser and
  // parsing all the IE-specific code properly.
  if (directive.find("stylesheet") != std::string::npos) {
  }
}

void CssCombineFilter::Flush() {
  // TODO(jmarantz): Ideally, all the css links will be encountered in the
  // <head>, before the first flush.  It's possible we'll get a Flush,
  // during the <head> parse, and there may be some css files before it,
  // and some afterward.  And there may be css links encountered in the body,
  // and there may have Flushed our head css combinations first.  So all of that
  // will have to be dealt with by calling EmitCombinations, after finding the
  // appropriate place in the DOM to insert the combination.
  //
  // The best performance will come when the entire document is parsed
  // without a Flush, in which case we can move all the css links into
  // the <head>, but even that is not yet implemented.
  css_elements_.clear();
}

void CssCombineFilter::EmitCombinations(HtmlElement* head) {
  MessageHandler* handler = html_parse_->message_handler();

  // It's possible that we'll have found 2 css files to combine, but one
  // of them became non-rewritable due to a flush, and thus we'll wind
  // up spriting just one file, so do a first pass counting rewritable
  // css linkes.  Also, load the CSS content in this pass.  We will only
  // do a combine if we have more than one css element that successfully
  // loaded.
  std::vector<HtmlElement*> combine_elements;
  ResourceVector combine_resources;
  StringVector media_attributes;
  for (int i = 0, n = css_elements_.size(); i < n; ++i) {
    HtmlElement* element = css_elements_[i];
    const char* media;
    HtmlElement::Attribute* href;
    if (css_tag_scanner_.ParseCssElement(element, &href, &media) &&
        html_parse_->IsRewritable(element)) {
      // TODO(jmarantz): consider async loads; exclude css file
      // from the combination that are not yet loaded.  For now, our
      // loads are blocking.  Need to understand Apache module
      Resource* css_resource =
          resource_manager_->CreateInputResource(href->value(), handler);
      if ((css_resource != NULL) &&
          resource_manager_->ReadIfCached(css_resource, handler) &&
          css_resource->ContentsValid()) {
        media_attributes.push_back(media);
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
      CssUrl* css_url = css_combine_url.add_element();
      css_url->set_origin_url(css_resource->url());
      css_url->set_media(media_attributes[i]);
    }
    Encode(css_combine_url, &url_safe_id);

    HtmlElement* combine_element = html_parse_->NewElement(head, s_link_);
    combine_element->AddAttribute(s_rel_, "stylesheet", "\"");
    combine_element->AddAttribute(s_type_, "text/css", "\"");

    // Start building up the combination.  At this point we are still
    // not committed to the combination, because the 'write' can fail.
    scoped_ptr<OutputResource> combination(
        resource_manager_->CreateNamedOutputResource(
            filter_prefix_, url_safe_id, &kContentTypeCss, handler));
    bool written = combination->IsWritten() ||
        WriteCombination(combine_resources, media_attributes, combination.get(),
                         handler);

    // We've collected at least two CSS files to combine, and whose
    // HTML elements are in the current flush window.  Last step
    // is to write the combination.
    if (written && combination->IsWritten()) {
      // commit by removing the elements from the DOM.
      for (size_t i = 0; i < combine_elements.size(); ++i) {
        html_parse_->DeleteElement(combine_elements[i]);
      }
      combine_element->AddAttribute(s_href_, combination->url(), "\"");
      html_parse_->InsertElementBeforeCurrent(combine_element);
      html_parse_->InfoHere("Combined %d CSS files into one",
                            static_cast<int>(combine_elements.size()));
      if (css_file_count_reduction_ != NULL) {
        css_file_count_reduction_->Add(combine_elements.size() - 1);
      }
    }
  }
  css_elements_.clear();
  STLDeleteContainerPointers(combine_resources.begin(),
                             combine_resources.end());
}

bool CssCombineFilter::WriteCombination(
    const ResourceVector& combine_resources,
    const StringVector& combine_media,
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

    const std::string& media = combine_media[i];
    if (!media.empty()) {
      combined_contents += "\n@Media ";
      combined_contents += media;
      combined_contents += " {\n";
    }

    // TODO(sligocki): We need a real CSS parser.  But for now we have to make
    // any URLs absolute.
    written = css_tag_scanner_.AbsolutifyUrls(contents, input->url(),
                                              &writer, handler);

    if (!media.empty()) {
      combined_contents += "\n}\n";
    }
  }
  if (written) {
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

  void AddResource(Resource* resource, const char* media) {
    combine_resources_.push_back(resource);
    combine_media_.push_back(media);
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
      ok = (filter_->WriteCombination(combine_resources_, combine_media_,
                                      combination_, message_handler_) &&
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
  StringVector combine_media_;
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
    for (int i = 0; i < css_combine_url.element_size(); ++i)  {
      const CssUrl& css_url = css_combine_url.element(i);
      std::string url = css_url.origin_url();
      Resource* css_resource =
          resource_manager_->CreateInputResource(url, message_handler);
      combiner->AddResource(css_resource, css_url.media().c_str());
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
