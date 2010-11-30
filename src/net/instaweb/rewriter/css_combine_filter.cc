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
#include "net/instaweb/rewriter/public/url_partnership.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/url_escaper.h"
#include "net/instaweb/util/public/url_multipart_encoder.h"

namespace net_instaweb {

namespace {

// names for Statistics variables.
const char kCssFileCountReduction[] = "css_file_count_reduction";

// TODO(jmarantz): This is arguably fragile.
//
// Another approach is to put a CHECK that the final URL with the
// resource naming does not exceed the limit.
//
// Another option too is to just instantiate a ResourceNamer and a
// hasher put in the correct ID and EXT and leave the name blank and
// take size of that.
const int kIdOverhead = 2;   // strlen("cc")
const int kExtOverhead = 3;  // strlen("css")
const int kUrlOverhead = kIdOverhead + ResourceNamer::kOverhead + kExtOverhead;

}  // namespace

class CssCombineFilter::Partnership : public UrlPartnership {
 public:
  Partnership(ResourceManager* resource_manager, const GURL& gurl)
      : UrlPartnership(resource_manager->domain_lawyer(), gurl),
        resource_manager_(resource_manager),
        prev_num_components_(0),
        accumulated_leaf_size_(0) {
  }

  virtual ~Partnership() {
    STLDeleteElements(&resources_);
  }

  bool AddElement(HtmlElement* element, const StringPiece& href,
                  const StringPiece& media, scoped_ptr<Resource>* resource,
                  MessageHandler* handler) {
    // Assert the sanity of three parallel vectors.
    CHECK_EQ(num_urls(), static_cast<int>(resources_.size()));
    CHECK_EQ(num_urls(), static_cast<int>(css_elements_.size()));
    CHECK_EQ(num_urls(), static_cast<int>(multipart_encoder_.num_urls()));

    bool added = true;
    if (num_urls() == 0) {
      // TODO(jmarantz): do media='' and media='display mean the same
      // thing?  sligocki thinks mdsteele looked into this and it
      // depended on HTML version.  In one display was default, in the
      // other screen was IIRC.
      media.CopyToString(&media_);
    } else {
      // After the first CSS file, subsequent CSS files must have matching
      // media and no @import tags.
      added = ((media_ == media) &&
               !CssTagScanner::HasImport((*resource)->contents(), handler));
    }
    if (added) {
      added = AddUrl(href, handler);
    }
    if (added) {
      int index = num_urls() - 1;
      CHECK_EQ(index, static_cast<int>(css_elements_.size()));

      if (num_components() != prev_num_components_) {
        UpdateResolvedBase();
      }
      const std::string relative_path = RelativePath(index);
      multipart_encoder_.AddUrl(relative_path);

      if (accumulated_leaf_size_ == 0) {
        ComputeLeafSize();
      } else {
        AccumulateLeafSize(relative_path);
      }

      if (UrlTooBig()) {
        added = false;
        RemoveLast();
        multipart_encoder_.pop_back();
      } else {
        css_elements_.push_back(element);
        resources_.push_back(resource->release());
      }
    }
    return added;
  }

  // Computes a name for the URL that meets all known character-set and
  // size restrictions.
  std::string UrlSafeId() const {
    UrlEscaper* escaper = resource_manager_->url_escaper();
    std::string segment;
    escaper->EncodeToUrlSegment(multipart_encoder_.Encode(), &segment);
    return segment;
  }

  // Computes the total size
  void ComputeLeafSize() {
    std::string segment = UrlSafeId();
    // TODO(sligocki): Use hasher for custom overhead.
    accumulated_leaf_size_ = segment.size() + kUrlOverhead
        + resource_manager_->hasher()->HashSizeInChars();
  }

  // Incrementally updates the accumulated leaf size without re-examining
  // every element in the combined css file.
  void AccumulateLeafSize(const StringPiece& url) {
    std::string segment;
    UrlEscaper* escaper = resource_manager_->url_escaper();
    escaper->EncodeToUrlSegment(url, &segment);
    const int kMultipartOverhead = 1;  // for the '+'
    accumulated_leaf_size_ += segment.size() + kMultipartOverhead;
  }

  // Determines whether our accumulated leaf size is too big, taking into
  // account both per-segment and total-url limitations.
  bool UrlTooBig() {
    if (accumulated_leaf_size_ > resource_manager_->max_url_segment_size()) {
      return true;
    }
    if ((accumulated_leaf_size_ + static_cast<int>(resolved_base_.size())) >
        resource_manager_->max_url_size()) {
      return true;
    }
    return false;
  }

  void UpdateResolvedBase() {
    // If the addition of this URL changes the base path,
    // then we will have to recompute the multi-part encoding.
    // This is n^2 in the pathalogical case and if this code
    // is copied from CSS combining to image spriting then this
    // algorithm should be revisited.  For CSS we expect N to
    // be relatively small.
    prev_num_components_ = num_components();
    resolved_base_ = ResolvedBase();
    multipart_encoder_.clear();
    for (size_t i = 0; i < css_elements_.size(); ++i) {
      multipart_encoder_.AddUrl(RelativePath(i));
    }

    accumulated_leaf_size_ = 0;
  }

  HtmlElement* element(int i) { return css_elements_[i]; }
  const ResourceVector& resources() const { return resources_; }
  const std::string& media() const { return media_; }

