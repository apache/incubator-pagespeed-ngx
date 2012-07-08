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

// here the entiry table is hardcoded into the function
// mainly because of consideration of efficiency
static const RgbValue * GetKnownColorValue(const char *colorstr) {
  switch (ascii_tolower(colorstr[0])) {
  case 'a':
    switch (ascii_tolower(colorstr[1])) {
    case 'l':
      if (!strcasecmp("aliceblue", colorstr)) {
        return &known_color_values[0];
      }
      return NULL;
    case 'n':
      if (!strcasecmp("antiquewhite", colorstr)) {
        return &known_color_values[1];
      }
      return NULL;
    case 'q':
      if (!strcasecmp("aqua", colorstr)) {
        return &known_color_values[2];
      } else if (!strcasecmp("aquamarine", colorstr)) {
        return &known_color_values[3];
      }
      return NULL;
    case 'z':
      if (!strcasecmp("azure", colorstr)) {
        return &known_color_values[4];
      }
      return NULL;
    }
    return NULL;
  case 'b':
    switch (ascii_tolower(colorstr[1])) {
    case 'e':
      if (!strcasecmp("beige", colorstr)) {
        return &known_color_values[5];
      }
      return NULL;
    case 'i':
      if (!strcasecmp("bisque", colorstr)) {
        return &known_color_values[6];
      }
      return NULL;
    case 'l':
      if (!strcasecmp("black", colorstr)) {
        return &known_color_values[7];
      } else if (!strcasecmp("blanchedalmond", colorstr)) {
        return &known_color_values[8];
      } else if (!strcasecmp("blue", colorstr)) {
        return &known_color_values[9];
      } else if (!strcasecmp("blueviolet", colorstr)) {
        return &known_color_values[10];
      }
      return NULL;
    case 'r':
      if (!strcasecmp("brown", colorstr)) {
        return &known_color_values[11];
      }
      return NULL;
    case 'u':
      if (!strcasecmp("burlywood", colorstr)) {
        return &known_color_values[12];
      }
      return NULL;
    }
    return NULL;
  case 'c':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (!strcasecmp("cadetblue", colorstr)) {
        return &known_color_values[13];
      }
      return NULL;
    case 'h':
      if (!strcasecmp("chartreuse", colorstr)) {
        return &known_color_values[14];
      } else if (!strcasecmp("chocolate", colorstr)) {
        return &known_color_values[15];
      }
      return NULL;
    case 'o':
      if (!strcasecmp("coral", colorstr)) {
        return &known_color_values[16];
      } else if (!strcasecmp("cornflowerblue", colorstr)) {
        return &known_color_values[17];
      } else if (!strcasecmp("cornsilk", colorstr)) {
        return &known_color_values[18];
      }
      return NULL;
    case 'r':
      if (!strcasecmp("crimson", colorstr)) {
        return &known_color_values[19];
      }
      return NULL;
    case 'y':
      if (!strcasecmp("cyan", colorstr)) {
        return &known_color_values[20];
      }
      return NULL;
    }

    return NULL;
  case 'd':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (!strcasecmp("darkblue", colorstr)) {
        return &known_color_values[21];
      } else if (!strcasecmp("darkcyan", colorstr)) {
        return &known_color_values[22];
      } else if (!strcasecmp("darkgoldenrod", colorstr)) {
        return &known_color_values[23];
      } else if (!strcasecmp("darkgray", colorstr)) {
        return &known_color_values[24];
      } else if (!strcasecmp("darkgreen", colorstr)) {
        return &known_color_values[25];
      } else if (!strcasecmp("darkgrey", colorstr)) {
        return &known_color_values[26];
      } else if (!strcasecmp("darkkhaki", colorstr)) {
        return &known_color_values[27];
      } else if (!strcasecmp("darkmagenta", colorstr)) {
        return &known_color_values[28];
      } else if (!strcasecmp("darkolivegreen", colorstr)) {
        return &known_color_values[29];
      } else if (!strcasecmp("darkorange", colorstr)) {
        return &known_color_values[30];
      } else if (!strcasecmp("darkorchid", colorstr)) {
        return &known_color_values[31];
      } else if (!strcasecmp("darkred", colorstr)) {
        return &known_color_values[32];
      } else if (!strcasecmp("darksalmon", colorstr)) {
        return &known_color_values[33];
      } else if (!strcasecmp("darkseagreen", colorstr)) {
        return &known_color_values[34];
      } else if (!strcasecmp("darkslateblue", colorstr)) {
        return &known_color_values[35];
      } else if (!strcasecmp("darkslategray", colorstr)) {
        return &known_color_values[36];
      } else if (!strcasecmp("darkslategrey", colorstr)) {
        return &known_color_values[37];
      } else if (!strcasecmp("darkturquoise", colorstr)) {
        return &known_color_values[38];
      } else if (!strcasecmp("darkviolet", colorstr)) {
        return &known_color_values[39];
      }
      return NULL;
    case 'e':
      if (!strcasecmp("deeppink", colorstr)) {
        return &known_color_values[40];
      } else if (!strcasecmp("deepskyblue", colorstr)) {
        return &known_color_values[41];
      }
      return NULL;
    case 'i':
      if (!strcasecmp("dimgray", colorstr)) {
        return &known_color_values[42];
      } else if (!strcasecmp("dimgrey", colorstr)) {
        return &known_color_values[43];
      }
      return NULL;
    case 'o':
      if (!strcasecmp("dodgerblue", colorstr)) {
        return &known_color_values[44];
      }
      return NULL;
    }
    return NULL;
  case 'f':
    switch (ascii_tolower(colorstr[1])) {
    case 'i':
      if (!strcasecmp("firebrick", colorstr)) {
        return &known_color_values[45];
      }
      return NULL;
    case 'l':
      if (!strcasecmp("floralwhite", colorstr)) {
        return &known_color_values[46];
      }
      return NULL;
    case 'o':
      if (!strcasecmp("forestgreen", colorstr)) {
        return &known_color_values[47];
      }
      return NULL;
    case 'u':
      if (!strcasecmp("fuchsia", colorstr)) {
        return &known_color_values[48];
      }
      return NULL;
    }
    return NULL;
  case 'g':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (!strcasecmp("gainsboro", colorstr)) {
        return &known_color_values[49];
      }
      return NULL;
    case 'h':
      if (!strcasecmp("ghostwhite", colorstr)) {
        return &known_color_values[50];
      }
      return NULL;
    case 'o':
      if (!strcasecmp("gold", colorstr)) {
        return &known_color_values[51];
      } else if (!strcasecmp("goldenrod", colorstr)) {
        return &known_color_values[52];
      }
      return NULL;
    case 'r':
      if (!strcasecmp("gray", colorstr)) {
        return &known_color_values[53];
      } else if (!strcasecmp("green", colorstr)) {
        return &known_color_values[54];
      } else if (!strcasecmp("grey", colorstr)) {
        return &known_color_values[55];
      } else if (!strcasecmp("greenyellow", colorstr)) {
        return &known_color_values[56];
      }
      return NULL;
    }
    return NULL;
  case 'h':
    if (!strcasecmp("honeydew", colorstr)) {
      return &known_color_values[57];
    } else if (!strcasecmp("hotpink", colorstr)) {
      return &known_color_values[58];
    }
    return NULL;
  case 'i':
    if (!strcasecmp("indianred", colorstr)) {
      return &known_color_values[59];
    } else if (!strcasecmp("indigo", colorstr)) {
      return &known_color_values[60];
    } else if (!strcasecmp("ivory", colorstr)) {
      return &known_color_values[61];
    }
    return NULL;
  case 'k':
    if (!strcasecmp("khaki", colorstr)) {
      return &known_color_values[62];
    }
    return NULL;
  case 'l':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (!strcasecmp("lavender", colorstr)) {
        return &known_color_values[63];
      } else if (!strcasecmp("lavenderblush", colorstr)) {
        return &known_color_values[64];
      } else if (!strcasecmp("lawngreen", colorstr)) {
        return &known_color_values[65];
      }
      return NULL;
    case 'e':
      if (!strcasecmp("lemonchiffon", colorstr)) {
        return &known_color_values[66];
      }
      return NULL;
    case 'i':
      if (!strcasecmp("lightblue", colorstr)) {
        return &known_color_values[67];
      } else if (!strcasecmp("lightcoral", colorstr)) {
        return &known_color_values[68];
      } else if (!strcasecmp("lightcyan", colorstr)) {
        return &known_color_values[69];
      } else if (!strcasecmp("lightgoldenrodyellow", colorstr)) {
        return &known_color_values[70];
      } else if (!strcasecmp("lightgray", colorstr)) {
        return &known_color_values[71];
      } else if (!strcasecmp("lightgreen", colorstr)) {
        return &known_color_values[72];
      } else if (!strcasecmp("lightgrey", colorstr)) {
        return &known_color_values[73];
      } else if (!strcasecmp("lightpink", colorstr)) {
        return &known_color_values[74];
      } else if (!strcasecmp("lightsalmon", colorstr)) {
        return &known_color_values[75];
      } else if (!strcasecmp("lightseagreen", colorstr)) {
        return &known_color_values[76];
      } else if (!strcasecmp("lightskyblue", colorstr)) {
        return &known_color_values[77];
      } else if (!strcasecmp("lightslategray", colorstr)) {
        return &known_color_values[78];
      } else if (!strcasecmp("lightslategrey", colorstr)) {
        return &known_color_values[79];
      } else if (!strcasecmp("lightsteelblue", colorstr)) {
        return &known_color_values[80];
      } else if (!strcasecmp("lightyellow", colorstr)) {
        return &known_color_values[81];
      } else if (!strcasecmp("lime", colorstr)) {
        return &known_color_values[82];
      } else if (!strcasecmp("limegreen", colorstr)) {
        return &known_color_values[83];
      } else if (!strcasecmp("linen", colorstr)) {
        return &known_color_values[84];
      }
      return NULL;
    }
    return NULL;
  case 'm':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (!strcasecmp("magenta", colorstr)) {
        return &known_color_values[85];
      } else if (!strcasecmp("maroon", colorstr)) {
        return &known_color_values[86];
      }
      return NULL;
    case 'e':
      if (!strcasecmp("mediumaquamarine", colorstr)) {
        return &known_color_values[87];
      } else if (!strcasecmp("mediumblue", colorstr)) {
        return &known_color_values[88];
      } else if (!strcasecmp("mediumorchid", colorstr)) {
        return &known_color_values[89];
      } else if (!strcasecmp("mediumpurple", colorstr)) {
        return &known_color_values[90];
      } else if (!strcasecmp("mediumseagreen", colorstr)) {
        return &known_color_values[91];
      } else if (!strcasecmp("mediumslateblue", colorstr)) {
        return &known_color_values[92];
      } else if (!strcasecmp("mediumspringgreen", colorstr)) {
        return &known_color_values[93];
      } else if (!strcasecmp("mediumturquoise", colorstr)) {
        return &known_color_values[94];
      } else if (!strcasecmp("mediumvioletred", colorstr)) {
        return &known_color_values[95];
      }
      return NULL;
    case 'i':
      if (!strcasecmp("midnightblue", colorstr)) {
        return &known_color_values[96];
      } else if (!strcasecmp("mintcream", colorstr)) {
        return &known_color_values[97];
      } else if (!strcasecmp("mistyrose", colorstr)) {
        return &known_color_values[98];
      }
      return NULL;
    case 'o':
      if (!strcasecmp("moccasin", colorstr)) {
        return &known_color_values[99];
      }
      return NULL;
    }
    return NULL;
  case 'n':
    if (!strcasecmp("navajowhite", colorstr)) {
      return &known_color_values[100];
    } else if (!strcasecmp("navy", colorstr)) {
      return &known_color_values[101];
    }
    return NULL;
  case 'o':
    switch (ascii_tolower(colorstr[1])) {
    case 'l':
      if (!strcasecmp("oldlace", colorstr)) {
        return &known_color_values[102];
      } else if (!strcasecmp("olive", colorstr)) {
        return &known_color_values[103];
      } else if (!strcasecmp("olivedrab", colorstr)) {
        return &known_color_values[104];
      }
      return NULL;
    case 'r':
      if (!strcasecmp("orange", colorstr)) {
        return &known_color_values[105];
      } else if (!strcasecmp("orangered", colorstr)) {
        return &known_color_values[106];
      } else if (!strcasecmp("orchid", colorstr)) {
        return &known_color_values[107];
      }
      return NULL;
    }
    return NULL;
  case 'p':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (!strcasecmp("palegoldenrod", colorstr)) {
        return &known_color_values[108];
      } else if (!strcasecmp("palegreen", colorstr)) {
        return &known_color_values[109];
      } else if (!strcasecmp("paleturquoise", colorstr)) {
        return &known_color_values[110];
      } else if (!strcasecmp("palevioletred", colorstr)) {
        return &known_color_values[111];
      } else if (!strcasecmp("papayawhip", colorstr)) {
        return &known_color_values[112];
      }
      return NULL;
    case 'e':
      if (!strcasecmp("peachpuff", colorstr)) {
        return &known_color_values[113];
      } else if (!strcasecmp("peru", colorstr)) {
        return &known_color_values[114];
      }
      return NULL;
    case 'i':
      if (!strcasecmp("pink", colorstr)) {
        return &known_color_values[115];
      }
      return NULL;
    case 'l':
      if (!strcasecmp("plum", colorstr)) {
        return &known_color_values[116];
      }
      return NULL;
    case 'o':
      if (!strcasecmp("powderblue", colorstr)) {
        return &known_color_values[117];
      }
      return NULL;
    case 'u':
      if (!strcasecmp("purple", colorstr)) {
        return &known_color_values[118];
      }
      return NULL;
    }
    return NULL;
  case 'r':
    if (!strcasecmp("red", colorstr)) {
      return &known_color_values[119];
    } else if (!strcasecmp("rosybrown", colorstr)) {
      return &known_color_values[120];
    } else if (!strcasecmp("royalblue", colorstr)) {
      return &known_color_values[121];
    }
    return NULL;
  case 's':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (!strcasecmp("saddlebrown", colorstr)) {
        return &known_color_values[122];
      } else if (!strcasecmp("salmon", colorstr)) {
        return &known_color_values[123];
      } else if (!strcasecmp("sandybrown", colorstr)) {
        return &known_color_values[124];
      }
      return NULL;
    case 'e':
      if (!strcasecmp("seagreen", colorstr)) {
        return &known_color_values[125];
      } else if (!strcasecmp("seashell", colorstr)) {
        return &known_color_values[126];
      }
      return NULL;
    case 'i':
      if (!strcasecmp("sienna", colorstr)) {
        return &known_color_values[127];
      } else if (!strcasecmp("silver", colorstr)) {
        return &known_color_values[128];
      }
      return NULL;
    case 'k':
      if (!strcasecmp("skyblue", colorstr)) {
        return &known_color_values[129];
      }
      return NULL;
    case 'l':
      if (!strcasecmp("slateblue", colorstr)) {
        return &known_color_values[130];
      } else if (!strcasecmp("slategray", colorstr)) {
        return &known_color_values[131];
      } else if (!strcasecmp("slategrey", colorstr)) {
        return &known_color_values[132];
      }
      return NULL;
    case 'n':
      if (!strcasecmp("snow", colorstr)) {
        return &known_color_values[133];
      }
      return NULL;
    case 'p':
      if (!strcasecmp("springgreen", colorstr)) {
        return &known_color_values[134];
      }
      return NULL;
    case 't':
      if (!strcasecmp("steelblue", colorstr)) {
        return &known_color_values[135];
      }
      return NULL;
    }
    return NULL;
  case 't':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (!strcasecmp("tan", colorstr)) {
        return &known_color_values[136];
      }
      return NULL;
    case 'e':
      if (!strcasecmp("teal", colorstr)) {
        return &known_color_values[137];
      }
      return NULL;
    case 'h':
      if (!strcasecmp("thistle", colorstr)) {
        return &known_color_values[138];
      }
      return NULL;
    case 'o':
      if (!strcasecmp("tomato", colorstr)) {
        return &known_color_values[139];
      }
      return NULL;
    case 'u':
      if (!strcasecmp("turquoise", colorstr)) {
        return &known_color_values[140];
      }
      return NULL;
    }
    return NULL;
  case 'v':
    if (!strcasecmp("violet", colorstr)) {
      return &known_color_values[141];
    }
    return NULL;
  case 'w':
    if (!strcasecmp("wheat", colorstr)) {
      return &known_color_values[142];
    } else if (!strcasecmp("white", colorstr)) {
      return &known_color_values[143];
    } else if (!strcasecmp("whitesmoke", colorstr)) {
      return &known_color_values[144];
    }
    return NULL;
  case 'y':
    if (!strcasecmp("yellow", colorstr)) {
      return &known_color_values[145];
    } else if (!strcasecmp("yellowgreen", colorstr)) {
      return &known_color_values[146];
    }
    return NULL;
  };

  return NULL;
}  // NOLINT

