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

#include <cctype>
#include <cstddef>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/doctype.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/javascript_library_identification.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

void CleanupWhitespaceScriptBody(
    RewriteDriver* driver, RewriteContext* context, HtmlCharactersNode* node) {
  if (node != NULL) {
    // Note that an external script tag might contain body data.  We erase this
    // if it is just whitespace; otherwise we leave it alone.  The script body
    // is ignored by all browsers we know of.  However, various sources have
    // encouraged using the body of an external script element to store a
    // post-load callback.  As this technique is preferable to storing callbacks
    // in, say, html comments, we support it here.
    const GoogleString& contents = node->contents();
    for (size_t j = 0; j < contents.size(); ++j) {
      char c = contents[j];
      if (!isspace(c) && c != 0) {
        driver->InfoAt(context, "Retaining contents of script tag;"
                       " probably data for external script.");
        return;
      }
    }
    driver->DeleteElement(node);
  }
}

}  // namespace

class RewriteContext;
class Statistics;
JavascriptFilter::JavascriptFilter(RewriteDriver* driver)
    : RewriteFilter(driver),
      body_node_(NULL),
      script_in_progress_(NULL),
      script_src_(NULL),
      some_missing_scripts_(false),
      config_(driver->resource_manager()->statistics()),
      script_tag_scanner_(driver_) { }

JavascriptFilter::~JavascriptFilter() { }

void JavascriptFilter::Initialize(Statistics* statistics) {
  JavascriptRewriteConfig::Initialize(statistics);
}

class JavascriptFilter::Context : public SingleRewriteContext {
 public:
  Context(RewriteDriver* driver, RewriteContext* parent,
          JavascriptRewriteConfig* config,
          HtmlCharactersNode* body_node)
      : SingleRewriteContext(driver, parent, NULL),
        config_(config),
        body_node_(body_node) {
  }

  RewriteResult RewriteJavascript(
      const ResourcePtr& input, const OutputResourcePtr& output) {
    MessageHandler* message_handler = Manager()->message_handler();
    StringPiece script = input->contents();
    JavascriptCodeBlock code_block(script, config_, input->url(),
                                   message_handler);
    JavascriptLibraryId library = code_block.ComputeJavascriptLibrary();
    if (library.recognized()) {
      message_handler->Message(kInfo, "Script %s is %s %s",
                               input->url().c_str(),
                               library.name(), library.version());
    }

    bool ok = code_block.ProfitableToRewrite();
    if (ok) {
      // Give the script a nice mimetype and extension.
      // (There is no harm in doing this, they're ignored anyway).
      output->SetType(&kContentTypeJavascript);
      ok = WriteExternalScriptTo(input, code_block.Rewritten(), output);
    } else {
      // Rewriting happened but wasn't useful; as we return false base class
      // will remember this for later so we don't attempt to rewrite twice.
      message_handler->Message(kInfo, "Script %s didn't shrink",
                               input->url().c_str());
    }
    return ok ? kRewriteOk : kRewriteFailed;
  }

 protected:
  // Implements the asynchronous interface required by SingleRewriteContext.
  //
  // TODO(jmarantz): this should be done as a SimpleTextFilter.
  virtual void RewriteSingle(
      const ResourcePtr& input, const OutputResourcePtr& output) {
    RewriteDone(RewriteJavascript(input, output), 0);
  }

  virtual void Render() {
    CleanupWhitespaceScriptBody(Driver(), this, body_node_);
  }

  virtual OutputResourceKind kind() const { return kRewrittenResource; }

  virtual const char* id() const { return RewriteOptions::kJavascriptMinId; }

 private:
  // Take script_out, which is derived from the script at script_url,
  // and write it to script_dest.
  // Returns true on success, reports failures itself.
  bool WriteExternalScriptTo(
      const ResourcePtr script_resource,
      const StringPiece& script_out, const OutputResourcePtr& script_dest) {
    bool ok = false;
    ResourceManager* resource_manager = Manager();
    MessageHandler* message_handler = resource_manager->message_handler();
    resource_manager->MergeNonCachingResponseHeaders(
        script_resource, script_dest);
    if (resource_manager->Write(HttpStatus::kOK, script_out, script_dest.get(),
                                message_handler)) {
      ok = true;
      message_handler->Message(kInfo, "Rewrite script %s to %s",
                               script_resource->url().c_str(),
                               script_dest->url().c_str());
    }
    return ok;
  }

  JavascriptRewriteConfig* config_;

  // The node containing the body of the script tag, or NULL.
  HtmlCharactersNode* body_node_;
};

