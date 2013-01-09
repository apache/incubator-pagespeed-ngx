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

#ifndef GOOGLEURL_SRC_GURL_H__
#define GOOGLEURL_SRC_GURL_H__

#include <iosfwd>
#include <string>

#include "base/string16.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_canon_stdstring.h"
#include "googleurl/src/url_common.h"
#include "googleurl/src/url_parse.h"

class GURL {
 public:
  typedef url_canon::StdStringReplacements<std::string> Replacements;
  typedef url_canon::StdStringReplacements<string16> ReplacementsW;

  // Creates an empty, invalid URL.
  GURL_API GURL();

  // Copy construction is relatively inexpensive, with most of the time going
  // to reallocating the string. It does not re-parse.
  GURL_API GURL(const GURL& other);

  // The narrow version requires the input be UTF-8. Invalid UTF-8 input will
  // result in an invalid URL.
  //
  // The wide version should also take an encoding parameter so we know how to
  // encode the query parameters. It is probably sufficient for the narrow
  // version to assume the query parameter encoding should be the same as the
  // input encoding.
  GURL_API explicit GURL(const std::string& url_string
                         /*, output_param_encoding*/);
  GURL_API explicit GURL(const string16& url_string
                         /*, output_param_encoding*/);

  // Constructor for URLs that have already been parsed and canonicalized. This
  // is used for conversions from KURL, for example. The caller must supply all
  // information associated with the URL, which must be correct and consistent.
  GURL_API GURL(const char* canonical_spec, size_t canonical_spec_len,
                const url_parse::Parsed& parsed, bool is_valid);

  GURL_API GURL& operator=(const GURL& other);

  // Returns true when this object represents a valid parsed URL. When not
  // valid, other functions will still succeed, but you will not get canonical
  // data out in the format you may be expecting. Instead, we keep something
  // "reasonable looking" so that the user can see how it's busted if
  // displayed to them.
  bool is_valid() const {
    return is_valid_;
  }

  // Returns true if the URL is zero-length. Note that empty URLs are also
  // invalid, and is_valid() will return false for them. This is provided
  // because some users may want to treat the empty case differently.
  bool is_empty() const {
    return spec_.empty();
  }

  // Returns the raw spec, i.e., the full text of the URL, in canonical UTF-8,
  // if the URL is valid. If the URL is not valid, this will assert and return
  // the empty string (for safety in release builds, to keep them from being
  // misused which might be a security problem).
  //
  // The URL will be ASCII except the reference fragment, which may be UTF-8.
  // It is guaranteed to be valid UTF-8.
  //
  // The exception is for empty() URLs (which are !is_valid()) but this will
  // return the empty string without asserting.
  //
  // Used invalid_spec() below to get the unusable spec of an invalid URL. This
  // separation is designed to prevent errors that may cause security problems
  // that could result from the mistaken use of an invalid URL.
  GURL_API const std::string& spec() const;

  // Returns the potentially invalid spec for a the URL. This spec MUST NOT be
  // modified or sent over the network. It is designed to be displayed in error
  // messages to the user, as the apperance of the spec may explain the error.
  // If the spec is valid, the valid spec will be returned.
  //
  // The returned string is guaranteed to be valid UTF-8.
  const std::string& possibly_invalid_spec() const {
    return spec_;
  }

  // Getter for the raw parsed structure. This allows callers to locate parts
  // of the URL within the spec themselves. Most callers should consider using
  // the individual component getters below.
  //
  // The returned parsed structure will reference into the raw spec, which may
  // or may not be valid. If you are using this to index into the spec, BE
  // SURE YOU ARE USING possibly_invalid_spec() to get the spec, and that you
  // don't do anything "important" with invalid specs.
  const url_parse::Parsed& parsed_for_possibly_invalid_spec() const {
    return parsed_;
  }

  // Defiant equality operator!
  bool operator==(const GURL& other) const {
    return spec_ == other.spec_;
  }
  bool operator!=(const GURL& other) const {
    return spec_ != other.spec_;
  }

  // Allows GURL to used as a key in STL (for example, a std::set or std::map).
  bool operator<(const GURL& other) const {
    return spec_ < other.spec_;
  }

