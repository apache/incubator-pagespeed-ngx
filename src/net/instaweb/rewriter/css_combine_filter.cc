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
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

// names for Statistics variables.
const char kCssFileCountReduction[] = "css_file_count_reduction";

}  // namespace

class CssCombineFilter::Partnership : public CombineFilterBase::Partnership {
 public:
  Partnership(CssCombineFilter* filter, RewriteDriver* driver, int url_overhead)
      : CombineFilterBase::Partnership(filter, driver, url_overhead) {
  }

  virtual bool ResourceCombinable(Resource* resource, MessageHandler* handler) {
    // styles containing @import cannot be appended to others, as any
    // @import in the middle will be ignored.
    return ((num_urls() == 0)
            || !CssTagScanner::HasImport(resource->contents(), handler));
  }

  bool AddElement(HtmlElement* element, const StringPiece& href,
                  const StringPiece& media, MessageHandler* handler) {
    if (num_urls() == 0) {
      // TODO(jmarantz): do media='' and media='display mean the same
      // thing?  sligocki thinks mdsteele looked into this and it
      // depended on HTML version.  In one display was default, in the
      // other screen was IIRC.
      media.CopyToString(&media_);
    } else {
      // After the first CSS file, subsequent CSS files must have matching media
      if (media_ != media)
        return false;
    }
    return CombineFilterBase::Partnership::AddElement(element, href, handler);
  }

  const std::string& media() const { return media_; }

 private:
  std::string media_;
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
//
// TODO(jmaessen): The addition of 1 below avoids the leading ".";
// make this convention consistent and fix all code.
CssCombineFilter::CssCombineFilter(RewriteDriver* driver,
                                   const char* filter_prefix)
    : CombineFilterBase(driver, filter_prefix,
                        kContentTypeCss.file_extension() + 1),
      html_parse_(driver->html_parse()),
      css_tag_scanner_(html_parse_),
      css_file_count_reduction_(NULL) {
  s_link_ = html_parse_->Intern("link");
  s_href_ = html_parse_->Intern("href");
  s_type_ = html_parse_->Intern("type");
  s_rel_  = html_parse_->Intern("rel");
  s_media_ = html_parse_->Intern("media");
  s_style_ = html_parse_->Intern("style");
  Statistics* stats = resource_manager()->statistics();
  if (stats != NULL) {
    css_file_count_reduction_ = stats->GetVariable(kCssFileCountReduction);
  }
}

CssCombineFilter::~CssCombineFilter() {
}

void CssCombineFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kCssFileCountReduction);
}

void CssCombineFilter::StartDocumentImpl() {
  // This should already be clear, but just in case.
  partnership_.reset(new Partnership(this, driver_, url_overhead_));
}

void CssCombineFilter::EndElementImpl(HtmlElement* element) {
  HtmlElement::Attribute* href;
  const char* media;
  if (css_tag_scanner_.ParseCssElement(element, &href, &media)) {
    // We cannot combine with a link in <noscript> tag and we cannot combine
    // over a link in a <noscript> tag, so this is a barrier.
    if (noscript_element() != NULL) {
      TryCombineAccumulated();
    } else {
      const char* url = href->value();
      MessageHandler* handler = html_parse_->message_handler();

      if (!partnership_->AddElement(element, url, media, handler)) {
        // This element can't be included in the previous combination,
        // so try to flush out what we have.
        TryCombineAccumulated();

        // Now we'll try to start a new partnership with this CSS file --
        // perhaps we ran out out of space in the previous combination
        // or this file is simply in a different authorized domain, or
        // contained @Import.
        //
        // Note that it's OK if this fails; we will simply not rewrite
        // the element in that case
        partnership_->AddElement(element, url, media, handler);
      }
    }
  } else if (element->tag() == s_style_) {
    // We can't reorder styles on a page, so if we are only combining <link>
    // tags, we can't combine them across a <style> tag.
    // TODO(sligocki): Maybe we should just combine <style>s too?
    // We can run outline_css first for now to make all <style>s into <link>s.
    TryCombineAccumulated();
  }
}

