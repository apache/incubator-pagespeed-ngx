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

// Copyright (C) 2000 and onwards Google, Inc.
//
//
// .cc for the HtmlColor class

#include "webutil/html/htmlcolor.h"

#include <string.h>
#include <cmath>

#include "base/stringprintf.h"
#include "strings/ascii_ctype.h"
#include "strings/case.h"
#include "strings/escaping.h"

typedef struct RgbValue {
  unsigned char r_;
  unsigned char g_;
  unsigned char b_;
} RgbValue;

// color table
// when making change to known_color_values, please
// also change the GetKnownColorValue function because
// entire table is hardcoded into the function for efficiency
static const RgbValue known_color_values[] = {
/* 49 aliceblue */{240, 248, 255},
/* 50 antiquewhite */{250, 235, 215},
/* 51 aqua */{  0, 255, 255},
/* 52 aquamarine */{127, 255, 212},
/* 53 azure */{240, 255, 255},
/* 54 beige */{245, 245, 220},
/* 55 bisque */{255, 228, 196},
/* 56 black */{  0,   0,   0},
/* 57 blanchedalmond */{255, 235, 205},
/* 58 blue */{  0,   0, 255},
/* 59 blueviolet */{138,  43, 226},
/* 60 brown */{165,  42,  42},
/* 61 burlywood */{222, 184, 135},
/* 62 cadetblue */{ 95, 158, 160},
/* 63 chartreuse */{127, 255,   0},
/* 64 chocolate */{210, 105,  30},
/* 65 coral */{255, 127,  80},
/* 66 cornflowerblue */{100, 149, 237},
/* 67 cornsilk */{255, 248, 220},
/* 68 crimson */{220,  20,  60},
/* 69 cyan */{  0, 255, 255},
/* 70 darkblue */{  0,   0, 139},
/* 71 darkcyan */{  0, 139, 139},
/* 72 darkgoldenrod */{184, 134,  11},
/* 73 darkgray */{169, 169, 169},
/* 74 darkgreen */{  0, 100,   0},
/* 75 darkgrey */{169, 169, 169},
/* 76 darkkhaki */{189, 183, 107},
/* 77 darkmagenta */{139,   0, 139},
/* 78 darkolivegreen */{ 85, 107,  47},
/* 79 darkorange */{255, 140,   0},
/* 80 darkorchid */{153,  50, 204},
/* 81 darkred */{139,   0,   0},
/* 82 darksalmon */{233, 150, 122},
/* 83 darkseagreen */{143, 188, 143},
/* 84 darkslateblue */{ 72,  61, 139},
/* 85 darkslategray */{ 47,  79,  79},
/* 86 darkslategrey */{ 47,  79,  79},
/* 87 darkturquoise */{  0, 206, 209},
/* 88 darkviolet */{148,   0, 211},
/* 89 deeppink */{255,  20, 147},
/* 90 deepskyblue */{  0, 191, 255},
/* 91 dimgray */{105, 105, 105},
/* 92 dimgrey */{105, 105, 105},
/* 93 dodgerblue */{ 30, 144, 255},
/* 94 firebrick */{178,  34,  34},
/* 95 floralwhite */{255, 250, 240},
/* 96 forestgreen */{ 34, 139,  34},
/* 97 fuchsia */{255,   0, 255},
/* 98 gainsboro */{220, 220, 220},
/* 99 ghostwhite */{248, 248, 255},
/*100 gold */{255, 215,   0},
/*101 goldenrod */{218, 165,  32},
/*102 gray */{128, 128, 128},
/*103 green */{  0, 128,   0},
/*104 grey */{128, 128, 128},
/*105 greenyellow */{173, 255,  47},
/*106 honeydew */{240, 255, 240},
/*107 hotpink */{255, 105, 180},
/*108 indianred */{205,  92,  92},
/*109 indigo */{ 75,   0, 130},
/*110 ivory */{255, 255, 240},
/*111 khaki */{240, 230, 140},
/*112 lavender */{230, 230, 250},
/*113 lavenderblush */{255, 240, 245},
/*114 lawngreen */{124, 252,   0},
/*115 lemonchiffon */{255, 250, 205},
/*116 lightblue */{173, 216, 230},
/*117 lightcoral */{240, 128, 128},
/*118 lightcyan */{224, 255, 255},
/*119 lightgoldenrodyellow */{250, 250, 210},
/*120 lightgray */{211, 211, 211},
/*121 lightgreen */{144, 238, 144},
/*122 lightgrey */{211, 211, 211},
/*123 lightpink */{255, 182, 193},
/*124 lightsalmon */{255, 160, 122},
/*125 lightseagreen */{ 32, 178, 170},
/*126 lightskyblue */{135, 206, 250},
/*127 lightslategray */{119, 136, 153},
/*128 lightslategrey */{119, 136, 153},
/*129 lightsteelblue */{176, 196, 222},
/*130 lightyellow */{255, 255, 224},
/*131 lime */{  0, 255,   0},
/*132 limegreen */{ 50, 205,  50},
/*133 linen */{250, 240, 230},
/*134 magenta */{255,   0, 255},
/*135 maroon */{128,   0,   0},
/*136 mediumaquamarine */{102, 205, 170},
/*137 mediumblue */{  0,   0, 205},
/*138 mediumorchid */{186,  85, 211},
/*139 mediumpurple */{147, 112, 219},
/*140 mediumseagreen */{ 60, 179, 113},
/*141 mediumslateblue */{123, 104, 238},
/*142 mediumspringgreen */{  0, 250, 154},
/*143 mediumturquoise */{ 72, 209, 204},
/*144 mediumvioletred */{199,  21, 133},
/*145 midnightblue */{ 25,  25, 112},
/*146 mintcream */{245, 255, 250},
/*147 mistyrose */{255, 228, 225},
/*148 moccasin */{255, 228, 181},
/*149 navajowhite */{255, 222, 173},
/*150 navy */{  0,   0, 128},
/*151 oldlace */{253, 245, 230},
/*152 olive */{128, 128,   0},
/*153 olivedrab */{107, 142,  35},
/*154 orange */{255, 165,   0},
/*155 orangered */{255,  69,   0},
/*156 orchid */{218, 112, 214},
/*157 palegoldenrod */{238, 232, 170},
/*158 palegreen */{152, 251, 152},
/*159 paleturquoise */{175, 238, 238},
/*160 palevioletred */{219, 112, 147},
/*161 papayawhip */{255, 239, 213},
/*162 peachpuff */{255, 218, 185},
/*163 peru */{205, 133,  63},
/*164 pink */{255, 192, 203},
/*165 plum */{221, 160, 221},
/*166 powderblue */{176, 224, 230},
/*167 purple */{128,   0, 128},
/*168 red */{255,   0,   0},
/*169 rosybrown */{188, 143, 143},
/*170 royalblue */{ 65, 105, 225},
/*171 saddlebrown */{139,  69,  19},
/*172 salmon */{250, 128, 114},
/*173 sandybrown */{244, 164,  96},
/*174 seagreen */{ 46, 139,  87},
/*175 seashell */{255, 245, 238},
/*176 sienna */{160,  82,  45},
/*177 silver */{192, 192, 192},
/*178 skyblue */{135, 206, 235},
/*179 slateblue */{106,  90, 205},
/*180 slategray */{112, 128, 144},
/*181 slategrey */{112, 128, 144},
/*182 snow */{255, 250, 250},
/*183 springgreen */{  0, 255, 127},
/*184 steelblue */{ 70, 130, 180},
/*185 tan */{210, 180, 140},
/*186 teal */{  0, 128, 128},
/*187 thistle */{216, 191, 216},
/*188 tomato */{255,  99,  71},
/*189 turquoise */{ 64, 224, 208},
/*190 violet */{238, 130, 238},
/*191 wheat */{245, 222, 179},
/*192 white */{255, 255, 255},
/*193 whitesmoke */{245, 245, 245},
/*194 yellow */{255, 255,   0},
/*195 yellowgreen */{154, 205,  50},
};

