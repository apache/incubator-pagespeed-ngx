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

// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/javascript_filter.h"

#include <cstddef>

#include <vector>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/charset_util.h"
#include "pagespeed/kernel/base/source_map.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

namespace {

void CleanupWhitespaceScriptBody(RewriteDriver* driver,
                                 HtmlCharactersNode* node) {
  // Note that an external script tag might contain body data.  We erase this
  // if it is just whitespace; otherwise we leave it alone.  The script body
  // is ignored by all browsers we know of.  However, various sources have
  // encouraged using the body of an external script element to store a
  // post-load callback.  As this technique is preferable to storing callbacks
  // in, say, html comments, we support it here.
  const GoogleString& contents = node->contents();
  for (size_t j = 0; j < contents.size(); ++j) {
    char c = contents[j];
    if (!IsHtmlSpace(c) && c != 0) {
      driver->InfoHere("Retaining contents of script tag;"
                       " probably data for external script.");
      return;
    }
  }
  bool deleted = driver->DeleteNode(node);
  DCHECK(deleted);
}

}  // namespace

JavascriptFilter::JavascriptFilter(RewriteDriver* driver)
    : RewriteFilter(driver),
      script_type_(kNoScript),
      some_missing_scripts_(false),
      script_tag_scanner_(driver) { }

JavascriptFilter::~JavascriptFilter() { }

void JavascriptFilter::InitStats(Statistics* statistics) {
  JavascriptRewriteConfig::InitStats(statistics);
}

class JavascriptFilter::Context : public SingleRewriteContext {
 public:
  Context(RewriteDriver* driver, RewriteContext* parent,
          JavascriptRewriteConfig* config, bool output_source_map)
      : SingleRewriteContext(driver, parent, NULL),
        config_(config),
        output_source_map_(output_source_map) {}

  // Rewriting JS actually produces 2 output resources. Rewritten JS and a
  // source map, but RewriteContext doesn't really know how to deal with one
  // input producing two outputs, so:
  // * If output_source_map == false -> output is the rewritten JS,
  // * If output_source_map == true  -> output is the source map.
  RewriteResult RewriteJavascript(
      const ResourcePtr& input, const OutputResourcePtr& output) {
    OutputResourcePtr rewritten, source_map;
    if (output_source_map_) {
      rewritten = Driver()->CreateOutputResourceFromResource(
          id(), encoder(), resource_context(), input, kind());
      source_map = output;
    } else {
      rewritten = output;
      source_map = Driver()->CreateOutputResourceFromResource(
          RewriteOptions::kJavascriptMinSourceMapId, encoder(),
          resource_context(), input, kRewrittenResource);
    }

    ServerContext* server_context = FindServerContext();
    MessageHandler* message_handler = server_context->message_handler();
    JavascriptCodeBlock code_block(
        input->contents(), config_, input->url(), message_handler);
    code_block.Rewrite();
    // Check whether this code should, for various reasons, not be rewritten.
    if (PossiblyRewriteToLibrary(code_block, server_context, rewritten)) {
      // Code was a library, so we will use the canonical url rather than create
      // an optimized version.
      // libraries_identified is incremented internally in
      // PossiblyRewriteToLibrary, so there's no specific failure metric here.
      return kRewriteFailed;
    }
    if (!config_->minify()) {
      config_->minification_disabled()->Add(1);
      return kRewriteFailed;
    }
    if (!code_block.successfully_rewritten()) {
      // Optimization happened but wasn't useful; the base class will remember
      // this for later so we don't attempt to rewrite twice.
      message_handler->Message(
          kInfo, "Script %s didn't shrink.", code_block.message_id().c_str());
      config_->did_not_shrink()->Add(1);
      return kRewriteFailed;
    }

    // Write out source map first so that we can embed the source map URL
    // into the rewritten version.
    if (Options()->Enabled(RewriteOptions::kIncludeJsSourceMaps) &&
        // Source map will be empty if we can't construct it correctly.
        !code_block.SourceMappings().empty()) {
      // Note: We append PageSpeed=off query parameter to make sure that
      // the source URL doesn't get rewritten with IPRO.
      GoogleUrl original_gurl(input->url());
      scoped_ptr<GoogleUrl> source_gurl(
          original_gurl.CopyAndAddEscapedQueryParam(RewriteQuery::kPageSpeed,
                                                    "off"));

      GoogleString source_map_text;
      // Note: We omit rewritten URL because of a chicken-and-egg problem.
      // rewritten URL depends on rewritten content, which depends on
      // source map URL, which depends on source map contents.
      // (So source map contents can't depend on rewritten URL!)
      source_map::Encode("" /* Omit rewritten URL */, source_gurl->Spec(),
                         code_block.SourceMappings(), &source_map_text);

      // TODO(sligocki): Perhaps we should not insert source maps into the
      // cache on every JS rewrite request because they will generally not
      // be used? Note that will make things more complicated because we
      // will have to generate the source map URL in some other way.
      if (WriteSourceMapTo(input, source_map_text, source_map)) {
        code_block.AppendSourceMapUrl(source_map->url());
      }
    }
    // Code block was optimized, so write out the new version.
    if (!WriteExternalScriptTo(
            input, code_block.rewritten_code(), server_context, rewritten)) {
      config_->failed_to_write()->Add(1);
      return kRewriteFailed;
    }
    // We only check and rule out introspective javascript *after* writing the
    // minified script because we might be performing AJAX rewriting, in which
    // case we'll rewrite without changing the url and can ignore introspection.
    // TODO(jmaessen): Figure out how to distinguish AJAX rewrites so that we
    // don't need the special control flow (and url_relocatable field in
    // cached_result and its treatment in rewrite_context).
    if (Options()->avoid_renaming_introspective_javascript() &&
        JavascriptCodeBlock::UnsafeToRename(code_block.rewritten_code())) {
      CachedResult* result = rewritten->EnsureCachedResultCreated();
      result->set_url_relocatable(false);
      message_handler->Message(
          kInfo, "Script %s is unsafe to replace.", input->url().c_str());
    }
    return kRewriteOk;
  }

