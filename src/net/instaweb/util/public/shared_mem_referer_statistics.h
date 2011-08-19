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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_REFERER_STATISTICS_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_REFERER_STATISTICS_H_

#include <cstddef>
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractSharedMem;
class GoogleUrl;
class MessageHandler;
class SharedDynamicStringMap;
class Writer;

// This class handles persistent memory of referer statistics by wrapping a
// SharedDynamicStringMap by encoding references into string map entries and
// parsing these entries later.
//   GetEntryStringForUrl and GetDivLocationEntryStringForUrl can be overridden
// to tweak encodings of strings (say, if you want information to be hashed).
class SharedMemRefererStatistics {
 public:
  // All inputs are passed to SharedDynamicStringMap constructor.
  SharedMemRefererStatistics(size_t number_of_strings,
                             size_t average_string_length,
                             AbstractSharedMem* shm_runtime,
                             const GoogleString& filename_prefix,
                             const GoogleString& filename_suffix);
  virtual ~SharedMemRefererStatistics();

  // All inputs are passed to SharedDynamicStringMap method.
  bool InitSegment(bool parent, MessageHandler* message_handler);

  // The first LogPageRequest method is to be used when there is no referer,
  // and logs only the page visit.  The second logs the visit and the referral.
  //   LogResourceRequest logs only the referral.
  void LogPageRequestWithoutReferer(const GoogleUrl& target);
  void LogPageRequestWithReferer(const GoogleUrl& target,
                                 const GoogleUrl& referer);
  void LogResourceRequestWithReferer(const GoogleUrl& target,
                                     const GoogleUrl& referer);

  int GetNumberOfVisitsForUrl(const GoogleUrl& url);
  int GetNumberOfReferencesFromUrlToPage(const GoogleUrl& from_url,
                                         const GoogleUrl& to_url);
  int GetNumberOfReferencesFromUrlToDivLocation(
      const GoogleUrl& from_url, const GoogleString& div_location);
  int GetNumberOfReferencesFromUrlToResource(const GoogleUrl& from_url,
                                             const GoogleUrl& resource_url);

  // Extracts the div_location from the Url.
  static GoogleString GetDivLocationFromUrl(const GoogleUrl& url);

  // Calls shared_dynamic_string_map_->GlobalCleanup(message_handler)
  void GlobalCleanup(MessageHandler* message_handler);

  // Various methods of dumping information, that go from hard to understand and
  // cheap to well-organized and expensive:
  //   1. DumpFast writes SharedDynamicStringMap information in the order it
  //      was provided without parsing or decoding string entries (see Dump
  //      method of SharedDynamicStringMap)
  //
  //        Example:
  //          http://www.example.com/news: 1
  //          http://www.example.com/news/us: 1
  //          http://www.example.com/news/us phttp://www.example.com/news: 1
  //          1.1.2.0 dhttp://www.example.com/news: 1
  //          http://img.ex.com/news_us.jpg rhttp://www.example.com/news/us: 1
  //
  //   2. DumpSimple writes SharedDynamicStringMap information in the order it
  //      was provided, but it parses and decodes string entries into a more
  //      readable format (see DecodeEntry method below)
  //
  //        Example:
  //          http://www.example.com/news refered div location 1.1.2.0 : 1
  //          http://www.example.com/news/us refered resource
  //              http://img.ex.com/news_us.jpg : 1
  //          http://www.example.com/news visits: 1
  //          http://www.example.com/news/us visits: 1
  //          http://www.example.com/news refered page
  //              http://www.example.com/news/us : 1
  //
  //   3. DumpOrganized writes SharedDynamicStringMap information, grouped by
  //      referers, in alphabetical order.
  //
  //        Example:
  //          http://www.example.com/news visits: 1
  //          http://www.example.com/news refered:
  //            div location 1.1.2.0 : 1
  //            page http://ww.example.com/news/us : 1
  //          http://www.example.com/news/us visits: 1
  //          http://www.example.com/news/us refered:
  //            resource http://img.ex.com/news_us.jpg
  //
  void DumpFast(Writer* writer, MessageHandler* message_handler);
  void DumpSimple(Writer* writer, MessageHandler* message_handler);
  void DumpOrganized(Writer* writer, MessageHandler* message_handler);

  // The name for special div location query parameter
  static const char kParamName[];

 protected:
  // Given a Url string, produces the corresponding ready-for-storage string
  virtual GoogleString GetEntryStringForUrlString(
      const StringPiece& url_string) const;
  // Given a div location (string), produces the corresponding ready-for-storage
  // string
  virtual GoogleString GetEntryStringForDivLocation(
      const StringPiece& div_location) const;

 private:
  // Given a Url, extracts the div location and returns ready-for-storage string
  GoogleString GetUrlEntryStringForUrl(const GoogleUrl& url) const;
  // Given a Url, extracts the div location and returns ready-for-storage string
  GoogleString GetDivLocationEntryStringForUrl(const GoogleUrl& url) const;
  // These methods combine ready-for-storage strings into the final entry string
  static GoogleString GetEntryForReferedPage(const StringPiece& target,
                                             const StringPiece& referer);
  static GoogleString GetEntryForReferedDivLocation(const StringPiece& target,
                                                    const StringPiece& referer);
  static GoogleString GetEntryForVisitedPage(const StringPiece& target);
  static GoogleString GetEntryForReferedResource(const StringPiece& target,
                                                 const StringPiece& referer);
  // These methods extract the information encoded in the methods above and
  // return a more readable string.  The second method is a convenience method
  // for when we don't care about target, referer
  GoogleString DecodeEntry(const StringPiece& entry,
                           GoogleString* target,
                           GoogleString* referer) const;
  GoogleString DecodeEntry(const StringPiece& entry) const;

  // These helper methods cut down on duplicate code in the public Log methods
  void LogPageRequest(const GoogleUrl& target, GoogleString* target_string);

  scoped_ptr<SharedDynamicStringMap> shared_dynamic_string_map_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_REFERER_STATISTICS_H_