const unsigned char HtmlColor::kGoodColorValue;
const unsigned char HtmlColor::kBadColorName;
const unsigned char HtmlColor::kBadColorHex;

static inline int TwoXDigitsToNum(const char *xstr) {
  return (
      strings::hex_digit_to_int(
          xstr[0])*16 + strings::hex_digit_to_int(xstr[1]));
}

void HtmlColor::SetValueFromHexStr(const char *hexstr) {
  int hexstr_len = strlen(hexstr);
  const char* finalstr = hexstr;
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

HtmlColor::HtmlColor(const string& str) {
  SetValueFromStr(str.c_str());
}

HtmlColor::HtmlColor(const char *str, int colorstrlen) {
  string tmp(str, colorstrlen);
  SetValueFromStr(tmp.c_str());
}

void HtmlColor::SetValueFromStr(const char* colorstr) {
  if (colorstr[0] == '#') {
    // rgb value
    SetValueFromHexStr(colorstr + 1);
  } else {
    SetValueFromName(colorstr);
    if (!IsDefined() && strlen(colorstr) == 6) {
      SetValueFromHexStr(colorstr);
      if (!IsDefined())
        SetBadNameValue();
    }
  }
}

void HtmlColor::SetValueFromName(const char* str) {
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
string HtmlColorUtils::MaybeConvertToCssShorthand(const char* orig_color) {
  HtmlColor color(orig_color);
  if ( !color.IsDefined() )
    return orig_color;

  string shorthand = MaybeConvertToCssShorthand(color);
  if (shorthand.size() < strlen(orig_color)) {
    return shorthand;
  } else {
    return orig_color;
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
