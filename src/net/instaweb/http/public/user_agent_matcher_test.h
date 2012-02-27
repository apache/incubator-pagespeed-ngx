// Copyright 2012 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef NET_INSTAWEB_HTTP_PUBLIC_USER_AGENT_MATCHER_TEST_H_
#define NET_INSTAWEB_HTTP_PUBLIC_USER_AGENT_MATCHER_TEST_H_

namespace net_instaweb {
namespace UserAgentStrings {

// User Agent strings are from http://www.useragentstring.com/.
// IE: http://www.useragentstring.com/pages/Internet Explorer/
// FireFox: http://www.useragentstring.com/pages/Firefox/
// Chrome: http://www.useragentstring.com/pages/Chrome/
// And there are many more.

const char kAndroidHCUserAgent[] =
    "Mozilla/5.0 (Linux; U; Android 3.2; en-us; Sony Tablet S Build/THMAS11000)"
    " AppleWebKit/534.13 (KHTML, like Gecko) Version/4.0 Safari/534.13";
const char kAndroidICSUserAgent[] =
    "Mozilla/5.0 (Linux; U; Android 4.0.1; en-us; Galaxy Nexus Build/ICL27) "
    "AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30";
const char kChromeUserAgent[] =
    "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US) "
    "AppleWebKit/525.13 (KHTML, like Gecko) Chrome/0.A.B.C Safari/525.13";
const char kChrome9UserAgent[] =  // Not webp capable
    "Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US) "
    "AppleWebKit/534.13 (KHTML, like Gecko) Chrome/9.0.597.19 Safari/534.13";
const char kChrome15UserAgent[] =  // Not webp capable
    "Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US) "
    "AppleWebKit/534.13 (KHTML, like Gecko) Chrome/15.0.597.19 Safari/534.13";
const char kChrome18UserAgent[] =  // webp capable
    "Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US) "
    "AppleWebKit/534.13 (KHTML, like Gecko) Chrome/18.0.597.19 Safari/534.13";
const char kChrome12UserAgent[] =  // webp capable
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_6_4) "
    "AppleWebKit/534.30 (KHTML, like Gecko) Chrome/12.0.742.100 Safari/534.30";
const char kOpera1101UserAgent[] =  // Not webp capable
    "Opera/9.80 (Windows NT 5.2; U; ru) Presto/2.7.62 Version/11.01";
const char kOpera1110UserAgent[] =  // webp capable
    "Opera/9.80 (Windows NT 6.0; U; en) Presto/2.8.99 Version/11.10";
const char kFirefoxUserAgent[] =
    "Mozilla/5.0 (X11; U; Linux x86_64; zh-CN; rv:1.9.2.10) "
    "Gecko/20100922 Ubuntu/10.10 (maverick) Firefox/3.6.10";
const char kFirefox1UserAgent[] =
    "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.8.0.7) "
    "Gecko/20060909 Firefox/1.5.0.7 MG (Novarra-Vision/6.1)";
const char kIe6UserAgent[] =
    "Mozilla/5.0 (Windows; U; MSIE 6.0; Windows NT 5.1; SV1;"
    " .NET CLR 2.0.50727)";
const char kIe7UserAgent[] =
    "Mozilla/5.0 (Windows; U; MSIE 7.0; Windows NT 6.0; en-US)";
const char kIe8UserAgent[] =
    "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; WOW64;"
    " Trident/4.0; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729;"
    " .NET CLR 3.0.30729; Media Center PC 6.0; InfoPath.2;"
    " .NET4.0C; .NET4.0E; FDM)";
const char kIe9UserAgent[] =
    "Mozilla/5.0 (Windows; U; MSIE 9.0; WIndows NT 9.0; en-US))";
const char kIPhoneUserAgent[] =
    "Apple iPhone OS v2.1.1 CoreMedia v1.0.0.5F138";
const char kIPhone4Safari[] =
    "Mozilla/5.0 (iPhone; CPU iPhone OS 5_0_1 like Mac OS X) AppleWebKit/534.46"
    " (KHTML, like Gecko) Version/5.1 Mobile/9A405 Safari/7534.48.3";
const char kNokiaUserAgent[] =
    "Nokia2355/1.0 (JN100V0200.nep) UP.Browser/6.2.2.1.c.1.108 (GUI) MMP/2.0";
const char kOpera5UserAgent[] =
    "Opera/5.0 (SunOS 5.8 sun4u; U) [en]";
const char kOpera8UserAgent[] =
    "Opera/8.01 (J2ME/MIDP; Opera Mini/1.1.2666/1724; en; U; ssr)";
const char kPSPUserAgent[] =
    "Mozilla/4.0 (PSP (PlayStation Portable); 2.00)";
const char kSafariUserAgent[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_6_8) AppleWebKit/534.51.22 "
    "(KHTML, like Gecko) Version/5.1.1 Safari/534.51.22";
const char kOperaMobi9[] =
    "Opera/9.51 Beta (Microsoft Windows; PPC; Opera Mobi/1718; U; en)";
const char kFirefoxNokiaN800[] =  /* This is a tablet */
    "Mozilla/5.0 (X11; U; Linux armv6l; en-US; rv:1.9a6pre) Gecko/20070810 "
    "Firefox/3.0a1 Tablet browser 0.1.16 RX-34_2007SE_4.2007.38-2";

}  // namespace UserAgentStrings
}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_USER_AGENT_MATCHER_TEST_H_
