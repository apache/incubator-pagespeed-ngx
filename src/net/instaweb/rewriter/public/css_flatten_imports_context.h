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

// Author: matterbury@google.com (Matt Atterbury)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_FLATTEN_IMPORTS_CONTEXT_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_FLATTEN_IMPORTS_CONTEXT_H_

#include "base/logging.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/css_hierarchy.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"

namespace net_instaweb {

class RewriteContext;

// Context used by CssFilter under async flow that flattens @imports.
class CssFlattenImportsContext : public SingleRewriteContext {
 public:
  CssFlattenImportsContext(RewriteContext* parent,
                           CssFilter* filter,
                           CssFilter::Context* rewriter,
                           CssHierarchy* hierarchy)
      : SingleRewriteContext(NULL, parent, NULL /* no resource_context */),
        filter_(filter),
        rewriter_(rewriter),
        hierarchy_(hierarchy) {
  }
  virtual ~CssFlattenImportsContext() {}

  virtual GoogleString CacheKeySuffix() const {
    // We have to include the media that applies to this context in its key
    // so that, if someone @import's the same file but with a different set
    // of media on the @import rule, we don't fetch the cached file, since
    // it has been minified based on the original set of applicable media.
    GoogleString suffix;
    if (hierarchy_->media().empty()) {
      suffix = "all";
    } else {
      suffix = hierarchy_->media()[0];
      for (int i = 1, n = hierarchy_->media().size(); i < n; ++i) {
        StrAppend(&suffix, "_", hierarchy_->media()[i]);
      }
    }
    return suffix;
  }

  virtual void RewriteSingle(const ResourcePtr& input_resource,
                             const OutputResourcePtr& output_resource) {
    input_resource_ = input_resource;
    output_resource_ = output_resource;

    // We have to fix relative URLs in the CSS as they break if used in a
    // CSS file that itself was loaded via a relative path from the base
    // (for example, if styles/screen.css references ../images/icon.png, then
    // the correct path for the image is /images/icon.png). We also need to
    // absolutify or left-trim URLs in flattened CSS if no other rewriter is
    // going to do it (cache extend, css image rewriter, etc), but it's
    // hard to tell if that will happen so we transform URLs here regardless
    // and note that for CssHierarchy::css_resolution_base().
    RewriteDomainTransformer transformer(&hierarchy_->css_base_url(),
                                         &hierarchy_->css_trim_url(),
                                         Driver());
    // If we rewrite the input resource's contents we need somewhere to store
    // it; that's what the hierarchy's backing store is for.
    StringWriter writer(hierarchy_->input_contents_backing_store());
    // See RewriteDriver::ResolveCssUrls about why we disable trimming in
    // proxy mode. We also disable it if trimming is not enabled.
    if ( Driver()->server_context()->url_namer()->ProxyMode() ||
        !Driver()->options()->trim_urls_in_css() ||
        !Driver()->options()->Enabled(RewriteOptions::kLeftTrimUrls)) {
      transformer.set_trim_urls(false);
    }
    if (CssTagScanner::TransformUrls(input_resource_->contents(),
                                     &writer,
                                     &transformer,
                                     Driver()->message_handler())) {
      hierarchy_->set_input_contents_to_backing_store();
      hierarchy_->set_input_contents_resolved(true);
    } else {
      hierarchy_->set_input_contents(input_resource_->contents());
    }

    bool ok = true;
    GoogleString failure_reason;
    if (!hierarchy_->Parse()) {
      // If we cannot parse the CSS then we cannot flatten it.
      ok = false;
      failure_reason = StrCat("Cannot parse the CSS in ",
                              hierarchy_->url_for_humans());
      filter_->num_flatten_imports_minify_failed_->Add(1);
    } else if (!hierarchy_->CheckCharsetOk(input_resource, &failure_reason)) {
      ok = false;
      filter_->num_flatten_imports_charset_mismatch_->Add(1);
    } else {
      rewriter_->RewriteCssFromNested(this, hierarchy_);
    }

    if (!ok) {
      hierarchy_->set_flattening_succeeded(false);
      hierarchy_->AddFlatteningFailureReason(failure_reason);
      RewriteDone(kRewriteFailed, 0);
    } else if (num_nested() > 0) {
      StartNestedTasks();  // Initiates rewriting of @import'd files.
    } else {
      Harvest();  // Harvest centralizes all the output generation.
    }
  }

  void Harvest() {
    DCHECK_EQ(1, num_output_partitions());

    // Roll up the rewritten CSS(s) regardless of success or failure.
    // Failure means we can't flatten it for some reason, such as incompatible
    // charsets or invalid CSS, but we still need to cache the unflattened
    // version so we don't try to flatten it again and again, so even in that
    // case we don't return kRewriteFailed.
    hierarchy_->RollUpContents();

    // Our result is the combination of all our imports and our own rules.
    output_partition(0)->set_inlined_data(hierarchy_->minified_contents());

    ServerContext* server_context = FindServerContext();
    server_context->MergeNonCachingResponseHeaders(input_resource_,
                                                   output_resource_);
    if (Driver()->Write(ResourceVector(1, input_resource_),
                        hierarchy_->minified_contents(),
                        &kContentTypeCss,
                        input_resource_->charset(),
                        output_resource_.get())) {
      RewriteDone(kRewriteOk, 0);
    } else {
      RewriteDone(kRewriteFailed, 0);
    }
  }

  virtual void Render() {
    // If we have flattened the imported file ...
    if (num_output_partitions() == 1 && output_partition(0)->optimizable()) {
      // If Harvest() was called, directly or from RewriteSingle(), then the
      // minified contents are already set as are the stylesheet and input
      // contents - in that case we don't actually have to do anything. If
      // they haven't been called then the minified contents are empty and
      // the result was found in the cache, in which case we have to set the
      // input and minified contents to this result; the minified because we
      // know that cached values are minified (we only cache minified contents),
      // the input because we will need that to generate the stylesheet from
      // when RollUpStylesheets is eventually called.
      if (hierarchy_->minified_contents().empty()) {
        hierarchy_->set_minified_contents(
            output_partition(0)->inlined_data());
        hierarchy_->set_input_contents(hierarchy_->minified_contents());
      }
    } else {
      // Something has gone wrong earlier. It could be that the resource is
      // not valid and cacheable (see SingleRewriteContext::Partition) or it
      // could be that we're handling a cached failure, but it's hard to tell.
      // So, mark flattening as failed but don't record a failure statistic
      // nor a failure reason.
      hierarchy_->set_flattening_succeeded(false);
    }
  }

  virtual const char* id() const {
    return RewriteOptions::kCssImportFlattenerId;
  }
  virtual OutputResourceKind kind() const { return kRewrittenResource; }

 private:
  CssFilter* filter_;
  CssFilter::Context* rewriter_;
  CssHierarchy* hierarchy_;
  ResourcePtr input_resource_;
  OutputResourcePtr output_resource_;
  DISALLOW_COPY_AND_ASSIGN(CssFlattenImportsContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_FLATTEN_IMPORTS_CONTEXT_H_