 protected:
  // Implements the asynchronous interface required by SingleRewriteContext.
  //
  // TODO(jmarantz): this should be done as a SimpleTextFilter.
  virtual void RewriteSingle(
      const ResourcePtr& input, const OutputResourcePtr& output) {
    bool is_ipro = IsNestedIn(RewriteOptions::kInPlaceRewriteId);
    AttachDependentRequestTrace(is_ipro ? "IproProcessJs" : "ProcessJs");
    if (!IsDataUrl(input->url())) {
      TracePrintf("RewriteJs: %s", input->url().c_str());
    }
    RewriteDone(RewriteJavascript(input, output), 0);
  }

  virtual void Render() {
    if (num_output_partitions() != 1) {
      return;
    }
    CachedResult* result = output_partition(0);
    ResourceSlot* output_slot = slot(0).get();
    if (!result->optimizable()) {
      if (result->canonicalize_url() && output_slot->CanDirectSetUrl()) {
        // Use the canonical library url and disable the later render step.
        // This permits us to patch in a library url that doesn't correspond to
        // the OutputResource naming scheme.
        // Note that we can't direct set the url during AJAX rewriting, but we
        // have computed and cached the library match for any subsequent visit
        // to the page.
        output_slot->DirectSetUrl(result->url());
      }
      return;
    }
    // The url or script content is changing, so log that fact.
    Driver()->log_record()->SetRewriterLoggingStatus(
        id(), output_slot->resource()->url(), RewriterApplication::APPLIED_OK);
    config_->num_uses()->Add(1);
  }

  virtual OutputResourceKind kind() const { return kRewrittenResource; }

  virtual bool OptimizationOnly() const {
    if (output_source_map_) {
      return false;  // Do not return original JS as fallback for source maps!
    } else {
      return true;   // Do return original JS as fallback for rewritten JS.
    }
  }

  virtual const char* id() const {
    if (output_source_map_) {
      return RewriteOptions::kJavascriptMinSourceMapId;
    } else {
      return RewriteOptions::kJavascriptMinId;
    }
  }