// here the entire table is hardcoded into the function
// mainly because of consideration of efficiency
static const RgbValue * GetKnownColorValue(StringPiece colorstr) {
  if (colorstr.size() <= 2) {
    return NULL;
  }
  switch (ascii_tolower(colorstr[0])) {
  case 'a':
    switch (ascii_tolower(colorstr[1])) {
    case 'l':
      if (CaseEqual("aliceblue", colorstr)) {
        return &known_color_values[0];
      }
      return NULL;
    case 'n':
      if (CaseEqual("antiquewhite", colorstr)) {
        return &known_color_values[1];
      }
      return NULL;
    case 'q':
      if (CaseEqual("aqua", colorstr)) {
        return &known_color_values[2];
      } else if (CaseEqual("aquamarine", colorstr)) {
        return &known_color_values[3];
      }
      return NULL;
    case 'z':
      if (CaseEqual("azure", colorstr)) {
        return &known_color_values[4];
      }
      return NULL;
    }
    return NULL;
  case 'b':
    switch (ascii_tolower(colorstr[1])) {
    case 'e':
      if (CaseEqual("beige", colorstr)) {
        return &known_color_values[5];
      }
      return NULL;
    case 'i':
      if (CaseEqual("bisque", colorstr)) {
        return &known_color_values[6];
      }
      return NULL;
    case 'l':
      if (CaseEqual("black", colorstr)) {
        return &known_color_values[7];
      } else if (CaseEqual("blanchedalmond", colorstr)) {
        return &known_color_values[8];
      } else if (CaseEqual("blue", colorstr)) {
        return &known_color_values[9];
      } else if (CaseEqual("blueviolet", colorstr)) {
        return &known_color_values[10];
      }
      return NULL;
    case 'r':
      if (CaseEqual("brown", colorstr)) {
        return &known_color_values[11];
      }
      return NULL;
    case 'u':
      if (CaseEqual("burlywood", colorstr)) {
        return &known_color_values[12];
      }
      return NULL;
    }
    return NULL;
  case 'c':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (CaseEqual("cadetblue", colorstr)) {
        return &known_color_values[13];
      }
      return NULL;
    case 'h':
      if (CaseEqual("chartreuse", colorstr)) {
        return &known_color_values[14];
      } else if (CaseEqual("chocolate", colorstr)) {
        return &known_color_values[15];
      }
      return NULL;
    case 'o':
      if (CaseEqual("coral", colorstr)) {
        return &known_color_values[16];
      } else if (CaseEqual("cornflowerblue", colorstr)) {
        return &known_color_values[17];
      } else if (CaseEqual("cornsilk", colorstr)) {
        return &known_color_values[18];
      }
      return NULL;
    case 'r':
      if (CaseEqual("crimson", colorstr)) {
        return &known_color_values[19];
      }
      return NULL;
    case 'y':
      if (CaseEqual("cyan", colorstr)) {
        return &known_color_values[20];
      }
      return NULL;
    }

    return NULL;
  case 'd':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (CaseEqual("darkblue", colorstr)) {
        return &known_color_values[21];
      } else if (CaseEqual("darkcyan", colorstr)) {
        return &known_color_values[22];
      } else if (CaseEqual("darkgoldenrod", colorstr)) {
        return &known_color_values[23];
      } else if (CaseEqual("darkgray", colorstr)) {
        return &known_color_values[24];
      } else if (CaseEqual("darkgreen", colorstr)) {
        return &known_color_values[25];
      } else if (CaseEqual("darkgrey", colorstr)) {
        return &known_color_values[26];
      } else if (CaseEqual("darkkhaki", colorstr)) {
        return &known_color_values[27];
      } else if (CaseEqual("darkmagenta", colorstr)) {
        return &known_color_values[28];
      } else if (CaseEqual("darkolivegreen", colorstr)) {
        return &known_color_values[29];
      } else if (CaseEqual("darkorange", colorstr)) {
        return &known_color_values[30];
      } else if (CaseEqual("darkorchid", colorstr)) {
        return &known_color_values[31];
      } else if (CaseEqual("darkred", colorstr)) {
        return &known_color_values[32];
      } else if (CaseEqual("darksalmon", colorstr)) {
        return &known_color_values[33];
      } else if (CaseEqual("darkseagreen", colorstr)) {
        return &known_color_values[34];
      } else if (CaseEqual("darkslateblue", colorstr)) {
        return &known_color_values[35];
      } else if (CaseEqual("darkslategray", colorstr)) {
        return &known_color_values[36];
      } else if (CaseEqual("darkslategrey", colorstr)) {
        return &known_color_values[37];
      } else if (CaseEqual("darkturquoise", colorstr)) {
        return &known_color_values[38];
      } else if (CaseEqual("darkviolet", colorstr)) {
        return &known_color_values[39];
      }
      return NULL;
    case 'e':
      if (CaseEqual("deeppink", colorstr)) {
        return &known_color_values[40];
      } else if (CaseEqual("deepskyblue", colorstr)) {
        return &known_color_values[41];
      }
      return NULL;
    case 'i':
      if (CaseEqual("dimgray", colorstr)) {
        return &known_color_values[42];
      } else if (CaseEqual("dimgrey", colorstr)) {
        return &known_color_values[43];
      }
      return NULL;
    case 'o':
      if (CaseEqual("dodgerblue", colorstr)) {
        return &known_color_values[44];
      }
      return NULL;
    }
    return NULL;
  case 'f':
    switch (ascii_tolower(colorstr[1])) {
    case 'i':
      if (CaseEqual("firebrick", colorstr)) {
        return &known_color_values[45];
      }
      return NULL;
    case 'l':
      if (CaseEqual("floralwhite", colorstr)) {
        return &known_color_values[46];
      }
      return NULL;
    case 'o':
      if (CaseEqual("forestgreen", colorstr)) {
        return &known_color_values[47];
      }
      return NULL;
    case 'u':
      if (CaseEqual("fuchsia", colorstr)) {
        return &known_color_values[48];
      }
      return NULL;
    }
    return NULL;
  case 'g':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (CaseEqual("gainsboro", colorstr)) {
        return &known_color_values[49];
      }
      return NULL;
    case 'h':
      if (CaseEqual("ghostwhite", colorstr)) {
        return &known_color_values[50];
      }
      return NULL;
    case 'o':
      if (CaseEqual("gold", colorstr)) {
        return &known_color_values[51];
      } else if (CaseEqual("goldenrod", colorstr)) {
        return &known_color_values[52];
      }
      return NULL;
    case 'r':
      if (CaseEqual("gray", colorstr)) {
        return &known_color_values[53];
      } else if (CaseEqual("green", colorstr)) {
        return &known_color_values[54];
      } else if (CaseEqual("grey", colorstr)) {
        return &known_color_values[55];
      } else if (CaseEqual("greenyellow", colorstr)) {
        return &known_color_values[56];
      }
      return NULL;
    }
    return NULL;
  case 'h':
    if (CaseEqual("honeydew", colorstr)) {
      return &known_color_values[57];
    } else if (CaseEqual("hotpink", colorstr)) {
      return &known_color_values[58];
    }
    return NULL;
  case 'i':
    if (CaseEqual("indianred", colorstr)) {
      return &known_color_values[59];
    } else if (CaseEqual("indigo", colorstr)) {
      return &known_color_values[60];
    } else if (CaseEqual("ivory", colorstr)) {
      return &known_color_values[61];
    }
    return NULL;
  case 'k':
    if (CaseEqual("khaki", colorstr)) {
      return &known_color_values[62];
    }
    return NULL;
  case 'l':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (CaseEqual("lavender", colorstr)) {
        return &known_color_values[63];
      } else if (CaseEqual("lavenderblush", colorstr)) {
        return &known_color_values[64];
      } else if (CaseEqual("lawngreen", colorstr)) {
        return &known_color_values[65];
      }
      return NULL;
    case 'e':
      if (CaseEqual("lemonchiffon", colorstr)) {
        return &known_color_values[66];
      }
      return NULL;
    case 'i':
      if (CaseEqual("lightblue", colorstr)) {
        return &known_color_values[67];
      } else if (CaseEqual("lightcoral", colorstr)) {
        return &known_color_values[68];
      } else if (CaseEqual("lightcyan", colorstr)) {
        return &known_color_values[69];
      } else if (CaseEqual("lightgoldenrodyellow", colorstr)) {
        return &known_color_values[70];
      } else if (CaseEqual("lightgray", colorstr)) {
        return &known_color_values[71];
      } else if (CaseEqual("lightgreen", colorstr)) {
        return &known_color_values[72];
      } else if (CaseEqual("lightgrey", colorstr)) {
        return &known_color_values[73];
      } else if (CaseEqual("lightpink", colorstr)) {
        return &known_color_values[74];
      } else if (CaseEqual("lightsalmon", colorstr)) {
        return &known_color_values[75];
      } else if (CaseEqual("lightseagreen", colorstr)) {
        return &known_color_values[76];
      } else if (CaseEqual("lightskyblue", colorstr)) {
        return &known_color_values[77];
      } else if (CaseEqual("lightslategray", colorstr)) {
        return &known_color_values[78];
      } else if (CaseEqual("lightslategrey", colorstr)) {
        return &known_color_values[79];
      } else if (CaseEqual("lightsteelblue", colorstr)) {
        return &known_color_values[80];
      } else if (CaseEqual("lightyellow", colorstr)) {
        return &known_color_values[81];
      } else if (CaseEqual("lime", colorstr)) {
        return &known_color_values[82];
      } else if (CaseEqual("limegreen", colorstr)) {
        return &known_color_values[83];
      } else if (CaseEqual("linen", colorstr)) {
        return &known_color_values[84];
      }
      return NULL;
    }
    return NULL;
  case 'm':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (CaseEqual("magenta", colorstr)) {
        return &known_color_values[85];
      } else if (CaseEqual("maroon", colorstr)) {
        return &known_color_values[86];
      }
      return NULL;
    case 'e':
      if (CaseEqual("mediumaquamarine", colorstr)) {
        return &known_color_values[87];
      } else if (CaseEqual("mediumblue", colorstr)) {
        return &known_color_values[88];
      } else if (CaseEqual("mediumorchid", colorstr)) {
        return &known_color_values[89];
      } else if (CaseEqual("mediumpurple", colorstr)) {
        return &known_color_values[90];
      } else if (CaseEqual("mediumseagreen", colorstr)) {
        return &known_color_values[91];
      } else if (CaseEqual("mediumslateblue", colorstr)) {
        return &known_color_values[92];
      } else if (CaseEqual("mediumspringgreen", colorstr)) {
        return &known_color_values[93];
      } else if (CaseEqual("mediumturquoise", colorstr)) {
        return &known_color_values[94];
      } else if (CaseEqual("mediumvioletred", colorstr)) {
        return &known_color_values[95];
      }
      return NULL;
    case 'i':
      if (CaseEqual("midnightblue", colorstr)) {
        return &known_color_values[96];
      } else if (CaseEqual("mintcream", colorstr)) {
        return &known_color_values[97];
      } else if (CaseEqual("mistyrose", colorstr)) {
        return &known_color_values[98];
      }
      return NULL;
    case 'o':
      if (CaseEqual("moccasin", colorstr)) {
        return &known_color_values[99];
      }
      return NULL;
    }
    return NULL;
  case 'n':
    if (CaseEqual("navajowhite", colorstr)) {
      return &known_color_values[100];
    } else if (CaseEqual("navy", colorstr)) {
      return &known_color_values[101];
    }
    return NULL;
  case 'o':
    switch (ascii_tolower(colorstr[1])) {
    case 'l':
      if (CaseEqual("oldlace", colorstr)) {
        return &known_color_values[102];
      } else if (CaseEqual("olive", colorstr)) {
        return &known_color_values[103];
      } else if (CaseEqual("olivedrab", colorstr)) {
        return &known_color_values[104];
      }
      return NULL;
    case 'r':
      if (CaseEqual("orange", colorstr)) {
        return &known_color_values[105];
      } else if (CaseEqual("orangered", colorstr)) {
        return &known_color_values[106];
      } else if (CaseEqual("orchid", colorstr)) {
        return &known_color_values[107];
      }
      return NULL;
    }
    return NULL;
  case 'p':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (CaseEqual("palegoldenrod", colorstr)) {
        return &known_color_values[108];
      } else if (CaseEqual("palegreen", colorstr)) {
        return &known_color_values[109];
      } else if (CaseEqual("paleturquoise", colorstr)) {
        return &known_color_values[110];
      } else if (CaseEqual("palevioletred", colorstr)) {
        return &known_color_values[111];
      } else if (CaseEqual("papayawhip", colorstr)) {
        return &known_color_values[112];
      }
      return NULL;
    case 'e':
      if (CaseEqual("peachpuff", colorstr)) {
        return &known_color_values[113];
      } else if (CaseEqual("peru", colorstr)) {
        return &known_color_values[114];
      }
      return NULL;
    case 'i':
      if (CaseEqual("pink", colorstr)) {
        return &known_color_values[115];
      }
      return NULL;
    case 'l':
      if (CaseEqual("plum", colorstr)) {
        return &known_color_values[116];
      }
      return NULL;
    case 'o':
      if (CaseEqual("powderblue", colorstr)) {
        return &known_color_values[117];
      }
      return NULL;
    case 'u':
      if (CaseEqual("purple", colorstr)) {
        return &known_color_values[118];
      }
      return NULL;
    }
    return NULL;
  case 'r':
    if (CaseEqual("red", colorstr)) {
      return &known_color_values[119];
    } else if (CaseEqual("rosybrown", colorstr)) {
      return &known_color_values[120];
    } else if (CaseEqual("royalblue", colorstr)) {
      return &known_color_values[121];
    }
    return NULL;
  case 's':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (CaseEqual("saddlebrown", colorstr)) {
        return &known_color_values[122];
      } else if (CaseEqual("salmon", colorstr)) {
        return &known_color_values[123];
      } else if (CaseEqual("sandybrown", colorstr)) {
        return &known_color_values[124];
      }
      return NULL;
    case 'e':
      if (CaseEqual("seagreen", colorstr)) {
        return &known_color_values[125];
      } else if (CaseEqual("seashell", colorstr)) {
        return &known_color_values[126];
      }
      return NULL;
    case 'i':
      if (CaseEqual("sienna", colorstr)) {
        return &known_color_values[127];
      } else if (CaseEqual("silver", colorstr)) {
        return &known_color_values[128];
      }
      return NULL;
    case 'k':
      if (CaseEqual("skyblue", colorstr)) {
        return &known_color_values[129];
      }
      return NULL;
    case 'l':
      if (CaseEqual("slateblue", colorstr)) {
        return &known_color_values[130];
      } else if (CaseEqual("slategray", colorstr)) {
        return &known_color_values[131];
      } else if (CaseEqual("slategrey", colorstr)) {
        return &known_color_values[132];
      }
      return NULL;
    case 'n':
      if (CaseEqual("snow", colorstr)) {
        return &known_color_values[133];
      }
      return NULL;
    case 'p':
      if (CaseEqual("springgreen", colorstr)) {
        return &known_color_values[134];
      }
      return NULL;
    case 't':
      if (CaseEqual("steelblue", colorstr)) {
        return &known_color_values[135];
      }
      return NULL;
    }
    return NULL;
  case 't':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (CaseEqual("tan", colorstr)) {
        return &known_color_values[136];
      }
      return NULL;
    case 'e':
      if (CaseEqual("teal", colorstr)) {
        return &known_color_values[137];
      }
      return NULL;
    case 'h':
      if (CaseEqual("thistle", colorstr)) {
        return &known_color_values[138];
      }
      return NULL;
    case 'o':
      if (CaseEqual("tomato", colorstr)) {
        return &known_color_values[139];
      }
      return NULL;
    case 'u':
      if (CaseEqual("turquoise", colorstr)) {
        return &known_color_values[140];
      }
      return NULL;
    }
    return NULL;
  case 'v':
    if (CaseEqual("violet", colorstr)) {
      return &known_color_values[141];
    }
    return NULL;
  case 'w':
    if (CaseEqual("wheat", colorstr)) {
      return &known_color_values[142];
    } else if (CaseEqual("white", colorstr)) {
      return &known_color_values[143];
    } else if (CaseEqual("whitesmoke", colorstr)) {
      return &known_color_values[144];
    }
    return NULL;
  case 'y':
    if (CaseEqual("yellow", colorstr)) {
      return &known_color_values[145];
    } else if (CaseEqual("yellowgreen", colorstr)) {
      return &known_color_values[146];
    }
    return NULL;
  }

  return NULL;
}  // NOLINT

