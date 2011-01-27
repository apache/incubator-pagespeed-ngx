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

#include <ctype.h>
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/javascript_library_identification.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/atom.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/http/public/response_headers.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/url_escaper.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

namespace {
const HttpStatus::Code kNotOptimizable = HttpStatus::kNotModified;
}  // namespace


JavascriptFilter::JavascriptFilter(RewriteDriver* driver,
                                   const StringPiece& path_prefix)
    : RewriteSingleResourceFilter(driver, path_prefix),
      script_in_progress_(NULL),
      script_src_(NULL),
      some_missing_scripts_(false),
      config_(driver->resource_manager()->statistics()),
      s_script_(html_parse_->Intern("script")),
      s_src_(html_parse_->Intern("src")),
      s_type_(html_parse_->Intern("type")),
      script_tag_scanner_(html_parse_) { }

JavascriptFilter::~JavascriptFilter() { }

void JavascriptFilter::Initialize(Statistics* statistics) {
  JavascriptRewriteConfig::Initialize(statistics);
}

void JavascriptFilter::StartElementImpl(HtmlElement* element) {
  CHECK(script_in_progress_ == NULL);

  switch (script_tag_scanner_.ParseScriptElement(element, &script_src_)) {
    case ScriptTagScanner::kJavaScript: {
      script_in_progress_ = element;
      if (script_src_ != NULL) {
        html_parse_->InfoHere("Found script with src %s", script_src_->value());
      }
      break;
    }
    case ScriptTagScanner::kUnknownScript: {
      std::string script_dump;
      element->ToString(&script_dump);
      html_parse_->InfoHere("Unrecognized script:'%s'", script_dump.c_str());
      break;
    }
    case ScriptTagScanner::kNonScript:
      break;
  }
}

void JavascriptFilter::Characters(HtmlCharactersNode* characters) {
  if (script_in_progress_ != NULL) {
    // Note that we're keeping a vector of nodes here,
    // and appending them lazily at the end.  This is
    // because there's usually only 1 HtmlCharactersNode involved,
    // and we end up not actually needing to copy the string.
    buffer_.push_back(characters);
  }
}

// Flatten script fragments in buffer_, using script_buffer to hold
// the data if necessary.  Return a StringPiece referring to the data.
const StringPiece JavascriptFilter::FlattenBuffer(std::string* script_buffer) {
  const int buffer_size = buffer_.size();
  if (buffer_.size() == 1) {
    StringPiece result(buffer_[0]->contents());
    return result;
  } else {
    for (int i = 0; i < buffer_size; i++) {
      script_buffer->append(buffer_[i]->contents());
    }
    StringPiece result(*script_buffer);
    return result;
  }
}

void JavascriptFilter::RewriteInlineScript() {
  const int buffer_size = buffer_.size();
  if (buffer_size > 0) {
    // First buffer up script data and minify it.
    std::string script_buffer;
    const StringPiece script = FlattenBuffer(&script_buffer);
    MessageHandler* message_handler = html_parse_->message_handler();
    JavascriptCodeBlock code_block(script, &config_, message_handler);
    JavascriptLibraryId library = code_block.ComputeJavascriptLibrary();
    if (library.recognized()) {
      html_parse_->InfoHere("Script is %s %s",
                            library.name(), library.version());
    }
    if (code_block.ProfitableToRewrite()) {
      // Now replace all CharactersNodes with a single CharactersNode containing
      // the minified script.
      HtmlCharactersNode* new_script =
          html_parse_->NewCharactersNode(buffer_[0]->parent(),
                                         code_block.Rewritten());
      html_parse_->ReplaceNode(buffer_[0], new_script);
      for (int i = 1; i < buffer_size; i++) {
        html_parse_->DeleteElement(buffer_[i]);
      }
    }
  }
}

// Load script resource located at the given URL,
// on error report & return NULL (caller need not report)
Resource* JavascriptFilter::ScriptAtUrl(const StringPiece& script_url) {
  Resource* script_input = CreateInputResourceAndReadIfCached(script_url);
  return script_input;
}

// Take script_out, which is derived from the script at script_url,
// and write it to script_dest.
// Returns true on success, reports failures itself.
bool JavascriptFilter::WriteExternalScriptTo(
    const Resource* script_resource,
    const StringPiece& script_out, OutputResource* script_dest) {
  bool ok = false;
  MessageHandler* message_handler = html_parse_->message_handler();
  int64 origin_expire_time_ms = script_resource->CacheExpirationTimeMs();
  if (resource_manager_->Write(HttpStatus::kOK, script_out, script_dest,
                               origin_expire_time_ms, message_handler)) {
    ok = true;
    html_parse_->InfoHere("Rewrite script %s to %s",
                          script_resource->url().c_str(),
                          script_dest->url().c_str());
  }
  return ok;
}

