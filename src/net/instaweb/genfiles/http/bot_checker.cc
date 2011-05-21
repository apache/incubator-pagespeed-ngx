/* C++ code produced by gperf version 3.0.3 */
/* Command-line: /usr/bin/gperf -m 10 http/bot_checker.gperf  */
/* Computed positions: -k'1-2' */

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

#line 1 "http/bot_checker.gperf"

// bot_checker.cc is automatically generated from bot_checker.gperf.
// Author: fangfei@google.com

#include "base/logging.h"
#include "net/instaweb/http/public/bot_checker.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
#include <string.h>

#define TOTAL_KEYWORDS 63
#define MIN_WORD_LENGTH 4
#define MAX_WORD_LENGTH 53
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 66
/* maximum key range = 63, duplicates = 0 */

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
  static const unsigned char asso_values[] =
    {
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 39, 13,  3, 43, 20, 24,
      36, 15, 67, 16,  0, 32, 57, 31, 15, 18,
      48, 67, 17, 11, 33, 67, 21, 31, 67,  8,
      67, 67, 67, 67, 67, 67, 67,  1,  0, 67,
      16, 20, 67, 41,  1, 12, 67, 67, 13, 67,
      19,  0,  6, 67, 16, 17,  0, 17,  1, 18,
      67,  0, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
      67, 67, 67, 67, 67, 67
    };
  return len + asso_values[(unsigned char)str[1]] + asso_values[(unsigned char)str[0]];
}

static const char * const wordlist[] =
  {
    "", "", "", "",
    "bbot",
    "Jobot",
    "JoeBot",
    "JBot",
    "borg-bot",
    "void-bot",
    "tarspider",
    "BackRub",
    "BoxSeaBot",
    "BaySpider",
    "about.ask.com",
    "Yahoo!",
    "YodaoBot",
    "Solbot",
    "YandexBot",
    "bingbot",
    "iajaBot",
    "BSpider",
    "Robot",
    "Spider",
    "Googlebot",
    "Roverbot",
    "SpiderBot",
    "JubiiRobot",
    "psbot",
    "SimBot",
    "Googlebot-Image",
    "ASpider",
    "uptimebot",
    "spiderline",
    "Gigabot",
    "RixBot",
    "AraybOt",
    "AlkalineBOT",
    "dlw3robot",
    "AITCSRobot",
    "dienstspider",
    "DNAbot",
    "DIIbot",
    "NDSpider",
    "ArchitextSpider",
    "DragonBot",
    "InfoSpiders",
    "OntoSpider",
    "MSNBOT",
    "ESISmartSpider",
    "CoolBot",
    "wired-digital-newsbot",
    "Checkbot",
    "gammaSpider",
    "Verticrawlbot",
    "CydralSpider",
    "Confuzzledbot",
    "Baiduspider+(+http://www.baidu.com/search/spider.htm)",
    "MOMspider",
    "VWbot_K",
    "TechBOT",
    "PortalBSpider",
    "Lycos",
    "EIT-Link-Verifier-Robot",
    "KO_Yappo_Robot",
    "Fish-Search-Robot",
    "w@pSpider"
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
#line 87 "http/bot_checker.gperf"


// TODO:(fangfei) check other cases
bool BotChecker::Lookup(const StringPiece& user_agent) {
  // check whether the whole string is in database
  if (RobotDetect::Lookup(user_agent.data(), user_agent.size()) != NULL) {
    return true;
  }
  // get the application_name/domain_name/email
  const char separator[] = " /;";
  std::vector<StringPiece> names;
  SplitStringPieceToVector(user_agent, separator, &names, true);
  for (int i = 0, n = names.size(); i < n; ++i) {
    if (RobotDetect::Lookup(names[i].data(),
                            names[i].size()) != NULL) {
      return true;
    }
  }
  return false;
}

}  // namespace net_instaweb
