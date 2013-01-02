// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#ifndef NET_BASE_COOKIE_STORE_H_
#define NET_BASE_COOKIE_STORE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "net/base/cookie_options.h"
#include "net/base/net_api.h"

class GURL;

namespace net {

class CookieMonster;

// An interface for storing and retrieving cookies. Implementations need to
// be thread safe as its methods can be accessed from IO as well as UI threads.
class NET_API CookieStore : public base::RefCountedThreadSafe<CookieStore> {
 public:
  // This struct contains additional consumer-specific information that might
  // be stored with cookies; currently just MAC information, see:
  // http://tools.ietf.org/html/draft-ietf-oauth-v2-http-mac
  struct NET_API CookieInfo {
    CookieInfo();
    ~CookieInfo();

    // The name of the cookie.
    std::string name;
    // TODO(abarth): Add value if any clients need it.

    // The time at which the cookie was created.
    base::Time creation_date;

    // The value of the MAC-Key and MAC-Algorithm attributes, if present.
    std::string mac_key;
    std::string mac_algorithm;
  };

  // Sets a single cookie.  Expects a cookie line, like "a=1; domain=b.com".
  virtual bool SetCookieWithOptions(const GURL& url,
                                    const std::string& cookie_line,
                                    const CookieOptions& options) = 0;

  // TODO(???): what if the total size of all the cookies >4k, can we have a
  // header that big or do we need multiple Cookie: headers?
  // Note: Some sites, such as Facebook, occationally use Cookie headers >4k.
  //
  // Simple interface, gets a cookie string "a=b; c=d" for the given URL.
  // Use options to access httponly cookies.
  virtual std::string GetCookiesWithOptions(const GURL& url,
                                            const CookieOptions& options) = 0;

  // This function is similar to GetCookiesWithOptions same functionality as
  // GetCookiesWithOptions except that it additionaly provides detailed
  // information about the cookie contained in the cookie line.  See |struct
  // CookieInfo| above for details.
  virtual void GetCookiesWithInfo(const GURL& url,
                                  const CookieOptions& options,
                                  std::string* cookie_line,
                                  std::vector<CookieInfo>* cookie_info) = 0;

  // Deletes the passed in cookie for the specified URL.
  virtual void DeleteCookie(const GURL& url,
                            const std::string& cookie_name) = 0;

  // Returns the underlying CookieMonster.
  virtual CookieMonster* GetCookieMonster() = 0;


  // --------------------------------------------------------------------------
  // Helpers to make the above interface simpler for some cases.

  // Sets a cookie for the given URL using default options.
  bool SetCookie(const GURL& url, const std::string& cookie_line);

  // Gets cookies for the given URL using default options.
  std::string GetCookies(const GURL& url);

  // Sets a vector of response cookie values for the same URL.
  void SetCookiesWithOptions(const GURL& url,
                             const std::vector<std::string>& cookie_lines,
                             const CookieOptions& options);
  void SetCookies(const GURL& url,
                  const std::vector<std::string>& cookie_lines);

 protected:
  friend class base::RefCountedThreadSafe<CookieStore>;
  CookieStore();
  virtual ~CookieStore();
};

}  // namespace net

#endif  // NET_BASE_COOKIE_STORE_H_
