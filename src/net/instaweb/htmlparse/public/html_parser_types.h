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

#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_PARSER_TYPES_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_PARSER_TYPES_H_

#include <list>

namespace net_instaweb {

class FileSystem;
class HtmlCdataNode;
class HtmlCharactersNode;
class HtmlCommentNode;
class HtmlDirectiveNode;
class HtmlElement;
class HtmlEvent;
class HtmlFilter;
class HtmlIEDirectiveNode;
class HtmlLeafNode;
class HtmlLexer;
class HtmlNode;
class HtmlParse;
class HtmlStartElementEvent;
class HtmlWriterFilter;
class LibxmlAdapter;
class MessageHandler;
class Writer;

typedef std::list<HtmlEvent*> HtmlEventList;
typedef HtmlEventList::iterator HtmlEventListIterator;

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_PARSER_TYPES_H_