const unsigned char HtmlColor::kGoodColorValue;
const unsigned char HtmlColor::kBadColorName;
const unsigned char HtmlColor::kBadColorHex;

static inline int TwoXDigitsToNum(StringPiece xstr) {
  return (
      strings::hex_digit_to_int(
          xstr[0])*16 + strings::hex_digit_to_int(xstr[1]));
}

void HtmlColor::SetValueFromHexStr(StringPiece hexstr) {
  int hexstr_len = hexstr.size();
  const char* finalstr = hexstr.data();
  char hexbuf[7];

  if (hexstr_len == 3) {
    for (int i = 0; i < 3; i++) {
      if (!ascii_isxdigit(hexstr[i])) {
        SetBadHexValue();
        return;
      }
      hexbuf[2 * i] = hexstr[i];
      hexbuf[2 * i + 1] = hexstr[i];
    }
    hexbuf[6] = '\0';
    finalstr = hexbuf;
  } else if (hexstr_len == 6) {
    for (int i = 0; i < 6; i++) {
      if (!ascii_isxdigit(hexstr[i])) {
        SetBadHexValue();
        return;
      }
    }
  } else {
    SetBadHexValue();
    return;
  }

  r_ = static_cast<unsigned char>(TwoXDigitsToNum(finalstr));
  g_ = static_cast<unsigned char>(TwoXDigitsToNum(finalstr + 2));
  b_ = static_cast<unsigned char>(TwoXDigitsToNum(finalstr + 4));
  is_bad_value_ = kGoodColorValue;
}

