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

// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/javascript_filter.h"

#include <ctype.h>
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/public/javascript_minification.h"
#include "net/instaweb/rewriter/public/input_resource.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/rewrite.pb.h"  // for ResourceUrl
#include "net/instaweb/util/public/atom.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/meta_data.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_async_fetcher.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

JavascriptFilter::JavascriptFilter(const StringPiece& path_prefix,
                                   HtmlParse* html_parse,
                                   ResourceManager* resource_manager)
    : RewriteFilter(path_prefix),
      html_parse_(html_parse),
      script_in_progress_(NULL),
      script_src_(NULL),
      resource_manager_(resource_manager),
      some_missing_scripts_(false),
      s_script_(html_parse->Intern("script")),
      s_src_(html_parse->Intern("src")) { }

JavascriptFilter::~JavascriptFilter() { }

void JavascriptFilter::StartElement(HtmlElement* element) {
  CHECK(script_in_progress_ == NULL);
  if (element->tag() == s_script_) {
    script_in_progress_ = element;
    if ((script_src_ = element->FindAttribute(s_src_)) != NULL) {
      html_parse_->InfoHere("Found script with src %s", script_src_->value());
    }
  }
}

void JavascriptFilter::Characters(HtmlCharactersNode* characters) {
  if (script_in_progress_ != NULL) {
    if (script_src_ == NULL) {
      // Note that we're keeping a vector of nodes here,
      // and appending them lazily at the end.  This is
      // because there's usually only 1 HtmlCharactersNode involved,
      // and we end up not actually needing to copy the string.
      buffer_.push_back(characters);
    } else {
      // A script with contents; they're ignored (TODO(jmaessen): Verify on IE).
      // Delete them.  Don't bother complaining if it's just whitespace.
      const std::string& contents = characters->contents();
      for (size_t i = 0; i < contents.size(); i++) {
        char c = contents[i];
        if (!isspace(c)) {
          html_parse_->ErrorHere("Dropping contents inside script with src");
          break;
        }
      }
      html_parse_->DeleteElement(characters);
    }
  }
}

void JavascriptFilter::RewriteInlineScript() {
  const int buffer_size = buffer_.size();
  if (buffer_size > 0) {
    // First buffer up script data and minify it.
    std::string script_buffer;
    const std::string* script = &script_buffer;
    if (buffer_.size() == 1) {
      // Just use the single characters node as script src.
      script = &(buffer_[0]->contents());
    } else {
      // Concatenate contents of all the characters nodes to
      // form the script src.
      for (int i = 0; i < buffer_size; i++) {
        script_buffer.append(buffer_[i]->contents());
      }
    }
    std::string script_out;
    MinifyJavascript(*script, &script_out);
    // Now replace all CharactersNodes with a single CharactersNode containing
    // the minified script.
    HtmlCharactersNode* new_script =
        html_parse_->NewCharactersNode(buffer_[0]->parent(), script_out);
    html_parse_->ReplaceNode(buffer_[0], new_script);
    for (int i = 1; i < buffer_size; i++) {
      html_parse_->DeleteElement(buffer_[i]);
    }
  }
}

// Load script resource located at the given URL,
// on error report & return NULL (caller need not report)
InputResource* JavascriptFilter::ScriptAtUrl(const std::string& script_url) {
  MessageHandler* message_handler = html_parse_->message_handler();
  InputResource* script_input =
      resource_manager_->CreateInputResource(script_url, message_handler);
  if (script_input == NULL ||
      !(script_input->Read(message_handler) &&
        script_input->ContentsValid())) {
    script_input = NULL;
    html_parse_->ErrorHere("Couldn't get external script %s",
                           script_url.c_str());
  }
  return script_input;
}