  // Resolves a URL that's possibly relative to this object's URL, and returns
  // it. Absolute URLs are also handled according to the rules of URLs on web
  // pages.
  //
  // It may be impossible to resolve the URLs properly. If the input is not
  // "standard" (SchemeIsStandard() == false) and the input looks relative, we
  // can't resolve it. In these cases, the result will be an empty, invalid
  // GURL.
  //
  // The result may also be a nonempty, invalid URL if the input has some kind
  // of encoding error. In these cases, we will try to construct a "good" URL
  // that may have meaning to the user, but it will be marked invalid.
  //
  // It is an error to resolve a URL relative to an invalid URL. The result
  // will be the empty URL.
  GURL_API GURL Resolve(const std::string& relative) const;
  GURL_API GURL Resolve(const string16& relative) const;

  // Like Resolve() above but takes a character set encoder which will be used
  // for any query text specified in the input. The charset converter parameter
  // may be NULL, in which case it will be treated as UTF-8.
  //
  // TODO(brettw): These should be replaced with versions that take something
  // more friendly than a raw CharsetConverter (maybe like an ICU character set
  // name).
  GURL_API GURL ResolveWithCharsetConverter(
      const std::string& relative,
      url_canon::CharsetConverter* charset_converter) const;
  GURL_API GURL ResolveWithCharsetConverter(
      const string16& relative,
      url_canon::CharsetConverter* charset_converter) const;

  // Creates a new GURL by replacing the current URL's components with the
  // supplied versions. See the Replacements class in url_canon.h for more.
  //
  // These are not particularly quick, so avoid doing mutations when possible.
  // Prefer the 8-bit version when possible.
  //
  // It is an error to replace components of an invalid URL. The result will
  // be the empty URL.
  //
  // Note that we use the more general url_canon::Replacements type to give
  // callers extra flexibility rather than our override.
  GURL_API GURL ReplaceComponents(
      const url_canon::Replacements<char>& replacements) const;
  GURL_API GURL ReplaceComponents(
      const url_canon::Replacements<char16>& replacements) const;

  // A helper function that is equivalent to replacing the path with a slash
  // and clearing out everything after that. We sometimes need to know just the
  // scheme and the authority. If this URL is not a standard URL (it doesn't
  // have the regular authority and path sections), then the result will be
  // an empty, invalid GURL. Note that this *does* work for file: URLs, which
  // some callers may want to filter out before calling this.
  //
  // It is an error to get an empty path on an invalid URL. The result
  // will be the empty URL.
  GURL_API GURL GetWithEmptyPath() const;

  // A helper function to return a GURL containing just the scheme, host,
  // and port from a URL. Equivalent to clearing any username and password,
  // replacing the path with a slash, and clearing everything after that. If
  // this URL is not a standard URL, then the result will be an empty,
  // invalid GURL. If the URL has neither username nor password, this
  // degenerates to GetWithEmptyPath().
  //
  // It is an error to get the origin of an invalid URL. The result
  // will be the empty URL.
  GURL_API GURL GetOrigin() const;

  // Returns true if the scheme for the current URL is a known "standard"
  // scheme. Standard schemes have an authority and a path section. This
  // includes file:, which some callers may want to filter out explicitly by
  // calling SchemeIsFile.
  GURL_API bool IsStandard() const;

  // Returns true if the given parameter (should be lower-case ASCII to match
  // the canonicalized scheme) is the scheme for this URL. This call is more
  // efficient than getting the scheme and comparing it because no copies or
  // object constructions are done.
  GURL_API bool SchemeIs(const char* lower_ascii_scheme) const;

  // We often need to know if this is a file URL. File URLs are "standard", but
  // are often treated separately by some programs.
  bool SchemeIsFile() const {
    return SchemeIs("file");
  }

  // If the scheme indicates a secure connection
  bool SchemeIsSecure() const {
    return SchemeIs("https");
  }

  // Returns true if the hostname is an IP address. Note: this function isn't
  // as cheap as a simple getter because it re-parses the hostname to verify.
  // This currently identifies only IPv4 addresses (bug 822685).
  GURL_API bool HostIsIPAddress() const;

  // Getters for various components of the URL. The returned string will be
  // empty if the component is empty or is not present.
  std::string scheme() const {  // Not including the colon. See also SchemeIs.
    return ComponentString(parsed_.scheme);
  }
  std::string username() const {
    return ComponentString(parsed_.username);
  }
  std::string password() const {
    return ComponentString(parsed_.password);
  }
  // Note that this may be a hostname, an IPv4 address, or an IPv6 literal
  // surrounded by square brackets, like "[2001:db8::1]".  To exclude these
  // brackets, use HostNoBrackets() below.
  std::string host() const {
    return ComponentString(parsed_.host);
  }
  std::string port() const {  // Returns -1 if "default"
    return ComponentString(parsed_.port);
  }
  std::string path() const {  // Including first slash following host
    return ComponentString(parsed_.path);
  }
  std::string query() const {  // Stuff following '?'
    return ComponentString(parsed_.query);
  }
  std::string ref() const {  // Stuff following '#'
    return ComponentString(parsed_.ref);
  }