HtmlColor::HtmlColor(StringPiece str) {
  SetValueFromStr(str);
}

HtmlColor::HtmlColor(const char* str, int colorstrlen) {
  SetValueFromStr(StringPiece(str, colorstrlen));
}

void HtmlColor::SetValueFromStr(StringPiece colorstr) {
  if (colorstr.size() > 0 && colorstr[0] == '#') {
    // rgb value
    colorstr.remove_prefix(1);
    SetValueFromHexStr(colorstr);
  } else {
    SetValueFromName(colorstr);
    if (!IsDefined() && colorstr.size() == 6) {
      SetValueFromHexStr(colorstr);
      if (!IsDefined())
        SetBadNameValue();
    }
  }
}

void HtmlColor::SetValueFromName(StringPiece str) {
  const RgbValue *p_rgb = GetKnownColorValue(str);
  if (p_rgb) {
    r_ = p_rgb->r_;
    g_ = p_rgb->g_;
    b_ = p_rgb->b_;
    is_bad_value_ = kGoodColorValue;
  } else {
    SetBadNameValue();
  }
}

void HtmlColor::SetValueFromRGB(unsigned char r, unsigned char g,
                                unsigned char b) {
  r_ = r;
  g_ = g;
  b_ = b;
  is_bad_value_ = kGoodColorValue;
}

