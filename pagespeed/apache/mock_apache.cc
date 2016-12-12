// Copyright 2016 Google Inc.
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
//
// Author: jefftk@google.com (Jeff Kaufman)
#include "pagespeed/apache/mock_apache.h"

#include <cstdlib>
#include <vector>

#include "base/logging.h"
#include "pagespeed/apache/apache_httpd_includes.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/http_names.h"

#include "util_filter.h"  // NOLINT

namespace {

net_instaweb::StringVector* recorded_actions = NULL;
bool apr_initialized = false;

}  // namespace

namespace net_instaweb {

void MockApache::Initialize() {
  CHECK(recorded_actions == NULL);
  recorded_actions = new StringVector();
  if (!apr_initialized) {
    apr_initialize();
    atexit(apr_terminate);
    apr_initialized = true;
  }
}

void MockApache::Terminate() {
  CHECK(recorded_actions != NULL);
  if (!recorded_actions->empty()) {
    LOG(FATAL) << "MockApache: unprocessed actions: "
               << ActionsSinceLastCall();
  }
  delete recorded_actions;
  recorded_actions = NULL;
}

void MockApache::PrepareRequest(request_rec* request) {
  apr_pool_create(&request->pool, NULL);
  request->headers_in = apr_table_make(request->pool, 10);
  request->headers_out = apr_table_make(request->pool, 10);
  request->subprocess_env = apr_table_make(request->pool, 10);

  // Create three fake downstream filters so we can make sure the right ones are
  // removed.
  int n_fake_filters = 3;
  ap_filter_t** filter = &request->output_filters;
  for (int i = 0;
       i < n_fake_filters;
       ++i, filter = &(*filter)->next) {
    *filter = static_cast<ap_filter_t*>(
        apr_palloc(request->pool, sizeof(ap_filter_t)));
    (*filter)->frec = static_cast<ap_filter_rec_t*>(
        apr_palloc(request->pool, sizeof(ap_filter_rec_t)));
    const char* filter_name;
    switch (i) {
      case 0:
        filter_name = "MOD_EXPIRES";
        break;
      case 1:
        filter_name = "FIXUP_HEADERS_OUT";
        break;
      case 2:
        filter_name = "OTHER_FILTER";
        break;
      default:
        LOG(FATAL) << "There should only be three fake filters.";
    }
    (*filter)->frec->name = filter_name;
  }
  *filter = NULL;  // Terminate the linked list.
}

void MockApache::CleanupRequest(request_rec* request) {
  apr_pool_destroy(request->pool);
}

GoogleString MockApache::ActionsSinceLastCall() {
  CHECK(recorded_actions != NULL) <<
      "Must call MockApache::Initialize() first";
  GoogleString response = JoinCollection(*recorded_actions, " ");
  recorded_actions->clear();
  return response;
}

}  // namespace net_instaweb

namespace {

void log_action(StringPiece action) {
  CHECK(recorded_actions != NULL) <<
      "Must call MockApache::Initialize() first";
  recorded_actions->push_back(action.as_string());
}

void log_fatal(StringPiece function) {
  LOG(FATAL) << function << " should not be called";
}

}  // namespace

extern "C" {

int ap_rwrite(const void *buf, int nbyte, request_rec *r) {
  log_action(net_instaweb::StrCat(
      "ap_rwrite(", StringPiece(static_cast<const char*>(buf), nbyte), ")"));
  return 1;
}

int ap_rflush(request_rec* r) {
  log_action("ap_rflush()");
  return 1;
}

void ap_set_content_length(request_rec* r, apr_off_t length) {
  log_action(net_instaweb::StrCat(
      "ap_set_content_length(", net_instaweb::IntegerToString(length), ")"));
}

void ap_set_content_type(request_rec* r, const char* ct) {
  log_action(net_instaweb::StrCat(
      "ap_set_content_type(", StringPiece(ct), ")"));
  // Incomplete implementation, but enough to be testing.
  apr_table_set(
      r->headers_out, net_instaweb::HttpAttributes::kContentType, ct);
}

void ap_remove_output_filter(ap_filter_t* filter) {
  CHECK(filter != NULL);
  CHECK(filter->frec != NULL);
  log_action(net_instaweb::StrCat(
      "ap_remove_output_filter(", filter->frec->name, ")"));
}

ap_filter_t* ap_add_output_filter(const char*, void*, request_rec*, conn_rec*) {
  log_fatal("ap_add_output_filter");
  return NULL;
}

apr_status_t ap_get_brigade(ap_filter_t*, apr_bucket_brigade*,
                            ap_input_mode_t, apr_read_type_e, apr_off_t) {
  log_fatal("ap_get_brigade");
  return 0;
}

apr_status_t ap_pass_brigade(ap_filter_t*, apr_bucket_brigade*) {
  log_fatal("ap_pass_brigade");
  return 0;
}

ap_filter_rec_t* ap_register_output_filter(
    const char*, ap_out_filter_func, ap_init_filter_func, ap_filter_type) {
  log_fatal("ap_register_output_filter");
  return NULL;
}

ap_filter_rec_t* ap_register_input_filter(const char*, ap_in_filter_func,
                                          ap_init_filter_func, ap_filter_type) {
  log_fatal("ap_register_input_filter");
  return NULL;
}

#define IMPLEMENT_AS_LOG_FATAL(AP_X) \
  void AP_X() { log_fatal(#AP_X); }

IMPLEMENT_AS_LOG_FATAL(ap_build_cont_config)
IMPLEMENT_AS_LOG_FATAL(ap_check_cmd_context)
IMPLEMENT_AS_LOG_FATAL(ap_construct_url)
IMPLEMENT_AS_LOG_FATAL(ap_directory_walk)
IMPLEMENT_AS_LOG_FATAL(ap_hook_child_init)
IMPLEMENT_AS_LOG_FATAL(ap_hook_handler)
IMPLEMENT_AS_LOG_FATAL(ap_hook_log_transaction)
IMPLEMENT_AS_LOG_FATAL(ap_hook_map_to_storage)
IMPLEMENT_AS_LOG_FATAL(ap_hook_optional_fn_retrieve)
IMPLEMENT_AS_LOG_FATAL(ap_hook_post_config)
IMPLEMENT_AS_LOG_FATAL(ap_hook_post_read_request)
IMPLEMENT_AS_LOG_FATAL(ap_hook_translate_name)
IMPLEMENT_AS_LOG_FATAL(ap_log_error)
IMPLEMENT_AS_LOG_FATAL(ap_log_rerror)
IMPLEMENT_AS_LOG_FATAL(ap_mpm_query)
IMPLEMENT_AS_LOG_FATAL(ap_send_error_response)

// We need to define the unixd_config symbol to avoid link errors.  This is
// completely the wrong type, but our tests that use MockApache don't actually
// need unixd_config at all.
int unixd_config = 0;

}  // extern "C"
