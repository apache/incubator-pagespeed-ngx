/*
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/resource_namer.h"

#include <cctype>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace net_instaweb {

namespace {

// The format of all resource names is:
//
//  ORIGINAL_NAME.pagespeed[.EXPT].ID.HASH.EXT
//
// "pagespeed" is what we'll call the system ID.  Rationale:
//   1. Any abbreviation of this will not be well known, e.g.
//         ps, mps (mod page speed), psa (page speed automatic)
//      and early reports from users indicate confusion over
//      the gibberish names in our resources.
//   2. "pagespeed" is the family of products now, not just the
//      firebug plug in.  Page Speed Automatic is the proper name for
//      the rewriting technology but it's longer, and "pagespeed" solves the
//      "WTF is this garbage in my URL" problem.
//   3. "mod_pagespeed" is slightly longer if/when this technology
//      is ported to other servers then the "mod_" is less relevant.
//
// EXPT is an optional character indicating the index of an ExperimentSpec.  The
// first ExperimentSpec is a, the next is b, ...  Users not in any experiment
// won't have this section.
//
// If you change this, or the structure of the encoded string,
// you will also need to change:
//
// automatic/system_test.sh
// system/system_test.sh
// apache/system_test.sh
//
// Plus a few constants in _test.cc files.

static const char kSystemId[] = "pagespeed";
static const char kSeparatorString[] = ".";
static const char kSeparatorChar = kSeparatorString[0];

}  // namespace

const int ResourceNamer::kOverhead = 4 + STATIC_STRLEN(kSystemId);

bool ResourceNamer::Decode(const StringPiece& encoded_string) {
  // Expected syntax:
  //   name.pagespeed[.experiment|.options].id.hash.ext
  // Note that 'name' and 'options' may have arbitrary numbers of dots, so
  // we parse by anchoring at the 'pagespeed', beginning, and end of the
  // StringPiece vector.

  StringPieceVector segments;
  SplitStringPieceToVector(encoded_string, kSeparatorString, &segments, false);
  int system_id_index = -1;
  int n = segments.size();
  for (int i = 0; i < n; ++i) {
    if (segments[i] == kSystemId) {
      system_id_index = i;
      break;
    }
  }

  experiment_.clear();
  options_.clear();

  // We expect at least one segment before the system-ID: the name.  We expect
  // at least 3 segments after it: the id, hash, and extension.  Extra segments
  // preceding the system-ID are part of the name.  Extra segments after the
  // system-ID are the options or experiments.  Options always are more than
  // one character, experiments always have 1 character.
  if ((system_id_index >= 1) &&      // at least 1 segment before the system ID.
      (n - system_id_index >= 4)) {  // at least 3 segments after the system ID.
    name_.clear();
    AppendJoinIterator(&name_,
                       segments.begin(), segments.begin() + system_id_index,
                       kSeparatorString);

    // Looking from the right, we should always see ext, hash, id
    segments[--n].CopyToString(&ext_);
    segments[--n].CopyToString(&hash_);
    segments[--n].CopyToString(&id_);

    // Now between system_id_index and n, we have the experiment or options.
    // Re-join them (general case includes dots for the options.
    int experiment_or_options_start = system_id_index + 1;
    if (experiment_or_options_start < n) {
      GoogleString experiment_or_options;
      AppendJoinIterator(
          &experiment_or_options,
          segments.begin() + experiment_or_options_start,
          segments.begin() + n,
          kSeparatorString);
      if (experiment_or_options.size() == 1) {
        if ((experiment_or_options[0] >= 'a') &&
            (experiment_or_options[0] <= 'z')) {
          experiment_or_options.swap(experiment_);
        } else {
          return false;  // invalid experiment
        }
      } else if (experiment_or_options.empty() ||
                 !UrlEscaper::DecodeFromUrlSegment(experiment_or_options,
                                                   &options_)) {
        return false;
      }
    }
    return true;
  }
  return LegacyDecode(encoded_string);
}

// TODO(jmarantz): validate that the 'id' is one of the filters that
// were implemented as of Nov 2010.  Also validate that the hash
// code is a 32-char hex number.
bool ResourceNamer::LegacyDecode(const StringPiece& encoded_string) {
  bool ret = false;
  // First check that this URL has a known extension type
  if (NameExtensionToContentType(encoded_string) != NULL) {
    StringPieceVector names;
    SplitStringPieceToVector(encoded_string, kSeparatorString, &names, true);
    if (names.size() == 4) {
      names[1].CopyToString(&hash_);

      // The legacy hash codes were all either 1-character (for tests) or
      // 32 characters, all in hex.
      if ((hash_.size() != 1) && (hash_.size() != 32)) {
        return false;
      }
      for (int i = 0, n = hash_.size(); i < n; ++i) {
        char ch = hash_[i];
        if (!isdigit(ch)) {
          ch = UpperChar(ch);
          if ((ch < 'A') || (ch > 'F')) {
            return false;
          }
        }
      }

      names[0].CopyToString(&id_);
      names[2].CopyToString(&name_);
      names[3].CopyToString(&ext_);
      ret = true;
    }
  }
  return ret;
}

// This is used for legacy compatibility as we transition to the grand new
// world.
GoogleString ResourceNamer::InternalEncode() const {
  StringPieceVector parts;
  GoogleString encoded_options;
  parts.push_back(name_);
  parts.push_back(kSystemId);
  DCHECK(!(has_experiment() && has_options()));
  if (has_experiment()) {
    parts.push_back(experiment_);
  } else if (has_options()) {
    UrlEscaper::EncodeToUrlSegment(options_, &encoded_options);
    parts.push_back(encoded_options);
  }
  parts.push_back(id_);
  parts.push_back(hash_);
  parts.push_back(ext_);
  return JoinCollection(parts, kSeparatorString);
}

// The current encoding assumes there are no dots in any of the components.
// This restriction may be relaxed in the future, but check it aggressively
// for now.
GoogleString ResourceNamer::Encode() const {
  DCHECK(StringPiece::npos == id_.find(kSeparatorChar));
  // It is OK for options_ to have separator characters because we
  // use the base UrlSegmentEncoder implementation, so we don't need
  // to run DCHECK(StringPiece::npos == options_.find(kSeparatorChar));
  DCHECK(!hash_.empty());
  DCHECK(StringPiece::npos == hash_.find(kSeparatorChar));
  DCHECK(StringPiece::npos == ext_.find(kSeparatorChar));
  DCHECK(StringPiece::npos == experiment_.find(kSeparatorChar));
  DCHECK(!has_experiment() || experiment_.length());
  DCHECK(!(has_experiment() && has_options()));
  return InternalEncode();
}

GoogleString ResourceNamer::EncodeIdName() const {
  CHECK(id_.find(kSeparatorChar) == StringPiece::npos);
  return StrCat(id_, kSeparatorString, name_);
}

const ContentType* ResourceNamer::ContentTypeFromExt() const {
  return NameExtensionToContentType(StrCat(".", ext_));
}

void ResourceNamer::CopyFrom(const ResourceNamer& other) {
  other.id().CopyToString(&id_);
  other.name().CopyToString(&name_);
  other.options().CopyToString(&options_);
  other.hash().CopyToString(&hash_);
  other.ext().CopyToString(&ext_);
  other.experiment().CopyToString(&experiment_);
}

int ResourceNamer::EventualSize(const Hasher& hasher) const {
  int e = name_.size() + id_.size() + ext_.size() + kOverhead +
      hasher.HashSizeInChars();
  if (has_experiment()) {
    e += 2;  // Experiment is one character, plus one for the separator.
  } else if (has_options()) {
    GoogleString encoded_options;
    UrlEscaper::EncodeToUrlSegment(options_, &encoded_options);
    e += 1 + encoded_options.size();  // add one for the separator.
  }
  return e;
}

}  // namespace net_instaweb
