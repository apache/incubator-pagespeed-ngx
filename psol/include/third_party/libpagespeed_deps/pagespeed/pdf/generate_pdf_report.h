// Copyright 2011 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_PDF_GENERATE_PDF_REPORT_H_
#define PAGESPEED_PDF_GENERATE_PDF_REPORT_H_

#include <string>

namespace pagespeed {

class FormattedResults;

// Generate a PDF summarizing the results of a Page Speed run, and save the PDF
// to a file.
bool GeneratePdfReportToFile(const FormattedResults& results,
                             const std::string& path);

// TODO(mdsteele): We'll probably also be wanting a function that generates a
// PDF and writes it to memory.  Just a question of what the API should be.
// (e.g. Does it take an ostream?  An empty std::string?  Something else?)

}  // namespace pagespeed

#endif  // PAGESPEED_PDF_GENERATE_PDF_REPORT_H_
