// Copyright 2010 Google Inc.
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
// Author: jmarantz@google.com (Joshua Marantz)

#include "pagespeed/apache/apache_config.h"

#include "base/logging.h"
#include "net/instaweb/public/version.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

namespace {

const char kModPagespeedStatisticsHandlerPath[] = "/mod_pagespeed_statistics";
const char kProxyAuth[] = "ProxyAuth";
const char kForceBuffering[] = "ForceBuffering";
const char kProxyAllRequests[] = "ExperimentalProxyAllRequests";
const char kMeasurementProxy[] = "ExperimentalMeasurementProxy";

}  // namespace

RewriteOptions::Properties* ApacheConfig::apache_properties_ = nullptr;

void ApacheConfig::Initialize() {
  if (Properties::Initialize(&apache_properties_)) {
    SystemRewriteOptions::Initialize();
    AddProperties();
  }
}

void ApacheConfig::Terminate() {
  if (Properties::Terminate(&apache_properties_)) {
    SystemRewriteOptions::Terminate();
  }
}

ApacheConfig::ApacheConfig(const StringPiece& description,
                           ThreadSystem* thread_system)
    : SystemRewriteOptions(description, thread_system) {
  Init();
}

ApacheConfig::ApacheConfig(ThreadSystem* thread_system)
    : SystemRewriteOptions(thread_system) {
  Init();
}

ApacheConfig::~ApacheConfig() {
}

void ApacheConfig::Init() {
  DCHECK(apache_properties_ != NULL)
      << "Call ApacheConfig::Initialize() before construction";
  InitializeOptions(apache_properties_);
}

void ApacheConfig::AddProperties() {
  AddApacheProperty(
      "", &ApacheConfig::proxy_auth_, "prxa",
      kProxyAuth,
      "CookieName[=Value][:RedirectUrl] -- checks proxy requests for "
      "CookieName.  If CookieValue is specified, checks for that.  If "
      "Redirect is specified, a failure results in a redirection to that URL "
      "otherwise a 403 is generated.",
      false /* safe_to_print */);

  AddApacheProperty(
      false, &ApacheConfig::force_buffering_, "afb",
      kForceBuffering,
      "Force buffering of non-html fetch responses rather than streaming",
      true /* safe_to_print */);

  AddApacheProperty(
      false, &ApacheConfig::proxy_all_requests_mode_, "aparx",
      kProxyAllRequests,
      "Experimental mode where mod_pagespeed acts entirely as a proxy, and "
      "doesn't attempt to work with any local serving. ",
      false /* safe_to_print*/);

  // Register deprecated options.
  AddDeprecatedProperty("CollectRefererStatistics",
                        RewriteOptions::kDirectoryScope);
  AddDeprecatedProperty("HashRefererStatistics",
                        RewriteOptions::kDirectoryScope);
  AddDeprecatedProperty("RefererStatisticsOutputLevel",
                        RewriteOptions::kDirectoryScope);
  AddDeprecatedProperty("StatisticsLoggingFile",
                        RewriteOptions::kDirectoryScope);
  AddDeprecatedProperty("DisableForBots",
                        RewriteOptions::kDirectoryScope);
  AddDeprecatedProperty("GeneratedFilePrefix",
                        RewriteOptions::kServerScope);
  AddDeprecatedProperty("InheritVHostConfig",
                        RewriteOptions::kServerScope);
  AddDeprecatedProperty("FetchFromModSpdy",
                        RewriteOptions::kServerScope);
  AddDeprecatedProperty("NumShards", RewriteOptions::kServerScope);
  AddDeprecatedProperty("UrlPrefix", RewriteOptions::kServerScope);

  MergeSubclassProperties(apache_properties_);

  // Default properties are global but to set them the current API requires
  // an ApacheConfig instance and we're in a static method.
  //
  // TODO(jmarantz): Perform these operations on the Properties directly and
  // get rid of this hack.
  //
  // Instantiation of the options with a null thread system wouldn't usually be
  // safe but it's ok here because we're only updating the static properties on
  // process startup.  We won't have a thread-system yet or multiple threads.
  ApacheConfig config("dummy_options", NULL);
  config.set_default_x_header_value(kModPagespeedVersion);
}

ApacheConfig* ApacheConfig::Clone() const {
  ApacheConfig* options =
      new ApacheConfig(StrCat("cloned from ", description()), thread_system());
  options->Merge(*this);
  return options;
}

ApacheConfig* ApacheConfig::NewOptions() const {
  return new ApacheConfig(StrCat("derived from ", description()),
                          thread_system());
}

const ApacheConfig* ApacheConfig::DynamicCast(const RewriteOptions* instance) {
  const ApacheConfig* config = dynamic_cast<const ApacheConfig*>(instance);
  DCHECK(config != NULL);
  return config;
}

ApacheConfig* ApacheConfig::DynamicCast(RewriteOptions* instance) {
  ApacheConfig* config = dynamic_cast<ApacheConfig*>(instance);
  DCHECK(config != NULL);
  return config;
}

void ApacheConfig::Merge(const RewriteOptions& src) {
  SystemRewriteOptions::Merge(src);
  const ApacheConfig* asrc = DynamicCast(&src);
  CHECK(asrc != NULL);

  // Can't use Merge() since we don't have names here.
  measurement_proxy_root_.MergeHelper(&asrc->measurement_proxy_root_);
  measurement_proxy_password_.MergeHelper(&asrc->measurement_proxy_password_);
}

RewriteOptions::OptionSettingResult ApacheConfig::ParseAndSetOptionFromName2(
    StringPiece name, StringPiece arg1, StringPiece arg2,
    GoogleString* msg, MessageHandler* handler) {
  OptionSettingResult result = SystemRewriteOptions::ParseAndSetOptionFromName2(
      name, arg1, arg2, msg, handler);
  if (result == RewriteOptions::kOptionNameUnknown) {
    if (name == kMeasurementProxy) {
      arg1.CopyToString(&measurement_proxy_root_.mutable_value());
      arg2.CopyToString(&measurement_proxy_password_.mutable_value());
      result = RewriteOptions::kOptionOk;
    }
  }
  return result;
}

GoogleString ApacheConfig::SubclassSignatureLockHeld() {
  return StrCat(SystemRewriteOptions::SubclassSignatureLockHeld(),
                "_MPR:", measurement_proxy_root_.value(),
                "_MPP:", measurement_proxy_password_.value());
}

bool ApacheConfig::GetProxyAuth(StringPiece* name, StringPiece* value,
                                StringPiece* redirect) const {
  StringPiece auth = proxy_auth_.value();
  TrimWhitespace(&auth);
  if (auth.empty()) {
    return false;
  }

  // Strip the redirect off the tail if a colon is present.  Note that
  // a colon may exist in the redirect URL but we search from the beginning
  // so it's no problem.
  stringpiece_ssize_type colon = auth.find(':');
  if (colon == StringPiece::npos) {
    *redirect = StringPiece();
  } else {
    *redirect = auth.substr(colon + 1);
    auth = auth.substr(0, colon);
  }

  // Split into name/value if an equals is present.
  stringpiece_ssize_type equals = auth.find('=');
  if (equals == StringPiece::npos) {
    *name = auth;
    *value = StringPiece();
  } else {
    *name = auth.substr(0, equals);
    *value = auth.substr(equals + 1);
  }
  return true;
}

}  // namespace net_instaweb
