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

#include "net/instaweb/apache/header_util.h"

namespace net_instaweb {

namespace {

int AddAttributeCallback(void *rec, const char *key, const char *value) {
  MetaData* meta_data = static_cast<MetaData*>(rec);
  meta_data->Add(key, value);
  return 1;
}

}  // namespace

// proto_num is the version number of protocol; 1.1 = 1001
void ApacheHeaderToMetaData(const apr_table_t* apache_headers,
                            int status_code,
                            int proto_num,
                            MetaData* meta_data) {
  meta_data->SetStatusAndReason(static_cast<HttpStatus::Code>(status_code));
  if (proto_num >= 1000) {
    meta_data->set_major_version(proto_num / 1000);
    meta_data->set_minor_version(proto_num % 1000);
  }
  apr_table_do(AddAttributeCallback, meta_data, apache_headers, NULL);
}

void MetaDataToApacheHeader(const MetaData& meta_data,
                            apr_table_t* apache_headers,
                            int* status_code,
                            int* proto_num) {
  *status_code = meta_data.status_code();
  *proto_num = (meta_data.major_version() * 1000) + meta_data.minor_version();
  for (int i = 0, n = meta_data.NumAttributes(); i < n; ++i) {
    apr_table_add(apache_headers, meta_data.Name(i), meta_data.Value(i));
  }
}

}  // namespace net_instaweb
