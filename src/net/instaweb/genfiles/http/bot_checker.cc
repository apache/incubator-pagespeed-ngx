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

#include "net/instaweb/http/public/bot_checker.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
#include <string.h>

#define TOTAL_KEYWORDS 65
#define MIN_WORD_LENGTH 4
#define MAX_WORD_LENGTH 23
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 74
/* maximum key range = 71, duplicates = 0 */

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
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 26, 10,  1, 37, 20, 32,
      50, 19, 75, 19, 19, 30, 56, 39, 20, 22,
      43, 75, 15,  5, 32, 75, 24, 31, 75,  9,
      75, 75, 75, 75, 75, 75, 75,  2,  0, 16,
      32, 24, 75, 46, 12,  0, 75, 75,  2, 75,
      17,  8, 16, 75, 12, 14, 45,  9, 39, 33,
      75,  4, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
      75, 75, 75, 75, 75, 75
    };
  return len + asso_values[(unsigned char)str[1]] + asso_values[(unsigned char)str[0]];
}

static const char * const wordlist[] =
  {
    "", "", "", "",
    "bbot",
    "", "",
    "bingbot",
    "bitlybot",
    "iajaBot",
    "BackRub",
    "SimBot",
    "BaySpider",
    "BSpider",
    "Baiduspider",
    "about.ask.com",
    "borg-bot",
    "Yahoo!",
    "BoxSeaBot",
    "Solbot",
    "YandexBot",
    "RixBot",
    "ASpider",
    "AlkalineBOT",
    "JBot",
    "YodaoBot",
    "Gigabot",
    "Spider",
    "Robot",
    "AraybOt",
    "SpiderBot",
    "Roverbot",
    "Jobot",
    "JoeBot",
    "uptimebot",
    "psbot",
    "Googlebot",
    "ArchitextSpider",
    "JubiiRobot",
    "AITCSRobot",
    "spiderline",
    "DragonBot",
    "Googlebot-Image",
    "dlw3robot",
    "dienstspider",
    "DIIbot",
    "DNAbot",
    "InfoSpiders",
    "NDSpider",
    "OntoSpider",
    "MSNBOT",
    "ESISmartSpider",
    "CoolBot",
    "CydralSpider",
    "wired-digital-newsbot",
    "void-bot",
    "tarspider",
    "Checkbot",
    "Confuzzledbot",
    "gammaSpider",
    "vcbot",
    "Verticrawlbot",
    "VWbot_K",
    "TechBOT",
    "PortalBSpider",
    "Lycos",
    "KO_Yappo_Robot",
    "Fish-Search-Robot",
    "w@pSpider",
    "",
    "MOMspider",
    "", "", "",
    "EIT-Link-Verifier-Robot"
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
#line 88 "http/bot_checker.gperf"


// TODO:(fangfei) check other cases
bool BotChecker::Lookup(const StringPiece& user_agent) {
  // check whether the whole string is in database
  if (RobotDetect::Lookup(user_agent.data(), user_agent.size()) != NULL) {
    return true;
  }
  // get the application_name/domain_name/email
  const char separator[] = " /,;+";
  StringPieceVector names;
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