HtmlColor::HtmlColor(unsigned char r, unsigned char g, unsigned char b) {
  SetValueFromRGB(r, g, b);
}

static const float kLumR = 0.30;
static const float kLumG = 0.59;
static const float kLumB = 0.11;

// Converts from RGB to HSL. This is an algorithm derived from Fundamentals of
// Interactive Computer Graphics by Foley and van Dam (1982). A (slightly)
// modified formula can be found at
//   http://en.wikipedia.org/wiki/HSL_color_space
// We adopt the notation that HSL values are expressed in the range of 0 to 1.
// Specifically, H is in [0, 1), while S and L are in [0, 1].
// (Another popular scheme normalizes HSL to [0, 360), [0, 100], and [0, 100],
// respectively).
static void RGBtoHSL(const HtmlColor& rgb, double* h, double* s, double* l) {
  int r = rgb.r();
  int g = rgb.g();
  int b = rgb.b();
  int max_v = (r < g) ? ((g < b) ? b : g) : ((b < r) ? r : b);
  int min_v = (r < g) ? ((r < b) ? r : b) : ((b < g) ? b : g);
  int sum = max_v + min_v;
  double delta = static_cast<double>(max_v - min_v);

  double dR = (max_v - r) / delta;
  double dG = (max_v - g) / delta;
  double dB = (max_v - b) / delta;

  if (min_v == max_v)
    *h = 0.0;
  else if (r == max_v)
    *h = (dB - dG) / 6.0;
  else if (g == max_v)
    *h = (2.0 + dR - dB) / 6.0;
  else
    *h = (4.0 + dG - dR) / 6.0;

  if (*h < 0.0)
    *h += 1.0;
  if (*h >= 1.0)
    *h -= 1.0;

  *l = 0.5 * sum / 255.0;

  if (max_v == 0 || min_v == 255)
    *s = 0.0;
  else if (sum <= 255)
    *s = delta / sum;
  else
    *s = delta / (2 * 255 - sum);
}

