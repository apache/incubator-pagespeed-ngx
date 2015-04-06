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

// Author: sligocki@google.com (Shawn Ligocki)

#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "webutil/css/parser.h"

int main(int argc, char** argv) {
  if (argc > 1) {
    std::string text(argv[1]);

    printf("Parsing:\n%s\n\n", text.c_str());
    Css::Parser parser(text);
    Css::Stylesheet* stylesheet = parser.ParseStylesheet();

    printf("Stylesheet:\n%s\n", stylesheet->ToString().c_str());

    delete stylesheet;
  }

  return EXIT_SUCCESS;
}
