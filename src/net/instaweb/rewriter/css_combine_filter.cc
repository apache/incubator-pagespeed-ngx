/*
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
//
// Contains implementation of CssCombineFilter, which concatenates multiple
// CSS files into one. Implemented in part via delegating to
// CssCombineFilter::CssCombiner, a ResourceCombiner subclass.

#include "net/instaweb/rewriter/public/css_combine_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_combiner.h"
#include "net/instaweb/rewriter/public/resource_combiner_template.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {
class MessageHandler;
class RequestHeaders;
class ResponseHeaders;
class HtmlIEDirectiveNode;

namespace {

// names for Statistics variables.
const char kCssFileCountReduction[] = "css_file_count_reduction";

}  // namespace

// Combining helper. Takes care of checking that media matches, that we do not
// produce @import's in the middle and of URL absolutification.
class CssCombineFilter::CssCombiner
    : public ResourceCombinerTemplate<HtmlElement*> {
 public:
  CssCombiner(RewriteDriver* driver, const StringPiece& filter_prefix,
              CssTagScanner* css_tag_scanner, CssCombineFilter *filter)
      : ResourceCombinerTemplate<HtmlElement*>(
          driver, filter_prefix, kContentTypeCss.file_extension() + 1,
          filter),
        css_tag_scanner_(css_tag_scanner),
        css_file_count_reduction_(NULL) {
    filter_prefix.CopyToString(&filter_prefix_);
    Statistics* stats = resource_manager_->statistics();
    if (stats != NULL) {
      css_file_count_reduction_ = stats->GetVariable(kCssFileCountReduction);
    }
  }

  virtual bool ResourceCombinable(Resource* resource, MessageHandler* handler) {
    // styles containing @import cannot be appended to others, as any
    // @import in the middle will be ignored.
    return ((num_urls() == 0)
            || !CssTagScanner::HasImport(resource->contents(), handler));
  }

  virtual bool AddElementWithMedia(
      HtmlElement* element, const StringPiece& href,
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
    return AddElement(element, href, handler).value;
  }

  // Try to combine all the CSS files we have seen so far.
  // Insert the combined resource where the first original CSS link was.
  void TryCombineAccumulated();

 private:
  virtual bool WritePiece(const Resource* input, OutputResource* combination,
                          Writer* writer, MessageHandler* handler);

  // Returns true iff all elements in current combination can be rewritten.
  bool CanRewrite() const {
    bool ret = (num_urls() > 0);
    for (int i = 0; ret && (i < num_urls()); ++i) {
      ret = rewrite_driver_->IsRewritable(element(i));
    }
    return ret;
  }

  GoogleString media_;
  GoogleString filter_prefix_;
  CssTagScanner* css_tag_scanner_;
  Variable* css_file_count_reduction_;
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
    : RewriteFilter(driver, filter_prefix),
      css_tag_scanner_(driver_) {
  combiner_.reset(new CssCombiner(driver_, filter_prefix, &css_tag_scanner_,
                                  this));
}

CssCombineFilter::~CssCombineFilter() {
}

void CssCombineFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kCssFileCountReduction);
}

void CssCombineFilter::StartDocumentImpl() {
}

void CssCombineFilter::StartElementImpl(HtmlElement* element) {
  HtmlElement::Attribute* href;
  const char* media;
  if (!driver_->HasChildrenInFlushWindow(element) &&
      css_tag_scanner_.ParseCssElement(element, &href, &media)) {
    // We cannot combine with a link in <noscript> tag and we cannot combine
    // over a link in a <noscript> tag, so this is a barrier.
    if (noscript_element() != NULL) {
      combiner_->TryCombineAccumulated();
    } else {
      const char* url = href->value();
      MessageHandler* handler = driver_->message_handler();

      if (!combiner_->AddElementWithMedia(element, url, media, handler)) {
        // This element can't be included in the previous combination,
        // so try to flush out what we have.
        combiner_->TryCombineAccumulated();

        // Now we'll try to start a new partnership with this CSS file --
        // perhaps we ran out out of space in the previous combination
        // or this file is simply in a different authorized domain, or
        // contained @Import.
        //
        // Note that it's OK if this fails; we will simply not rewrite
        // the element in that case
        combiner_->AddElementWithMedia(element, url, media, handler);
      }
    }
  } else if (element->keyword() == HtmlName::kStyle) {
    // We can't reorder styles on a page, so if we are only combining <link>
    // tags, we can't combine them across a <style> tag.
    // TODO(sligocki): Maybe we should just combine <style>s too?
    // We can run outline_css first for now to make all <style>s into <link>s.
    combiner_->TryCombineAccumulated();
  }
}

// An IE directive that includes any stylesheet info should be a barrier
// for css combining.  It's OK to emit the combination we've seen so far.
void CssCombineFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  // TODO(sligocki): Figure out how to safely parse IEDirectives, for now we
  // just consider them black boxes / solid barriers.
  combiner_->TryCombineAccumulated();
}

void CssCombineFilter::Flush() {
  combiner_->TryCombineAccumulated();
}

void CssCombineFilter::CssCombiner::TryCombineAccumulated() {
  if (CanRewrite()) {
    MessageHandler* handler = rewrite_driver_->message_handler();
    OutputResourcePtr combination(Combine(kContentTypeCss, handler));
    if (combination.get() != NULL) {
      // Ideally like to have a data-driven service tell us which elements
      // should be combined together.  Note that both the resources and the
      // elements are managed, so we don't delete them even if the spriting
      // fails.

      HtmlElement* combine_element =
          rewrite_driver_->NewElement(NULL, HtmlName::kLink);
      rewrite_driver_->AddAttribute(
          combine_element, HtmlName::kRel, "stylesheet");
      rewrite_driver_->AddAttribute(combine_element, HtmlName::kType,
                                    kContentTypeCss.mime_type());
      if (!media_.empty()) {
        rewrite_driver_->AddAttribute(
            combine_element, HtmlName::kMedia, media_);
      }

      rewrite_driver_->AddAttribute(combine_element, HtmlName::kHref,
                                    combination->url());
      // TODO(sligocki): Put at top of head/flush-window.
      // Right now we're putting it where the first original element used to be.
      rewrite_driver_->InsertElementBeforeElement(element(0),
                                                  combine_element);
      // ... and removing originals from the DOM.
      for (int i = 0; i < num_urls(); ++i) {
        rewrite_driver_->DeleteElement(element(i));
      }
      rewrite_driver_->InfoHere("Combined %d CSS files into one at %s",
                                num_urls(), combination->url().c_str());
      if (css_file_count_reduction_ != NULL) {
        css_file_count_reduction_->Add(num_urls() - 1);
      }
    }
  }
  Reset();
}

bool CssCombineFilter::CssCombiner::WritePiece(
    const Resource* input, OutputResource* combination, Writer* writer,
    MessageHandler* handler) {
  StringPiece contents = input->contents();
  GoogleUrl input_url(input->url());
  StringPiece input_dir = input_url.AllExceptLeaf();
  if (input_dir == combination->resolved_base()) {
      // We don't need to absolutify URLs if input directory is same as output.
      return writer->Write(contents, handler);
  } else {
    // If they are different directories, we need to absolutify.
    // TODO(sligocki): Perhaps we should use the real CSS parser.
    return css_tag_scanner_->AbsolutifyUrls(contents, input->url(), writer,
                                            handler);
  }
}

bool CssCombineFilter::Fetch(const OutputResourcePtr& resource,
                             Writer* writer,
                             const RequestHeaders& request_header,
                             ResponseHeaders* response_headers,
                             MessageHandler* message_handler,
                             UrlAsyncFetcher::Callback* callback) {
  return combiner_->Fetch(resource, writer, request_header, response_headers,
                          message_handler, callback);
}

}  // namespace net_instaweb