// Calculates the Euclidean distance between two color vectors on a HSL sphere.
// A demo of the sphere can also be found at:
//   http://en.wikipedia.org/wiki/HSL_color_space
// In short, a vector for color (H, S, L) in this system can be expressed as
//   (S*L'*cos(2*PI*H), S*L'*sin(2*PI*H), L), where L' = abs(L - 0.5).
// And we simply calculate the l-2 distance using these coordinates.
static double HSLDistance(double h1, double s1, double l1, double h2,
                          double s2, double l2) {
  double sl1, sl2;
  if (l1 <= 0.5)
    sl1 = s1 * l1;
  else
    sl1 = s1 * (1.0 - l1);

  if (l2 <= 0.5)
    sl2 = s2 * l2;
  else
    sl2 = s2 * (1.0 - l2);

  double dH = (h1 - h2) * 2.0 * M_PI;
  return (l1 - l2) * (l1 - l2) +
    sl1 * sl1 + sl2 * sl2 - 2 * sl1 * sl2 * cos(dH);
}

bool HtmlColor::IsSimilarInHSL(const HtmlColor &color, double level) const {
  double h1, s1, l1, h2, s2, l2;
  RGBtoHSL(*this, &h1, &s1, &l1);
  RGBtoHSL(color, &h2, &s2, &l2);
  return HSLDistance(h1, s1, l1, h2, s2, l2) <= level;
}