// An IE directive that includes any stylesheet info should be a barrier
// for css combining.  It's OK to emit the combination we've seen so far.
void CssCombineFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  // TODO(sligocki): Figure out how to safely parse IEDirectives, for now we
  // just consider them black boxes / solid barriers.
  TryCombineAccumulated();
}

void CssCombineFilter::Flush() {
  TryCombineAccumulated();
}

void CssCombineFilter::TryCombineAccumulated() {
  CHECK(partnership_.get() != NULL);
  MessageHandler* handler = html_parse_->message_handler();
  if (partnership_->num_urls() > 1) {
    // Ideally like to have a data-driven service tell us which elements should
    // be combined together.  Note that both the resources and the elements
    // are managed, so we don't delete them even if the spriting fails.

    // First, compute the name of the new resource based on the names of
    // the CSS files.
    std::string url_safe_id = partnership_->UrlSafeId();
    HtmlElement* combine_element = html_parse_->NewElement(NULL, s_link_);
    combine_element->AddAttribute(s_rel_, "stylesheet", "\"");
    combine_element->AddAttribute(s_type_, "text/css", "\"");
    StringPiece media = partnership_->media();
    if (!media.empty()) {
      combine_element->AddAttribute(s_media_, media, "\"");
    }

    // Start building up the combination.  At this point we are still
    // not committed to the combination, because the 'write' can fail.
    // TODO(jmaessen, jmarantz): encode based on partnership
    scoped_ptr<OutputResource> combination(
        resource_manager()->CreateOutputResourceWithPath(
            partnership_->ResolvedBase(),
            filter_prefix_, url_safe_id, &kContentTypeCss, handler));

    bool do_rewrite_html = false;
    // If the combination has a Url set on it we have cached information
    // on what the output would be, so we'll just use that.
    if (combination->HasValidUrl()) {
      do_rewrite_html = true;
      html_parse_->InfoHere("Reusing existing CSS combination: %s",
                            combination->url().c_str());
    } else {
      // Otherwise, we have to compute it.
      do_rewrite_html = WriteCombination(partnership_->resources(),
                                         combination.get(), handler);
      do_rewrite_html = do_rewrite_html && combination->IsWritten();
    }

    // Update the DOM for new elements.
    if (do_rewrite_html) {
      combine_element->AddAttribute(s_href_, combination->url(), "\"");
      // TODO(sligocki): Put at top of head/flush-window.
      // Right now we're putting it where the first original element used to be.
      html_parse_->InsertElementBeforeElement(partnership_->element(0),
                                              combine_element);
      // ... and removing originals from the DOM.
      for (int i = 0; i < partnership_->num_urls(); ++i) {
        html_parse_->DeleteElement(partnership_->element(i));
      }
      html_parse_->InfoHere("Combined %d CSS files into one at %s",
                            partnership_->num_urls(),
                            combination->url().c_str());
      if (css_file_count_reduction_ != NULL) {
        css_file_count_reduction_->Add(partnership_->num_urls() - 1);
      }
    }
  }
  partnership_.reset(new Partnership(this, driver_, url_overhead_));
}

bool CssCombineFilter::WritePiece(Resource* input, OutputResource* combination,
                                  Writer* writer, MessageHandler* handler) {
  StringPiece contents = input->contents();
  std::string input_dir =
      GoogleUrl::AllExceptLeaf(GoogleUrl::Create(input->url()));
  if (input_dir == combination->resolved_base()) {
      // We don't need to absolutify URLs if input directory is same as output.
      return writer->Write(contents, handler);
  } else {
    // If they are different directories, we need to absolutify.
    // TODO(sligocki): Perhaps we should use the real CSS parser.
    return css_tag_scanner_.AbsolutifyUrls(contents, input->url(), writer,
                                           handler);
  }
}

}  // namespace net_instaweb
