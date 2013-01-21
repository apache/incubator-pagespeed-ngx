// Copyright 2010 Google Inc. All Rights Reserved.
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

// Author: aoates@google.com (Andrew Oates)

#ifndef PAGESPEED_L10N_L10N_H_
#define PAGESPEED_L10N_L10N_H_

#include "pagespeed/l10n/user_facing_string.h"

#if defined(_) || defined(not_localized)
#error localization macros are already defined.
#endif

// Macro for marking strings that need to be localized.  Any string
// that should be localized should be surrounded by _(...).
#define _(X) (::pagespeed::UserFacingString(X, true))

// Macro for marking user-facing strings that should *not* be localized.
#define not_localized(X) (::pagespeed::UserFacingString(X, false))

#endif  // PAGESPEED_L10N_L10N_H_