  // Existance querying. These functions will return true if the corresponding
  // URL component exists in this URL. Note that existance is different than
  // being nonempty. http://www.google.com/? has a query that just happens to
  // be empty, and has_query() will return true.
  bool has_scheme() const {
    return parsed_.scheme.len >= 0;
  }
  bool has_username() const {
    return parsed_.username.len >= 0;
  }
  bool has_password() const {
    return parsed_.password.len >= 0;
  }
  bool has_host() const {
    // Note that hosts are special, absense of host means length 0.
    return parsed_.host.len > 0;
  }
  bool has_port() const {
    return parsed_.port.len >= 0;
  }
  bool has_path() const {
    // Note that http://www.google.com/" has a path, the path is "/". This can
    // return false only for invalid or nonstandard URLs.
    return parsed_.path.len >= 0;
  }
  bool has_query() const {
    return parsed_.query.len >= 0;
  }
  bool has_ref() const {
    return parsed_.ref.len >= 0;
  }

  // Returns a parsed version of the port. Can also be any of the special
  // values defined in Parsed for ExtractPort.
  GURL_API int IntPort() const;

  // Returns the port number of the url, or the default port number.
  // If the scheme has no concept of port (or unknown default) returns
  // PORT_UNSPECIFIED.
  GURL_API int EffectiveIntPort() const;

  // Extracts the filename portion of the path and returns it. The filename
  // is everything after the last slash in the path. This may be empty.
  GURL_API std::string ExtractFileName() const;

  // Returns the path that should be sent to the server. This is the path,
  // parameter, and query portions of the URL. It is guaranteed to be ASCII.
  GURL_API std::string PathForRequest() const;

  // Returns the host, excluding the square brackets surrounding IPv6 address
  // literals.  This can be useful for passing to getaddrinfo().
  GURL_API std::string HostNoBrackets() const;

  // Returns true if this URL's host matches or is in the same domain as
  // the given input string. For example if this URL was "www.google.com",
  // this would match "com", "google.com", and "www.google.com
  // (input domain should be lower-case ASCII to match the canonicalized
  // scheme). This call is more efficient than getting the host and check
  // whether host has the specific domain or not because no copies or
  // object constructions are done.
  //
  // If function DomainIs has parameter domain_len, which means the parameter
  // lower_ascii_domain does not gurantee to terminate with NULL character.
  GURL_API bool DomainIs(const char* lower_ascii_domain, int domain_len) const;

  // If function DomainIs only has parameter lower_ascii_domain, which means
  // domain string should be terminate with NULL character.
  bool DomainIs(const char* lower_ascii_domain) const {
    return DomainIs(lower_ascii_domain,
                    static_cast<int>(strlen(lower_ascii_domain)));
  }

  // Swaps the contents of this GURL object with the argument without doing
  // any memory allocations.
  GURL_API void Swap(GURL* other);

  // Returns a reference to a singleton empty GURL. This object is for callers
  // who return references but don't have anything to return in some cases.
  // This function may be called from any thread.
  GURL_API static const GURL& EmptyGURL();

 private:
  // Returns the substring of the input identified by the given component.
  std::string ComponentString(const url_parse::Component& comp) const {
    if (comp.len <= 0)
      return std::string();
    return std::string(spec_, comp.begin, comp.len);
  }

  // The actual text of the URL, in canonical ASCII form.
  std::string spec_;

  // Set when the given URL is valid. Otherwise, we may still have a spec and
  // components, but they may not identify valid resources (for example, an
  // invalid port number, invalid characters in the scheme, etc.).
  bool is_valid_;

  // Identified components of the canonical spec.
  url_parse::Parsed parsed_;

  // TODO bug 684583: Add encoding for query params.
};

// Stream operator so GURL can be used in assertion statements.
GURL_API std::ostream& operator<<(std::ostream& out, const GURL& url);

#endif  // GOOGLEURL_SRC_GURL_H__
