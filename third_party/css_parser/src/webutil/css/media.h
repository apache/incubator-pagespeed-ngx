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

// Copyright 2012 Google Inc. All Rights Reserved.
// Author: sligocki@google.com (Shawn Ligocki)
//
// Classes for storing CSS3 @media queries.
// See: http://www.w3.org/TR/css3-mediaqueries/

#ifndef WEBUTIL_CSS_MEDIA_H_
#define WEBUTIL_CSS_MEDIA_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "util/utf8/public/unicodetext.h"

namespace Css {

class Ruleset;

// Classes named roughly after CSS3 @media query syntax names.
// From http://www.w3.org/TR/css3-mediaqueries/#syntax
//
//  media_query_list
//   : S* [media_query [ ',' S* media_query ]* ]?
//   ;
//  media_query
//   : [ONLY | NOT]? S* media_type S* [ AND S* expression ]*
//   | expression [ AND S* expression ]*
//   ;
//  media_type
//   : IDENT
//   ;
//  expression
//   : '(' S* media_feature S* [ ':' S* expr ]? ')' S*
//   ;
//  media_feature
//   : IDENT
//   ;

// Ex: (max-width: 500px)
class MediaExpression {
 public:
  // Media feature without a value. Ex: (color).
  explicit MediaExpression(const UnicodeText& name)
      : name_(name), has_value_(false) {}
  // Media feature with value. Ex: (max-width: 500px).
  MediaExpression(const UnicodeText& name, const UnicodeText& value)
      : name_(name), has_value_(true), value_(value) {}
  ~MediaExpression();

  const UnicodeText& name() const { return name_; }
  bool has_value() const { return has_value_; }
  const UnicodeText& value() const { return value_; }

  MediaExpression* DeepCopy() const;
  string ToString() const;

 private:
  UnicodeText name_;
  bool has_value_;
  // Unparsed value. TODO(sligocki): Actually parse it?
  UnicodeText value_;

  DISALLOW_COPY_AND_ASSIGN(MediaExpression);
};

// Ex: (max-width: 500px) and (color)
class MediaExpressions : public std::vector<MediaExpression*> {
 public:
  MediaExpressions() : std::vector<MediaExpression*>() {}
  ~MediaExpressions();

  string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaExpressions);
};

// Ex: not screen and (max-width: 500px) and (color)
class MediaQuery {
 public:
  MediaQuery() : qualifier_(NO_QUALIFIER) {}
  ~MediaQuery();

  enum MediaQualifier { ONLY, NOT, NO_QUALIFIER };
  MediaQualifier qualifier() const { return qualifier_; }
  const UnicodeText& media_type() const { return media_type_; }
  const MediaExpressions& expressions() const { return expressions_; }
  const MediaExpression& expression(int i) const { return *expressions_[i]; }

  void set_qualifier(MediaQualifier q) { qualifier_ = q; }
  void set_media_type(const UnicodeText& m) { media_type_ = m; }
  // Takes ownership of |expression|.
  void add_expression(MediaExpression* expression) {
    expressions_.push_back(expression);
  }

  MediaQuery* DeepCopy() const;
  string ToString() const;

 private:
  MediaQualifier qualifier_;
  UnicodeText media_type_;
  MediaExpressions expressions_;

  DISALLOW_COPY_AND_ASSIGN(MediaQuery);
};

// Ex: not screen and (max-width: 500px), projection and (color)
class MediaQueries : public std::vector<MediaQuery*> {
 public:
  MediaQueries() : std::vector<MediaQuery*>() {}
  ~MediaQueries();

  // Like clear(), but takes care of memory management as well.
  void Clear();

  MediaQueries* DeepCopy() const;
  string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaQueries);
};

}  // namespace Css

#endif  // WEBUTIL_CSS_MEDIA_H_
