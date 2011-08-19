/*
 * Copyright 2011 Google Inc.
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

// Author: jhoch@google.com (Jason Hoch)

#include "net/instaweb/util/public/shared_mem_referer_statistics.h"

#include <map>
#include <set>
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/shared_dynamic_string_map.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

// We don't want this to conflict with another query parameter name, and length
// also matters (shorter is better).  I picked this somewhat arbitrarily...
const char SharedMemRefererStatistics::kParamName[] = "div_location";

namespace {

// The encoding scheme for referrals is the following:
//   <target> + <separator string> + <type string> + <referer>
// where separator string is kSeparatorString and type string is either
// kPageString, kDivLocationString, or kResourceString, depending on the type
// of the target.
//   The type string is used to differentiate different types of targets at the
// time of decoding, while the separator string is used to make the information
// parseable.  Therefore the separator string has to be distinguishable from a
// URL (e.g. a space character, since there are no spaces in URLs).
const char kSeparatorString[] = " ";
const char kPageString[] = "p";
const char kDivLocationString[] = "d";
const char kResourceString[] = "r";

}  // namespace

SharedMemRefererStatistics::SharedMemRefererStatistics(
    size_t number_of_strings,
    size_t average_string_length,
    AbstractSharedMem* shm_runtime,
    const GoogleString& filename_prefix,
    const GoogleString& filename_suffix)
    : shared_dynamic_string_map_(new SharedDynamicStringMap(
          number_of_strings,
          average_string_length,
          shm_runtime,
          filename_prefix,
          filename_suffix)) {
}

SharedMemRefererStatistics::~SharedMemRefererStatistics() {}

bool SharedMemRefererStatistics::InitSegment(bool parent,
                                             MessageHandler* message_handler) {
  return shared_dynamic_string_map_->InitSegment(parent, message_handler);
}

void SharedMemRefererStatistics::LogPageRequestWithoutReferer(
    const GoogleUrl& target) {
  // We don't need to use target_string later, but we need a placeholder
  // variable.  We still use LogPageRequest(target, target_string) so we
  // don't duplicate code with LogReferedPageRequest
  GoogleString placeholder;
  LogPageRequest(target, &placeholder);
}

void SharedMemRefererStatistics::LogPageRequestWithReferer(
    const GoogleUrl& target,
    const GoogleUrl& referer) {
  GoogleString target_string;
  LogPageRequest(target, &target_string);
  GoogleString referer_string = GetUrlEntryStringForUrl(referer);
  GoogleString reference_entry = GetEntryForReferedPage(target_string,
                                                        referer_string);
  shared_dynamic_string_map_->IncrementElement(reference_entry);
  GoogleString div_location = GetDivLocationEntryStringForUrl(target);
  if (!div_location.empty()) {
    GoogleString div_location_entry =
        GetEntryForReferedDivLocation(div_location, referer_string);
    shared_dynamic_string_map_->IncrementElement(div_location_entry);
  }
}

void SharedMemRefererStatistics::LogResourceRequestWithReferer(
    const GoogleUrl& target,
    const GoogleUrl& referer) {
  GoogleString entry = GetEntryForReferedResource(
      GetUrlEntryStringForUrl(target),
      GetUrlEntryStringForUrl(referer));
  shared_dynamic_string_map_->IncrementElement(entry);
}

// We want to avoid duplicating code between LogReferedPageRequest and
// LogVisitedPageRequest, but we also don't want to perform GetEntryStringForUrl
// twice.
void SharedMemRefererStatistics::LogPageRequest(
    const GoogleUrl& target,
    GoogleString* target_string) {
  *target_string = GetUrlEntryStringForUrl(target);
  GoogleString visit_entry = GetEntryForVisitedPage(*target_string);
  shared_dynamic_string_map_->IncrementElement(visit_entry);
}

int SharedMemRefererStatistics::GetNumberOfVisitsForUrl(const GoogleUrl& url) {
  GoogleString entry = GetEntryForVisitedPage(GetUrlEntryStringForUrl(url));
  return shared_dynamic_string_map_->LookupElement(entry);
}

int SharedMemRefererStatistics::GetNumberOfReferencesFromUrlToPage(
    const GoogleUrl& from_url,
    const GoogleUrl& to_url) {
  GoogleString entry = GetEntryForReferedPage(
      GetUrlEntryStringForUrl(to_url),
      GetUrlEntryStringForUrl(from_url));
  return shared_dynamic_string_map_->LookupElement(entry);
}

int SharedMemRefererStatistics::GetNumberOfReferencesFromUrlToDivLocation(
    const GoogleUrl& from_url,
    const GoogleString& div_location) {
  GoogleString entry = GetEntryForReferedDivLocation(
      GetEntryStringForDivLocation(div_location),
      GetUrlEntryStringForUrl(from_url));
  return shared_dynamic_string_map_->LookupElement(entry);
}

int SharedMemRefererStatistics::GetNumberOfReferencesFromUrlToResource(
    const GoogleUrl& from_url,
    const GoogleUrl& resource_url) {
  GoogleString entry = GetEntryForReferedResource(
      GetUrlEntryStringForUrl(resource_url),
      GetUrlEntryStringForUrl(from_url));
  return shared_dynamic_string_map_->LookupElement(entry);
}

GoogleString SharedMemRefererStatistics::GetDivLocationFromUrl(
    const GoogleUrl& url) {
  QueryParams query_params;
  query_params.Parse(url.Query());
  StringStarVector div_locations;
  if ((query_params.Lookup(kParamName, &div_locations)) &&
      (!div_locations.empty())) {
    return *div_locations[0];
  }
  return "";
}

GoogleString SharedMemRefererStatistics::GetEntryStringForUrlString(
    const StringPiece& url_string) const {
  // Default implementation does nothing
  return url_string.as_string();
}

GoogleString SharedMemRefererStatistics::GetEntryStringForDivLocation(
    const StringPiece& div_location) const {
  // Default implementation does nothing
  return div_location.as_string();
}

GoogleString SharedMemRefererStatistics::GetUrlEntryStringForUrl(
    const GoogleUrl& url) const {
  return GetEntryStringForUrlString(url.AllExceptQuery());
}

GoogleString SharedMemRefererStatistics::GetDivLocationEntryStringForUrl(
    const GoogleUrl& url) const {
  return GetEntryStringForDivLocation(GetDivLocationFromUrl(url));
}

GoogleString SharedMemRefererStatistics::GetEntryForReferedPage(
    const StringPiece& target,
    const StringPiece& referer) {
  return StrCat(target, kSeparatorString, kPageString, referer);
}

GoogleString SharedMemRefererStatistics::GetEntryForReferedDivLocation(
    const StringPiece& target,
    const StringPiece& referer) {
  return StrCat(target, kSeparatorString, kDivLocationString, referer);
}

GoogleString SharedMemRefererStatistics::GetEntryForVisitedPage(
    const StringPiece& target) {
  return target.as_string();
}

GoogleString SharedMemRefererStatistics::GetEntryForReferedResource(
    const StringPiece& target,
    const StringPiece& referer) {
  return StrCat(target, kSeparatorString, kResourceString, referer);
}

GoogleString SharedMemRefererStatistics::DecodeEntry(
    const StringPiece& entry,
    GoogleString* target,
    GoogleString* referer) const {
  size_t separator_pos = entry.find(kSeparatorString);
  if (separator_pos == StringPiece::npos) {
    *target = entry.as_string();
    *referer = "";
    return StrCat(*target, " visits: ");
  } else {
    StringPiece basic_target(entry.data(), separator_pos);
    *referer = StringPiece(entry.data() + separator_pos + 2).as_string();
    char type_char = *(entry.data() + separator_pos + 1);
    const char* type_string = "";
    if (type_char == kPageString[0]) {
      type_string = "page ";
    } else if (type_char == kDivLocationString[0]) {
      type_string = "div location ";
    } else if (type_char == kResourceString[0]) {
      type_string = "resource ";
    }
    *target = StrCat(type_string, basic_target, " : ");
    return StrCat(*referer, " refered ", *target);
  }
}

GoogleString SharedMemRefererStatistics::DecodeEntry(
    const StringPiece& entry) const {
  GoogleString target;
  GoogleString referer;
  return DecodeEntry(entry, &target, &referer);
}

void SharedMemRefererStatistics::GlobalCleanup(
    MessageHandler* message_handler) {
  shared_dynamic_string_map_->GlobalCleanup(message_handler);
}

void SharedMemRefererStatistics::DumpFast(Writer* writer,
                                          MessageHandler* message_handler) {
  shared_dynamic_string_map_->Dump(writer, message_handler);
}

void SharedMemRefererStatistics::DumpSimple(Writer* writer,
                                            MessageHandler* message_handler) {
  StringSet strings;
  shared_dynamic_string_map_->GetKeys(&strings);
  for (StringSet::iterator i = strings.begin(); i != strings.end(); i++) {
    GoogleString string = *i;
    int value = shared_dynamic_string_map_->LookupElement(string);
    writer->Write(DecodeEntry(string), message_handler);
    writer->Write(IntegerToString(value), message_handler);
    writer->Write("\n", message_handler);
  }
}

void SharedMemRefererStatistics::DumpOrganized(
    Writer* writer,
    MessageHandler* message_handler) {
  StringSet strings;
  shared_dynamic_string_map_->GetKeys(&strings);
  // We first accumulate referers and group referrals by referer
  StringSet referers;
  std::map<GoogleString, StringSet> referees_by_referer;
  StringStringMap visits_by_referer;
  for (StringSet::iterator i = strings.begin(); i != strings.end(); i++) {
    GoogleString string = *i;
    int value = shared_dynamic_string_map_->LookupElement(string);
    GoogleString target;
    GoogleString referer;
    GoogleString output = DecodeEntry(string, &target, &referer);
    if (referer.empty()) {
      visits_by_referer[target] = StrCat(output, IntegerToString(value));
      referers.insert(target);
    } else {
      referees_by_referer[referer].insert(
          StrCat(target, IntegerToString(value)));
      referers.insert(referer);
    }
  }
  // We now dump the grouped referals in a nice readable format
  for (StringSet::iterator i = referers.begin(); i != referers.end(); i++) {
    GoogleString referer = *i;
    writer->Write(visits_by_referer[referer], message_handler);
    writer->Write("\n", message_handler);
    StringSet referees = referees_by_referer[referer];
    if (referees.size() > 0) {
      writer->Write(referer, message_handler);
      writer->Write(" refered:\n", message_handler);
      for (StringSet::iterator j = referees.begin(); j != referees.end(); j++) {
        GoogleString referee = *j;
        writer->Write("  ", message_handler);
        writer->Write(referee, message_handler);
        writer->Write("\n", message_handler);
      }
    }
  }
}

}  // namespace net_instaweb
