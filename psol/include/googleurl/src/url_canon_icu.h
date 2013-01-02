// Copyright 2007, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ICU integration functions.

#ifndef GOOGLEURL_SRC_URL_CANON_ICU_H__
#define GOOGLEURL_SRC_URL_CANON_ICU_H__

#include "googleurl/src/url_canon.h"

typedef struct UConverter UConverter;

namespace url_canon {

// An implementation of CharsetConverter that implementations can use to
// interface the canonicalizer with ICU's conversion routines.
class ICUCharsetConverter : public CharsetConverter {
 public:
  // Constructs a converter using an already-existing ICU character set
  // converter. This converter is NOT owned by this object; the lifetime must
  // be managed by the creator such that it is alive as long as this is.
  GURL_API ICUCharsetConverter(UConverter* converter);

  GURL_API virtual ~ICUCharsetConverter() {}

  GURL_API virtual void ConvertFromUTF16(const char16* input,
                                         int input_len,
                                         CanonOutput* output);

 private:
  // The ICU converter, not owned by this class.
  UConverter* converter_;
};

}  // namespace url_canon

#endif  // GOOGLEURL_SRC_URL_CANON_ICU_H__