 private:
  // Take script_out, which is derived from the script at script_url,
  // and write it to script_dest.
  // Returns true on success, reports failures itself.
  bool WriteExternalScriptTo(
      const ResourcePtr script_resource,
      StringPiece script_out, ServerContext* server_context,
      const OutputResourcePtr& script_dest) {
    bool ok = false;
    server_context->MergeNonCachingResponseHeaders(
        script_resource, script_dest);
    // Try to preserve original content type to avoid breaking upstream proxies
    // and the like.
    const ContentType* content_type = script_resource->type();
    if (content_type == NULL ||
        content_type->type() != ContentType::kJavascript) {
      content_type = &kContentTypeJavascript;
    }
    if (Driver()->Write(ResourceVector(1, script_resource),
                        script_out,
                        content_type,
                        script_resource->charset(),
                        script_dest.get())) {
      ok = true;
    }
    return ok;
  }

  bool WriteSourceMapTo(const ResourcePtr input_resource,
                        StringPiece contents,
                        const OutputResourcePtr& source_map) {
    source_map->response_headers()->Add(HttpAttributes::kXContentTypeOptions,
                                        HttpAttributes::kNosniff);
    source_map->response_headers()->Add(HttpAttributes::kContentDisposition,
                                        HttpAttributes::kAttachment);
    return Driver()->Write(ResourceVector(1, input_resource),
                           contents,
                           &kContentTypeSourceMap,
                           kUtf8Charset,
                           source_map.get());
  }

  // Decide if given code block is a JS library, and if so set up CachedResult
  // to reflect this fact.
  bool PossiblyRewriteToLibrary(
      const JavascriptCodeBlock& code_block, ServerContext* server_context,
      const OutputResourcePtr& output) {
    StringPiece library_url = code_block.ComputeJavascriptLibrary();
    if (library_url.empty()) {
      return false;
    }
    // We expect canonical urls to be protocol relative, and so we use the base
    // to provide a protocol when one is missing (while still permitting
    // absolute canonical urls when they are required).
    GoogleUrl library_gurl(Driver()->base_url(), library_url);
    server_context->message_handler()->Message(
        kInfo, "Canonical script %s is %s", code_block.message_id().c_str(),
        library_gurl.UncheckedSpec().as_string().c_str());
    if (!library_gurl.IsWebValid()) {
      return false;
    }
    // We remember the canonical url in the CachedResult in the metadata cache,
    // but don't actually write any kind of resource corresponding to the
    // rewritten file (since we don't need it).  This means we'll end up with a
    // CachedResult with a url() set, but none of the output resource metadata
    // such as a hash().  We set canonicalize_url to signal the Render() method
    // below to handle this case.  If it's useful for another filter, the logic
    // here can move up to RewriteContext::Propagate(...), but this ought to be
    // sufficient for a single filter-specific path.
    CachedResult* cached = output->EnsureCachedResultCreated();
    cached->set_url(library_gurl.Spec().data(),
                    library_gurl.Spec().size());
    cached->set_canonicalize_url(true);
    ResourceSlotPtr output_slot = slot(0);
    output_slot->set_disable_further_processing(true);
    return true;
  }

  JavascriptRewriteConfig* config_;
  bool output_source_map_;
};

void JavascriptFilter::StartElementImpl(HtmlElement* element) {
  DCHECK_EQ(kNoScript, script_type_);
  HtmlElement::Attribute* script_src;
  switch (script_tag_scanner_.ParseScriptElement(element, &script_src)) {
    case ScriptTagScanner::kJavaScript:
      if (script_src != NULL) {
        script_type_ = kExternalScript;
        RewriteExternalScript(element, script_src);
      } else {
        script_type_ = kInlineScript;
      }
      break;
    case ScriptTagScanner::kUnknownScript: {
      GoogleString script_dump;
      element->ToString(&script_dump);
      driver()->InfoHere("Unrecognized script:'%s'", script_dump.c_str());
      break;
    }
    case ScriptTagScanner::kNonScript:
      break;
  }
}

void JavascriptFilter::Characters(HtmlCharactersNode* characters) {
  switch (script_type_) {
    case kInlineScript:
      RewriteInlineScript(characters);
      break;
    case kExternalScript:
      CleanupWhitespaceScriptBody(driver(), characters);
      break;
    case kNoScript:
      break;
  }
}

