/* C++ code produced by gperf version 3.0.3 */
/* Command-line: /usr/bin/gperf -m 10 htmlparse/html_agent.gperf  */
/* Computed positions: -k'1-2,5,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "htmlparse/html_agent.gperf"

// html_agent.cc is automatically generated from html_agent.gperf.
// Author: fangfei@google.com

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_agent.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
#include <string.h>

#define TOTAL_KEYWORDS 248
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 61
#define MIN_HASH_VALUE 23
#define MAX_HASH_VALUE 400
/* maximum key range = 378, duplicates = 0 */

class RobotDetect
{
private:
  static inline unsigned int hash (const char *str, unsigned int len);
public:
  static const char *Lookup (const char *str, unsigned int len);
};

inline unsigned int
RobotDetect::hash (register const char *str, register unsigned int len)
{
  static const unsigned short asso_values[] =
    {
      401, 401, 401, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 401, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 401, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 180,  10, 401, 401, 401, 401, 401,   4,
      401,  44, 401, 401, 401, 147,  27, 401,  78,  14,
       47,  72, 401, 401, 401, 401, 401, 401, 401,   4,
      401, 401, 401, 401,  55,  90, 124,  21,  75, 133,
      133, 122,  83,  49,  90,  43, 130,  36,  65, 132,
       32, 401, 157,  42,  61,  39, 141,  10,  10, 205,
      401, 401, 401, 401, 401, 401, 401,   7,  66,  56,
      130,   5,  61,  68,  43,   4,  14,  41,  44,  16,
       29,  10,   4, 401,   5,  25,   4,  23,  73, 141,
        4, 167,  33, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 401, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 401, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 401, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 401, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 401, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 401, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 401, 401, 401,  15, 401, 401, 401, 401,
      401, 401, 401, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 401, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 401, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 401, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 401, 401, 401, 401, 401, 401, 401, 401,
      401, 401, 401, 401, 401, 401
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
      case 3:
      case 2:
        hval += asso_values[(unsigned char)str[1]];
      /*FALLTHROUGH*/
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

static const char * const wordlist[] =
  {
    "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "",
    "", "", "", "", "",
    "root",
    "", "",
    "appie",
    "", "",
    "tarspider",
    "", "",
    "ia_archiver",
    "WebLinker",
    "WebReaper",
    "WebBandit",
    "WebWalker",
    "WebCatcher",
    "WebMoose",
    "moget",
    "",
    "arks",
    "psbot",
    "WWWWanderer",
    "image.kapsi.net",
    "WWWC",
    "esther",
    "mouse.house",
    "none",
    "spiderline",
    "WebQuest",
    "profitnet@myezmail.com",
    "aWapClient",
    "Pioneer",
    "marvin-team@webseek.de",
    "Poppi",
    "uptimebot",
    "suke",
    "Magpie",
    "",
    "MediaFox",
    "Motor",
    "Monster",
    "",
    "SpiderBot",
    "",
    "SimBot",
    "PortalBSpider",
    "spider@portalb.com",
    "Cusco",
    "Katipo",
    "Confuzzledbot",
    "Solbot",
    "WebWatch",
    "PerlCrawler",
    "irobot@chaos.dk",
    "explorersearch",
    "MindCrawler",
    "legs",
    "fido",
    "PortalJuice.com",
    "CMC",
    "MuscatFerret",
    "", "",
    "CrawlPaper",
    "Wget",
    "Snooper",
    "Senrigan",
    "SpiderMan",
    "elfinbot",
    "havIndex",
    "sharp-info-agent",
    "esculapio",
    "ParaSite",
    "Digger",
    "Informant",
    "MerzScope",
    "gammaSpider",
    "Slurp",
    "suntek",
    "gestaltIconoclast",
    "jumpstation",
    "DragonBot",
    "PlumtreeWebAccessor",
    "DesertRealm.com;",
    "about.ask.com",
    "cosmos",
    "robi@computingsite.com",
    "newscan-online",
    "NetCarta CyberPilot Pro",
    "ia_archiver-web.archive.org",
    "gazz",
    "Tarantula",
    "JoBo",
    "urlck",
    "Araneo",
    "Checkbot",
    "Digimarc CGIReader",
    "ArchitextSpider",
    "JoeBot",
    "DWCP",
    "ChristCrawler.com",
    "Muninn",
    "searchprocess",
    "phpdig",
    "SiteTech-Rover",
    "Infoseek Sidewinder",
    "TitIn",
    "H\303\244m\303\244h\303\244kki",
    "Nederland.zoek",
    "JubiiRobot",
    "NorthStar",
    "W3M2",
    "Duppies",
    "IsraeliSearch",
    "ChristCrawler@ChristCENTRAL.com",
    "DoCoMo",
    "NetScoop",
    "gcreep",
    "bbot",
    "Gromit",
    "NetMechanic",
    "vision-search",
    "DIIbot",
    "ssearcher100",
    "iajaBot",
    "bingbot",
    "Templeton",
    "BaySpider",
    "logo.gif",
    "grabber",
    "BoxSeaBot",
    "Linkidator",
    "Peregrinator-Mathematics",
    "Calif",
    "InfoSpiders",
    "NDSpider",
    "Arachnophilia",
    "LinkWalker",
    "DNAbot",
    "",
    "Gulliver",
    "GulperBot",
    "fouineur.9bit.qc.ca)",
    "Atomz",
    "CoolBot",
    "Verticrawlbot",
    "lim@cs.leidenuniv.nl",
    "Golem",
    "Victoria",
    "GetterroboPlus",
    "AraybOt",
    "NHSEWalker",
    "Anthill",
    "LWP",
    "",
    "FastCrawler",
    "WOLP",
    "ATN_Worldwide",
    "Robot",
    "RixBot",
    "Robbie",
    "cIeNcIaFiCcIoN.nEt",
    "Roverbot",
    "Lockon",
    "MOMspider",
    "weblayers",
    "htdig",
    "Googlebot",
    "robot-response@openfind.com.tw",
    "inspectorwww",
    "dlw3robot",
    "SLCrawler",
    "Orbsearch",
    "AITCSRobot",
    "Googlebot-Image",
    "XGET",
    "ESIRover",
    "WebCopy",
    "KO_Yappo_Robot",
    "webwalk",
    "SpiderView",
    "Gigabot",
    "Iron33",
    "VWbot_K",
    "PiltdownMan",
    "PackRat",
    "TLSpider",
    "CyberSpyder",
    "ESISmartSpider",
    "WebFetcher ",
    "CydralSpider",
    "LinkScan",
    "w@pSpider",
    "webvac",
    "Robozilla",
    "Deweb",
    "OntoSpider",
    "libwww-perl-5.41",
    "AURESYS",
    "",
    "JBot",
    "webs@recruit.co.jp",
    "UCSD-Crawler",
    "Occam",
    "UdmSearch",
    "HTMLgobble",
    "w3mir",
    "Shai'Hulud",
    "YandexBot",
    "Voyager",
    "SG-Scout",
    "JavaBee",
    "MwdSearch",
    "borg-bot",
    "EbiNess",
    "YodaoBot",
    "Yahoo!",
    "Robofox",
    "Patric           ",
    "RoboCrawl",
    "void-bot",
    "",
    "INGRID",
    "TITAN",
    "whatUseek_winona",
    "Freecrawl",
    "Raven-v2",
    "",
    "AlkalineBOT",
    "Baiduspider+(+http://www.baidu.com/search/spider.htm)",
    "FunnelWeb-1.0",
    "",
    "Emacs-w3",
    "NEC-MeshExplorer",
    "LabelGrab",
    "",
    "TechBOT",
    "",
    "Die Blinde Kuh",
    "",
    "URL Spider Pro",
    "",
    "IncyWincy",
    "JCrawler",
    "", "",
    "KDD-Explorer",
    "RuLeS",
    "",
    "IAGENT",
    "", "",
    "ASpider",
    "", "",
    "MSNBOT",
    "", "",
    "CACTVS Chemistry Spider",
    "KIT-Fireball",
    "",
    "FelixIDE",
    "", "",
    "RHCS",
    "", "", "", "", "", "",
    "PGP-KA",
    "", "", "", "", "",
    "GetURL.rexx",
    "wired-digital-newsbot",
    "", "", "", "",
    "Fish-Search-Robot",
    "", "",
    "BSpider",
    "",
    "ObjectsSearch",
    "", "", "", "", "", "", "", "", "",
    "Bjaaland",
    "", "", "", "", "", "", "",
    "Valkyrie",
    "", "", "", "", "", "", "", "",
    "PageBoy",
    "",
    "EIT-Link-Verifier-Robot",
    "Nomad",
    "", "", "", "", "", "", "", "", "",
    "", "",
    "Lycos",
    "dienstspider  ",
    "w3index",
    "", "", "", "", "",
    "BlackWidow",
    "BackRub",
    "", "", "", "", "", "", "", "", "",
    "", "", "", "",
    "griffon                                                      ",
    "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "",
    "IBM_Planetwide "
  };

const char *
RobotDetect::Lookup (register const char *str, register unsigned int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key];

          if (*str == *s && !strncmp (str + 1, s + 1, len - 1) && s[len] == '\0')
            return s;
        }
    }
  return 0;
}
#line 271 "htmlparse/html_agent.gperf"


// TODO:(fangfei) check other cases
bool HtmlAgent::Lookup(const char* usr_agent ) {
  StringPiece url(usr_agent);
  // check whether the whole string is in database
  if (RobotDetect::Lookup(url.data(), url.size()) != NULL) {
    return true;
  }
  char separator[] = " /_;";
  std::vector<StringPiece> names;
  names.clear();
  SplitStringPieceToVector(url, separator, &names, true);
  for (int i = 0, n = names.size(); i < n; ++i) {
    LOG(ERROR) << names[i];
    if (RobotDetect::Lookup(names[i].data(),
                            names[i].size()) != NULL) {
         return true;
      }
  }
  
  return false;
}

}  // namespace net_instaweb
