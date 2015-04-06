/*
   From:  http://www.adp-gmbh.ch/cpp/common/base64.html

   base64.cpp and base64.h

   Copyright (C) 2004-2008 René Nyffenegger

   This source code is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this source code must not be misrepresented; you must not
      claim that you wrote the original source code. If you use this source code
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original source code.

   3. This notice may not be removed or altered from any source distribution.

   René Nyffenegger rene.nyffenegger@adp-gmbh.ch

   This file was modified by jmarantz@google.com to:
      - add a web-safe variant that uses '-' and '_' in lieu of '+' and '/'
      - change the 'decode' interface to return false if the input contains
        characters not in the expected char-set.
*/

#ifndef THIRD_PARTY_BASE64_BASE64_H_
#define THIRD_PARTY_BASE64_BASE64_H_

#include <string>

// Initialize the base64 library.  Call this before other methods if
// multi-threaded access is required.
void base64_init();

// Encode/deocde strings in common base64 encoding
std::string base64_encode(const unsigned char* data, unsigned int len);
bool base64_decode(const std::string& s, std::string* output);

// Encode/deocde strings in a base64 encoding for URL values.
std::string web64_encode(const unsigned char* data, unsigned int len);
bool web64_decode(const std::string& s, std::string* output);

#endif  // THIRD_PARTY_BASE64_BASE64_H_
