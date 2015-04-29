/*
 * Copyright 2014 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_absolutify.h"

#include <cstddef>
#include <vector>

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/http/google_url.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/parser.h"
#include "webutil/css/property.h"
#include "webutil/css/selector.h"
#include "webutil/css/value.h"

namespace net_instaweb {

// TODO(sligocki): Move this into AbsolutifyUrls(). We don't ever want
// to use them separately. If we want to absolutify URLs, we want to
// absolutify them all.
bool CssAbsolutify::AbsolutifyImports(Css::Stylesheet* stylesheet,
                                      const GoogleUrl& base) {
  bool urls_modified = false;
  const Css::Imports& imports = stylesheet->imports();
  Css::Imports::const_iterator iter;
  for (iter = imports.begin(); iter != imports.end(); ++iter) {
    Css::Import* import = *iter;
    StringPiece url(import->link().utf8_data(), import->link().utf8_length());
    GoogleUrl gurl(base, url);
    if (gurl.IsWebValid() && gurl.Spec() != url) {
      url = gurl.Spec();
      import->set_link(UTF8ToUnicodeText(url.data(), url.length()));
      urls_modified = true;
    }
  }
  return urls_modified;
}

bool CssAbsolutify::AbsolutifyUrls(Css::Stylesheet* stylesheet,
                                   const GoogleUrl& base,
                                   bool handle_parseable_ruleset_sections,
                                   bool handle_unparseable_sections,
                                   RewriteDriver* driver,
                                   MessageHandler* handler) {
  RewriteDomainTransformer transformer(
      &base, &base, driver->server_context(), driver->options(),
      driver->message_handler());
  transformer.set_trim_urls(false);
  bool urls_modified = false;

  Css::FontFaces& font_faces = stylesheet->mutable_font_faces();
  for (Css::FontFaces::iterator font_face_iter = font_faces.begin();
       font_face_iter != font_faces.end(); ++font_face_iter) {
    Css::FontFace* font_face = *font_face_iter;
    if (AbsolutifyDeclarations(
            &font_face->mutable_declarations(), &transformer,
            true,  /* Must handle parseable sections in @font-face. */
            handle_unparseable_sections, handler)) {
        urls_modified = true;
    }
  }

  // Absolutify URLs in unparseable selectors and declarations.
  Css::Rulesets& rulesets = stylesheet->mutable_rulesets();
  for (Css::Rulesets::iterator ruleset_iter = rulesets.begin();
       ruleset_iter != rulesets.end(); ++ruleset_iter) {
    Css::Ruleset* ruleset = *ruleset_iter;
    // Check any unparseable sections for any URLs and absolutify as required.
    if (handle_unparseable_sections) {
      switch (ruleset->type()) {
        case Css::Ruleset::RULESET: {
          Css::Selectors& selectors(ruleset->mutable_selectors());
          if (selectors.is_dummy()) {
            StringPiece original_bytes = selectors.bytes_in_original_buffer();
            GoogleString rewritten_bytes;
            StringWriter writer(&rewritten_bytes);
            if (CssTagScanner::TransformUrls(original_bytes, &writer,
                                             &transformer, handler)) {
              selectors.set_bytes_in_original_buffer(rewritten_bytes);
              urls_modified = true;
            }
          }
          break;
        }
        case Css::Ruleset::UNPARSED_REGION: {
          Css::UnparsedRegion* unparsed = ruleset->mutable_unparsed_region();
          StringPiece original_bytes = unparsed->bytes_in_original_buffer();
          GoogleString rewritten_bytes;
          StringWriter writer(&rewritten_bytes);
          if (CssTagScanner::TransformUrls(original_bytes, &writer,
                                           &transformer, handler)) {
            unparsed->set_bytes_in_original_buffer(rewritten_bytes);
            urls_modified = true;
          }
          break;
        }
      }
    }
    if (ruleset->type() == Css::Ruleset::RULESET) {
      if (AbsolutifyDeclarations(&ruleset->mutable_declarations(), &transformer,
                                 handle_parseable_ruleset_sections,
                                 handle_unparseable_sections, handler)) {
        urls_modified = true;
      }
    }
  }

  return urls_modified;
}

bool CssAbsolutify::AbsolutifyDeclarations(
    Css::Declarations* decls, CssTagScanner::Transformer* transformer,
    bool handle_parseable_sections, bool handle_unparseable_sections,
    MessageHandler* handler) {
  bool urls_modified = false;
  for (Css::Declarations::iterator decl_iter = decls->begin();
       decl_iter != decls->end(); ++decl_iter) {
    Css::Declaration* decl = *decl_iter;
    if (decl->prop() == Css::Property::UNPARSEABLE) {
      if (handle_unparseable_sections) {
        StringPiece original_bytes = decl->bytes_in_original_buffer();
        GoogleString rewritten_bytes;
        StringWriter writer(&rewritten_bytes);
        if (CssTagScanner::TransformUrls(original_bytes, &writer,
                                         transformer, handler)) {
          urls_modified = true;
          decl->set_bytes_in_original_buffer(rewritten_bytes);
        }
      }
    } else if (handle_parseable_sections) {
      Css::Values* values = decl->mutable_values();
      for (size_t value_index = 0; value_index < values->size();
           ++value_index) {
        Css::Value* value = values->at(value_index);
        if (value->GetLexicalUnitType() == Css::Value::URI) {
          urls_modified = true;
          GoogleString url = UnicodeTextToUTF8(value->GetStringValue());
          if (transformer->Transform(&url) ==
              CssTagScanner::Transformer::kSuccess) {
            delete (*values)[value_index];
            (*values)[value_index] =
                new Css::Value(Css::Value::URI, UTF8ToUnicodeText(url));
          }
        }
      }
    }
  }
  return urls_modified;
}

}  // namespace net_instaweb
