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

#include <assert.h>
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/input_resource.h"
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

CssCombineFilter::CssCombineFilter(const char* filter_prefix,
                                   HtmlParse* html_parse,
                                   ResourceManager* resource_manager)
    : RewriteFilter(filter_prefix),
      html_parse_(html_parse),
      resource_manager_(resource_manager),
      css_filter_(html_parse),
      counter_(NULL) {
  s_head_ = html_parse->Intern("head");
  s_link_ = html_parse->Intern("link");
  s_href_ = html_parse->Intern("href");
  s_type_ = html_parse->Intern("type");
  s_rel_  = html_parse->Intern("rel");
  head_element_ = NULL;
  Statistics* stats = resource_manager_->statistics();
  if (stats != NULL) {
    counter_ = stats->AddVariable("css_file_count_reduction");
  }
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
  if (css_filter_.ParseCssElement(element, &href, &media)) {
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
  InputResourceVector combine_resources;
  std::vector<std::string> media_attributes;
  for (int i = 0, n = css_elements_.size(); i < n; ++i) {
    HtmlElement* element = css_elements_[i];
    const char* media;
    HtmlElement::Attribute* href;
    if (css_filter_.ParseCssElement(element, &href, &media) &&
        html_parse_->IsRewritable(element)) {
      // TODO(jmarantz): consider async loads; exclude css file
      // from the combination that are not yet loaded.  For now, our
      // loads are blocking.  Need to understand Apache module
      InputResource* css_resource =
          resource_manager_->CreateInputResource(href->value(), handler);
      if ((css_resource != NULL) && css_resource->Read(handler)) {
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
      InputResource* css_element = combine_resources[i];
      CssUrl* css_url = css_combine_url.add_element();
      css_url->set_origin_url(css_element->absolute_url());
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
            filter_prefix_, url_safe_id, kContentTypeCss));
    bool written = combination->IsWritten() ||
        WriteCombination(combine_resources, combination.get(), handler);

    // We've collected at least two CSS files to combine, and whose
    // HTML elements are in the current flush window.  Last step
    // is to write the combination.
    if (written && combination->IsReadable()) {
      // commit by removing the elements from the DOM.
      for (size_t i = 0; i < combine_elements.size(); ++i) {
        html_parse_->DeleteElement(combine_elements[i]);
      }
      combine_element->AddAttribute(s_href_, combination->url(), "\"");
      html_parse_->InsertElementBeforeCurrent(combine_element);
      html_parse_->InfoHere("Combined %d CSS files into one",
                            static_cast<int>(combine_elements.size()));
      if (counter_ != NULL) {
        counter_->Add(combine_elements.size() - 1);
      }
    }
  }
  css_elements_.clear();
  STLDeleteContainerPointers(combine_resources.begin(),
                             combine_resources.end());
}

bool CssCombineFilter::WriteCombination(
    const InputResourceVector& combine_resources,
    OutputResource* combination,
    MessageHandler* handler) {
  Writer* writer = combination->BeginWrite(handler);
  bool written = (writer != NULL);
  if (written) {
    for (int i = 0, n = combine_resources.size(); written && (i < n); ++i) {
      InputResource* input = combine_resources[i];
      const std::string& contents = input->contents();

      // We need a real CSS parser.  But for now we have to make
      // any URLs absolute.
      written = css_filter_.AbsolutifyUrls(contents, input->url(),
                                           writer, handler);
    }
    written &= combination->EndWrite(writer, handler);
  }
  return written;
}

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
    InputResourceVector combine_resources;

    // TODO(jmarantz): This code reports failure if we do not have the
    // input .css files in cache.  Instead we should issue fetches for
    // each sub-resource and write the aggregate once we have all of them,
    // then call the callback.
    for (int i = 0; i < css_combine_url.element_size(); ++i)  {
      const CssUrl& css_url = css_combine_url.element(i);

      InputResource* css_resource =
          resource_manager_->CreateInputResource(css_url.origin_url(),
                                                 message_handler);
      if ((css_resource != NULL) && css_resource->Read(message_handler) &&
          css_resource->ContentsValid()) {
        combine_resources.push_back(css_resource);
      } else {
        ret = false;
      }
    }

    if (ret) {
      ret = (WriteCombination(combine_resources, combination,
                              message_handler) &&
             combination->IsWritten() &&
             resource_manager_->FetchOutputResource(
                 combination, writer, response_headers, message_handler));
    }
  }

  if (ret) {
    resource_manager_->SetDefaultHeaders(kContentTypeCss, response_headers);
    callback->Done(true);
  } else {
    message_handler->Error(url_safe_id.as_string().c_str(), 0,
                           "Unable to decode resource string");
  }
  return ret;
}

}  // namespace net_instaweb