JavascriptRewriteConfig* JavascriptFilter::InitializeConfig(
    RewriteDriver* driver) {
  return new JavascriptRewriteConfig(
                 driver->server_context()->statistics(),
                 driver->options()->Enabled(RewriteOptions::kRewriteJavascript),
                 driver->options()->use_experimental_js_minifier(),
                 driver->options()->javascript_library_identification(),
                 driver->server_context()->js_tokenizer_patterns());
}

void JavascriptFilter::InitializeConfigIfNecessary() {
  if (config_.get() == NULL) {
      config_.reset(InitializeConfig(driver()));
  }
}

void JavascriptFilter::RewriteInlineScript(HtmlCharactersNode* body_node) {
  // Log rewriter activity
  // First buffer up script data and minify it.
  GoogleString* script = body_node->mutable_contents();
  MessageHandler* message_handler = driver()->message_handler();
  JavascriptCodeBlock code_block(
      *script, config_.get(), driver()->UrlLine(), message_handler);
  code_block.Rewrite();
  StringPiece library_url = code_block.ComputeJavascriptLibrary();
  if (!library_url.empty()) {
    // TODO(jmaessen): outline and use canonical url.
    driver()->InfoHere("Script is inlined version of %s",
                       library_url.as_string().c_str());
  }
  if (code_block.successfully_rewritten()) {
    // Replace the old script string with the new, minified one.
    if ((driver()->MimeTypeXhtmlStatus() != RewriteDriver::kIsNotXhtml) &&
        (script->find("<![CDATA[") != StringPiece::npos) &&
        !code_block.rewritten_code().starts_with(
            "<![CDATA")) {  // See Issue 542.
      // Minifier strips leading and trailing CDATA comments from scripts.
      // Restore them if necessary and safe according to the original script.
      script->clear();
      StrAppend(script, "//<![CDATA[\n", code_block.rewritten_code(),
                "\n//]]>");
    } else {
      // Swap in the minified code to replace the original code.
      code_block.SwapRewrittenString(script);
      // Note: code_block and rewritten_script are INVALID after this point.
    }
    config_->num_uses()->Add(1);
    driver()->log_record()->SetRewriterLoggingStatus(
        id(), RewriterApplication::APPLIED_OK);
  } else {
    config_->did_not_shrink()->Add(1);
  }
}

// External script; minify and replace with rewritten version (also external).
void JavascriptFilter::RewriteExternalScript(
    HtmlElement* script_in_progress, HtmlElement::Attribute* script_src) {
  const StringPiece script_url(script_src->DecodedValueOrNull());
  ResourcePtr resource = CreateInputResource(script_url);
  if (resource.get() != NULL) {
    ResourceSlotPtr slot(
        driver()->GetSlot(resource, script_in_progress, script_src));
    if (driver()->options()->js_preserve_urls()) {
      slot->set_disable_rendering(true);
    }
    Context* jrc = new Context(driver(), NULL, config_.get(),
                               false /* output_source_map */);
    jrc->AddSlot(slot);
    driver()->InitiateRewrite(jrc);
  }
}

void JavascriptFilter::EndElementImpl(HtmlElement* element) {
  script_type_ = kNoScript;
}

void JavascriptFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  CHECK_EQ(kNoScript, script_type_);
  // We presume an IE directive is concealing some js code.
  some_missing_scripts_ = true;
}

RewriteContext* JavascriptFilter::MakeRewriteContext() {
  InitializeConfigIfNecessary();
  // A resource fetch.  This means a client has requested minified content;
  // we'll fail the request (serving the existing content) if minification is
  // disabled for this resource (eg because we've recognized it as a library).
  // This usually happens because the underlying JS content or rewrite
  // configuration changed since the client fetched a rewritten page.
  return new Context(driver(), NULL, config_.get(), output_source_map());
}

RewriteContext* JavascriptFilter::MakeNestedRewriteContext(
    RewriteContext* parent, const ResourceSlotPtr& slot) {
  InitializeConfigIfNecessary();
  // A nested rewrite, should work just like an HTML rewrite does.
  Context* context = new Context(NULL /* driver */, parent, config_.get(),
                                 output_source_map());
  context->AddSlot(slot);
  return context;
}

JavascriptSourceMapFilter::JavascriptSourceMapFilter(RewriteDriver* driver)
    : JavascriptFilter(driver) { }

JavascriptSourceMapFilter::~JavascriptSourceMapFilter() { }

}  // namespace net_instaweb