 private:
  ResourceManager* resource_manager_;
  std::vector<HtmlElement*> css_elements_;
  std::vector<Resource*> resources_;
  UrlMultipartEncoder multipart_encoder_;
  int prev_num_components_;
  int accumulated_leaf_size_;
  std::string resolved_base_;
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

CssCombineFilter::CssCombineFilter(RewriteDriver* driver,
                                   const char* filter_prefix)
    : RewriteFilter(driver, filter_prefix),
      html_parse_(driver->html_parse()),
      resource_manager_(driver->resource_manager()),
      css_tag_scanner_(html_parse_),
      css_file_count_reduction_(NULL) {
  // This CHECK is here because RewriteDriver is constructed with it's
  // resource_manager_ == NULL.
  // TODO(sligocki): Construct RewriteDriver with a ResourceManager.
  CHECK(resource_manager_ != NULL);
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

void CssCombineFilter::StartDocumentImpl() {
  // This should already be clear, but just in case.
  partnership_.reset(new Partnership(resource_manager_, base_gurl()));
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
      scoped_ptr<Resource> resource (resource_manager_->CreateInputResource(
          base_gurl(), url, handler));
      if (resource.get() == NULL) {
        TryCombineAccumulated();
      } else {
        if (!resource_manager_->ReadIfCached(resource.get(), handler) ||
            !resource->ContentsValid()) {
          TryCombineAccumulated();
        } else if (!partnership_->AddElement(element, url, media, &resource,
                                             handler)) {
          TryCombineAccumulated();

          // Now we'll try to start a new partnership with this CSS file --
          // perhaps we ran out out of space in the previous combination
          // or this file is simply in a different authorized domain, or
          // contained @Import.
          partnership_->AddElement(element, url, media, &resource, handler);
        }
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
        resource_manager_->CreateOutputResourceWithPath(
            partnership_->ResolvedBase(),
            filter_prefix_, url_safe_id, &kContentTypeCss, handler));
    bool written = combination->IsWritten() ||
        WriteCombination(partnership_->resources(), combination.get(), handler);

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
  partnership_.reset(new Partnership(resource_manager_, base_gurl()));
}

bool CssCombineFilter::WriteCombination(const ResourceVector& combine_resources,
                                        OutputResource* combination,
                                        MessageHandler* handler) {
  bool written = true;
  // TODO(sligocki): Write directly to a temp file rather than doing the extra
  // string copy.
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

    std::string input_dir =
        GoogleUrl::AllExceptLeaf(GoogleUrl::Create(input->url()));
    if (input_dir == combination->resolved_base()) {
      // We don't need to absolutify URLs if input directory is same as output.
      written = writer.Write(contents, handler);
    } else {
      // If they are different directories, we need to absolutify.
      // TODO(sligocki): Perhaps we should use the real CSS parser.
      written = css_tag_scanner_.AbsolutifyUrls(contents, input->url(),
                                                &writer, handler);
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

  // Note that the passed-in resource might be NULL; this gives us a chance
  // to note failure.
  bool AddResource(Resource* resource) {
    bool ret = false;
    if (resource == NULL) {
      // Whoops, we've failed to even obtain a resource.
      ++fail_count_;
    } else if (fail_count_ > 0) {
      // Another of the resource fetches failed.  Drop this resource
      // and don't fetch it.
      delete resource;
    } else {
      combine_resources_.push_back(resource);
      ret = true;
    }
    return ret;
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
      ok = (filter_->WriteCombination(combine_resources_, combination_,
                                      message_handler_) &&
            combination_->IsWritten() &&
            ((writer_ == NULL) ||
             writer_->Write(combination_->contents(), message_handler_)));
      // Above code fills in combination_->metadata(); now propagate to
      // repsonse_headers_.
    }
    if (ok) {
      response_headers_->CopyFrom(*combination_->metadata());
      callback_->Done(ok);
    } else {
      response_headers_->SetStatusAndReason(HttpStatus::kNotFound);
      // We assume that on failure the callback will be invoked by
      // RewriteDriver::FetchResource.  Since callbacks are self-deleting,
      // calling callback_->Done(false) here first would cause seg faults.
    }
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
                             MessageHandler* message_handler,
                             UrlAsyncFetcher::Callback* callback) {
  CHECK(response_headers != NULL);
  bool ret = false;
  StringPiece url_safe_id = combination->name();
  UrlMultipartEncoder multipart_encoder;
  UrlEscaper* escaper = resource_manager_->url_escaper();
  std::string multipart_encoding;
  GURL gurl(combination->url());
  if (gurl.is_valid() &&
      escaper->DecodeFromUrlSegment(url_safe_id, &multipart_encoding) &&
      multipart_encoder.Decode(multipart_encoding, message_handler)) {
    std::string url, decoded_resource;
    ret = true;
    CssCombiner* combiner = new CssCombiner(
        this, message_handler, callback, combination, writer, response_headers);

    std::string root = GoogleUrl::AllExceptLeaf(gurl);
    for (int i = 0; ret && (i < multipart_encoder.num_urls()); ++i)  {
      std::string url = StrCat(root, multipart_encoder.url(i));
      Resource* css_resource =
          resource_manager_->CreateInputResourceAbsolute(url, message_handler);
      ret = combiner->AddResource(css_resource);
      if (ret) {
        resource_manager_->ReadAsync(css_resource, combiner, message_handler);
      }
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