// Calculate the luminance of the color
// Luminance is an integer value from 0 - 255, which
// represents the gray level that most closely corresponds
// to the perceived brightness of the color, based on
// the way the human eye sees color. The weights in
// the function below are pretty standard.  See
// http://www.google.com/search?q=rgb+luminance+formula
int HtmlColor::Luminance() const {
  if (!IsDefined())
    return 0;

  float luminance = (kLumR * static_cast<float>(r_)) +
                    (kLumG * static_cast<float>(g_)) +
                    (kLumB * static_cast<float>(b_));
  return static_cast<int>(luminance);
}

void HtmlColor::Lighten(float factor) {
  HtmlColor white("ffffff", 6);
  BlendWithColor(1.0-factor, white);
}

void HtmlColor::Darken(float factor) {
  HtmlColor black("000000", 6);
  BlendWithColor(1.0-factor, black);
}

void HtmlColor::Desaturate(float factor) {
  if (!IsDefined() || factor < 0 || factor > 1)
    return;

  unsigned char lum = static_cast<unsigned char>(Luminance());
  HtmlColor gray(lum, lum, lum);
  BlendWithColor(1.0-factor, gray);
}

void HtmlColor::BlendWithColor(float factor, const HtmlColor& c) {
  if (!IsDefined() || factor < 0 || factor > 1)
    return;

  r_ = static_cast<unsigned char>(factor * r_ + (1-factor) * c.r_);
  g_ = static_cast<unsigned char>(factor * g_ + (1-factor) * c.g_);
  b_ = static_cast<unsigned char>(factor * b_ + (1-factor) * c.b_);
}

