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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_URL_MULTIPART_ENCODER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_URL_MULTIPART_ENCODER_H_

#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;

// Encodes a multiple strings into a single string so that it
// can be decoded.  This is not restricted to URLs but is optimized
// for them in its choice of escape characters.  '+' is used to
// separate the parts, and any parts that include '+' are prefixed
// by a '='.  '=' is converted to '==' -- it's a pretty lightweight
// encoding, and any other character restrictions will have to be
// applied to the output of this class.
//
// TODO(jmarantz): One possibly improvement is to bake this
// functionality into UrlEscaper, changing its interface to accept
// arbitrary numbers of pieces in & out.  However, that would change
// an interface that's used in multiple places, so this is left as
// a TODO.
class UrlMultipartEncoder {
 public:
  UrlMultipartEncoder() {}

  // Removes all the URLs from the encoding.
  void clear() { urls_.clear(); }

  // Adds a new URL to the encoding.  Actually there are no
  // character-set restrictions imposed by this method.
  void AddUrl(const StringPiece& url) {
    urls_.push_back(std::string(url.data(), url.size()));
  }

  // Encode the URLs added to this class into a single string.
  std::string Encode();

  // Decodde an encoding produced by Encode() above to populate
  // this class.
  bool Decode(const StringPiece& encoding, MessageHandler* handler);

  // Returns the number of URLs stored (either by Decode or by
  // AddUrl.
  int num_urls() const { return urls_.size(); }

  // Returns the url at the index.
  const std::string& url(int index) const { return urls_[index]; }

 private:
  StringVector urls_;

  DISALLOW_COPY_AND_ASSIGN(UrlMultipartEncoder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_URL_MULTIPART_ENCODER_H_