// External script; minify and replace with rewritten version (also external).
void JavascriptFilter::RewriteExternalScript() {
  const StringPiece script_url(script_src_->value());
  MessageHandler* message_handler = html_parse_->message_handler();
  scoped_ptr<OutputResource> script_dest(
      resource_manager_->CreateOutputResourceForRewrittenUrl(
          base_gurl(), filter_prefix_, script_url,
          &kContentTypeJavascript, resource_manager_->url_escaper(),
          driver_->options(), message_handler));
  if (script_dest != NULL) {
    bool ok;
    if (resource_manager_->FetchOutputResource(
            script_dest.get(), NULL, NULL, message_handler,
            ResourceManager::kNeverBlock)) {
      // Only rewrite URL if we have usable rewritten data.
      ok = script_dest->metadata()->status_code() == HttpStatus::kOK;
    } else {
      scoped_ptr<Resource> script_input(ScriptAtUrl(script_url));
      ok = script_input != NULL;
      if (ok) {
        StringPiece script = script_input->contents();
        MessageHandler* message_handler = html_parse_->message_handler();
        JavascriptCodeBlock code_block(script, &config_, message_handler);
        JavascriptLibraryId library = code_block.ComputeJavascriptLibrary();
        if (library.recognized()) {
          html_parse_->InfoHere("Script %s is %s %s",
                                script_input->url().c_str(),
                                library.name(), library.version());
        }
        ok = code_block.ProfitableToRewrite();
        if (ok) {
          ok = WriteExternalScriptTo(script_input.get(), code_block.Rewritten(),
                                     script_dest.get());
        } else {
          // Rewriting happened but wasn't useful; remember this for later
          // so we don't attempt to rewrite twice.
          html_parse_->InfoHere("Script %s didn't shrink",
                                script_input->url().c_str());
          int64 origin_expire_time_ms = script_input->CacheExpirationTimeMs();

          // TODO(jmarantz): currently this will not work, because HTTPCache
          // will not report a 'hit' on any status other than OK.  This should
          // be fixed by either:
          //   1. adding a few other codes that HTTPCache will return hits for
          //   2. using a special header to indicate failed-to-optimize.
          resource_manager_->Write(
              kNotOptimizable, "",
              script_dest.get(), origin_expire_time_ms, message_handler);
        }
      } else {
        some_missing_scripts_ = true;
      }
    }
    if (ok) {
      script_src_->SetValue(script_dest->url());
    }
  }
  // Finally, note that the script might contain body data.
  // We erase this if it is just whitespace; otherwise we leave it alone.
  // The script body is ignored by all browsers we know of.
  // However, various sources have encouraged using the body of an
  // external script element to store a post-load callback.
  // As this technique is preferable to storing callbacks in, say, html
  // comments, we support it for now.
  bool allSpaces = true;
  for (size_t i = 0; allSpaces && i < buffer_.size(); ++i) {
    const std::string& contents = buffer_[i]->contents();
    for (size_t j = 0; allSpaces && j < contents.size(); ++j) {
      char c = contents[j];
      if (!isspace(c) && c != 0) {
        html_parse_->WarningHere("Retaining contents of script tag"
                                 " even though script is external.");
        allSpaces = false;
      }
    }
  }
  for (size_t i = 0; allSpaces && i < buffer_.size(); ++i) {
    html_parse_->DeleteElement(buffer_[i]);
  }
}

// Reset state at end of script.
void JavascriptFilter::CompleteScriptInProgress() {
  buffer_.clear();
  script_in_progress_ = NULL;
  script_src_ = NULL;
}

void JavascriptFilter::EndElementImpl(HtmlElement* element) {
  if (script_in_progress_ != NULL &&
      html_parse_->IsRewritable(script_in_progress_) &&
      html_parse_->IsRewritable(element)) {
    if (element->tag() == s_script_) {
      if (element->close_style() == HtmlElement::BRIEF_CLOSE) {
        html_parse_->ErrorHere("Brief close of script tag (non-portable)");
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
    html_parse_->InfoHere("Flush in mid-script; leaving script untouched.");
    CompleteScriptInProgress();
    some_missing_scripts_ = true;
  }
}

void JavascriptFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  CHECK(script_in_progress_ == NULL);
  // We presume an IE directive is concealing some js code.
  some_missing_scripts_ = true;
}

bool JavascriptFilter::RewriteLoadedResource(const Resource* script_input,
                                             OutputResource* output_resource) {
  StringPiece script = script_input->contents();
  std::string script_out;
  JavascriptCodeBlock code_block(script, &config_,
                                 html_parse_->message_handler());
  return WriteExternalScriptTo(script_input,
                               code_block.Rewritten(), output_resource);
}

}  // namespace net_instaweb