// Return the color as a string for use in HTML
// This isn't the most efficient function.  If you're using it a lot,
// you might want to rewrite it.
string HtmlColor::ToString() const {
  return StringPrintf("#%02x%02x%02x", r_, g_, b_);
}


//
// == HtmlColorUtils ==
//
string HtmlColorUtils::MaybeConvertToCssShorthand(StringPiece orig_color) {
  HtmlColor color(orig_color);
  if ( !color.IsDefined() )
    return orig_color.as_string();

  string shorthand = MaybeConvertToCssShorthand(color);
  if (shorthand.size() < orig_color.size()) {
    return shorthand;
  } else {
    return orig_color.as_string();
  }
}

string HtmlColorUtils::MaybeConvertToCssShorthand(const HtmlColor& color) {
  // There are 16 color names which are supported by all known CSS compliant
  // browsers.  Of these 16, 9 are shorter than their hex equivalents.  For
  // this reason, we prefer to use the shorter color names in order to save
  // bytes.
  switch (color.rgb()) {
    case 0x000080:
      return "navy";
    case 0x008000:
      return "green";
    case 0x008080:
      return "teal";
    case 0x800000:
      return "maroon";
    case 0x800080:
      return "purple";
    case 0x808000:
      return "olive";
    case 0x808080:
      return "gray";
    case 0xC0C0C0:
        return "silver";
    case 0xFF0000:
        return "red";
  }

  if ( (color.r() >> 4) != (color.r() & 0xF) ||
       (color.g() >> 4) != (color.g() & 0xF) ||
       (color.b() >> 4) != (color.b() & 0xF) )
    return color.ToString();

  return StringPrintf("#%01x%01x%01x",
                      color.r() & 0xF,
                      color.g() & 0xF,
                      color.b() & 0xF);
}