// Take script_out, which is derived from the script at script_url,
// and write it to script_dest.
// Returns true on success, reports failures itself.
bool JavascriptFilter::WriteExternalScriptTo(
    const std::string& script_url,
    const std::string& script_out, OutputResource* script_dest) {
  bool ok = false;
  MessageHandler* message_handler = html_parse_->message_handler();
  Writer* script_writer = script_dest->BeginWrite(message_handler);
  if (script_writer != NULL) {
    script_writer->Write(script_out, message_handler);
    if (script_dest->EndWrite(script_writer, message_handler)) {
      ok = true;
      html_parse_->InfoHere("Rewrite script %s to %s",
                            script_url.c_str(),
                            script_dest->url().c_str());
    } else {
      html_parse_->ErrorHere("Write failed for %s", script_url.c_str());
    }
  } else {
    html_parse_->ErrorHere("No writer for %s", script_url.c_str());
  }
  return ok;
}

// External script; minify and replace with rewritten version (also external).
void JavascriptFilter::RewriteExternalScript() {
  // External script; minify and replace with rewritten version.
  const std::string& script_url = script_src_->value();
  ResourceUrl rewritten_url_proto;
  rewritten_url_proto.set_origin_url(script_url);
  std::string rewritten_url;
  Encode(rewritten_url_proto, &rewritten_url);
  OutputResource* script_dest =
      resource_manager_->CreateNamedOutputResource(
          filter_prefix_, rewritten_url, kContentTypeJavascript);
  if (script_dest != NULL) {
    bool ok;
    if (script_dest->IsWritten()) {
      // Only rewrite URL if we have usable rewritten data.
      ok = script_dest->metadata()->status_code() == HttpStatus::OK;
    } else {
      InputResource* script_input = ScriptAtUrl(script_url);
      ok = script_input != NULL;
      if (ok) {
        const std::string& script = script_input->contents();
        std::string script_out;
        MinifyJavascript(script, &script_out);
        ok = script.size() > script_out.size();
        if (ok) {
          ok = WriteExternalScriptTo(script_url, script_out, script_dest);
        } else {
          // Rewriting happened but wasn't useful; remember this for later
          // so we don't attempt to rewrite twice.
          html_parse_->InfoHere("Script %s didn't shrink", script_url.c_str());
          script_dest->metadata()->set_status_code(
              HttpStatus::INTERNAL_SERVER_ERROR);
          MessageHandler* message_handler = html_parse_->message_handler();
          Writer* writer = script_dest->BeginWrite(message_handler);
          if (writer != NULL) {
            script_dest->EndWrite(writer, message_handler);
          }
        }
      } else {
        some_missing_scripts_ = true;
      }
    }
    if (ok) {
      script_src_->SetValue(script_dest->url());
    }
  } else {
    html_parse_->ErrorHere("Couldn't create new destination for %s",
                           script_url.c_str());
  }
}

// Reset state at end of script.
void JavascriptFilter::CompleteScriptInProgress() {
  if (script_src_ == NULL) {
    buffer_.clear();
  }
  script_in_progress_ = NULL;
  script_src_ = NULL;
}

void JavascriptFilter::EndElement(HtmlElement* element) {
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

void JavascriptFilter::IEDirective(const std::string& directive) {
  CHECK(script_in_progress_ = NULL);
  // We presume an IE directive is concealing some js code.
  some_missing_scripts_ = true;
}

bool JavascriptFilter::Fetch(OutputResource* output_resource,
                             Writer* writer,
                             const MetaData& request_header,
                             MetaData* response_headers,
                             UrlAsyncFetcher* fetcher,
                             MessageHandler* message_handler,
                             UrlAsyncFetcher::Callback* callback) {
  ResourceUrl url_proto;
  bool ok = false;
  if (Decode(output_resource->name(), &url_proto)) {
    const std::string& script_url = url_proto.origin_url();
    InputResource* script_input =
        resource_manager_->CreateInputResource(script_url, message_handler);
    if (script_input != NULL &&
        script_input->Read(message_handler) &&
        script_input->ContentsValid()) {
      const std::string& script = script_input->contents();
      std::string script_out;
      MinifyJavascript(script, &script_out);
      ok = WriteExternalScriptTo(script_url, script_out, output_resource);
    } else {
      message_handler->Error(output_resource->name().as_string().c_str(), 0,
                             "Could not load original source %s",
                             script_url.c_str());
    }
  } else {
    message_handler->Error(output_resource->name().as_string().c_str(), 0,
                           "Could not decode original js url");
  }
  return ok;
}

}  // namespace net_instaweb