void JavascriptFilter::StartElementImpl(HtmlElement* element) {
  // These ought to be invariants.  If they're not, we may leak
  // memory and/or fail to optimize, but it's not a disaster.
  DCHECK(script_in_progress_ == NULL);
  DCHECK(body_node_ == NULL);

  switch (script_tag_scanner_.ParseScriptElement(element, &script_src_)) {
    case ScriptTagScanner::kJavaScript:
      script_in_progress_ = element;
      if (script_src_ != NULL) {
        driver_->InfoHere("Found script with src %s", script_src_->value());
      }
      break;
    case ScriptTagScanner::kUnknownScript: {
      GoogleString script_dump;
      element->ToString(&script_dump);
      driver_->InfoHere("Unrecognized script:'%s'", script_dump.c_str());
      break;
    }
    case ScriptTagScanner::kNonScript:
      break;
  }
}

void JavascriptFilter::Characters(HtmlCharactersNode* characters) {
  if (script_in_progress_ != NULL) {
    // Save a reference to characters encountered in the script body.
    body_node_ = characters;
  }
}

void JavascriptFilter::RewriteInlineScript() {
  if (body_node_ != NULL) {
    // First buffer up script data and minify it.
    GoogleString* script = body_node_->mutable_contents();
    MessageHandler* message_handler = driver_->message_handler();
    JavascriptCodeBlock code_block(*script, &config_, driver_->UrlLine(),
                                   message_handler);
    JavascriptLibraryId library = code_block.ComputeJavascriptLibrary();
    if (library.recognized()) {
      driver_->InfoHere("Script is %s %s",
                        library.name(), library.version());
    }
    if (code_block.ProfitableToRewrite()) {
      // Replace the old script string with the new, minified one.
      GoogleString* rewritten_script = code_block.RewrittenString();
      if (driver_->doctype().IsXhtml() &&
          script->find("<![CDATA[") != StringPiece::npos) {
        // Minifier strips leading and trailing CDATA comments from scripts.
        // Restore them if necessary and safe according to the original script.
        script->clear();
        StrAppend(script, "//<![CDATA[\n", *rewritten_script, "\n//]]>");
      } else {
        // Swap in the minified code to replace the original code.
        script->swap(*rewritten_script);
      }
    }
  }
}

// External script; minify and replace with rewritten version (also external).
void JavascriptFilter::RewriteExternalScript() {
  const StringPiece script_url(script_src_->value());
  ResourcePtr resource = CreateInputResource(script_url);
  if (resource.get() != NULL) {
    ResourceSlotPtr slot(
        driver_->GetSlot(resource, script_in_progress_, script_src_));
    Context* jrc = new Context(driver_, NULL, &config_, body_node_);
    jrc->AddSlot(slot);
    driver_->InitiateRewrite(jrc);
  }
}

// Reset state at end of script.
void JavascriptFilter::CompleteScriptInProgress() {
  body_node_ = NULL;
  script_in_progress_ = NULL;
  script_src_ = NULL;
}

void JavascriptFilter::EndElementImpl(HtmlElement* element) {
  if (script_in_progress_ != NULL &&
      driver_->IsRewritable(script_in_progress_) &&
      driver_->IsRewritable(element)) {
    if (element->keyword() == HtmlName::kScript) {
      if (element->close_style() == HtmlElement::BRIEF_CLOSE) {
        driver_->ErrorHere("Brief close of script tag (non-portable)");
      }
      if (script_src_ == NULL) {
        RewriteInlineScript();
      } else {
        RewriteExternalScript();
      }
      CompleteScriptInProgress();
    } else {
      // Should not happen by construction (parser should not have tags here).
      // Note that if we get here, this test *Will* fail; it is written
      // out longhand to make diagnosis easier.
      CHECK(script_in_progress_ == NULL);
    }
  }
}

void JavascriptFilter::Flush() {
  // TODO(jmaessen): We can be smarter here if it turns out to be necessary (eg
  // by buffering an in-progress script across the flush boundary).
  if (script_in_progress_ != NULL) {
    // Not actually an error!
    driver_->InfoHere("Flush in mid-script; leaving script untouched.");
    CompleteScriptInProgress();
    some_missing_scripts_ = true;
  }
}

void JavascriptFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  CHECK(script_in_progress_ == NULL);
  // We presume an IE directive is concealing some js code.
  some_missing_scripts_ = true;
}

RewriteResult JavascriptFilter::RewriteLoadedResource(
    const ResourcePtr& script_input,
    const OutputResourcePtr& output_resource) {
  // Temporary code so that we can share the rewriting implementation beteween
  // the old blocking rewrite model and the new async model.
  Context jrc(driver_, NULL, &config_, body_node_);
  return jrc.RewriteJavascript(script_input, output_resource);
}

RewriteContext* JavascriptFilter::MakeRewriteContext() {
  return new Context(driver_, NULL, &config_, NULL /* no body node */);
}

RewriteContext* JavascriptFilter::MakeNestedRewriteContext(
    RewriteContext* parent, const ResourceSlotPtr& slot) {
  Context* context = new Context(NULL /* driver */, parent, &config_,
                                 NULL /* no body node */);
  context->AddSlot(slot);
  return context;
}

}  // namespace net_instaweb
