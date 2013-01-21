// Copyright 2011 Google Inc.
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

#ifndef MOD_SPDY_COMMON_SPDY_SERVER_CONFIG_H_
#define MOD_SPDY_COMMON_SPDY_SERVER_CONFIG_H_

#include "base/basictypes.h"

namespace mod_spdy {

// Stores server configuration settings for our module.
class SpdyServerConfig {
 public:
  SpdyServerConfig();
  ~SpdyServerConfig();

  // Return true if SPDY is enabled for this server, false otherwise.
  bool spdy_enabled() const { return spdy_enabled_.get(); }

  // Return the maximum number of simultaneous SPDY streams that should be
  // permitted for a single client connection.
  int max_streams_per_connection() const {
    return max_streams_per_connection_.get();
  }

  // Return the minimum number of worker threads to spawn per child process.
  int min_threads_per_process() const {
    return min_threads_per_process_.get();
  }

  // Return the maximum number of worker threads to spawn per child process.
  int max_threads_per_process() const {
    return max_threads_per_process_.get();
  }

  // Return the maximum number of recursive levels to follow
  // X-Associated-Content headers
  int max_server_push_depth() const {
    return max_server_push_depth_.get();
  }

  // If nonzero, assume (unencrypted) SPDY/x for non-SSL connections, where x
  // is the version number returned here.  This will most likely break normal
  // browsers, but is useful for testing.
  int use_spdy_version_without_ssl() const {
    return use_spdy_version_without_ssl_.get();
  }

  // Return the maximum VLOG level we should use.
  int vlog_level() const { return vlog_level_.get(); }

  // Setters.  Call only during the configuration phase.
  void set_spdy_enabled(bool b) { spdy_enabled_.set(b); }
  void set_max_streams_per_connection(int n) {
    max_streams_per_connection_.set(n);
  }
  void set_min_threads_per_process(int n) {
    min_threads_per_process_.set(n);
  }
  void set_max_threads_per_process(int n) {
    max_threads_per_process_.set(n);
  }
  void set_max_server_push_depth(int n) {
    max_server_push_depth_.set(n);
  }
  void set_use_spdy_version_without_ssl(int n) {
    use_spdy_version_without_ssl_.set(n);
  }
  void set_vlog_level(int n) { vlog_level_.set(n); }

  // Set this config object to the merge of a and b.  Call only during the
  // configuration phase.
  void MergeFrom(const SpdyServerConfig& a, const SpdyServerConfig& b);

 private:
  template <typename T>
  class Option {
   public:
    explicit Option(const T& default_value)
        : was_set_(false), value_(default_value) {}
    const T& get() const { return value_; }
    void set(const T& value) { was_set_ = true; value_ = value; }
    void MergeFrom(const Option<T>& a, const Option<T>& b) {
      was_set_ = a.was_set_ || b.was_set_;
      value_ = a.was_set_ ? a.value_ : b.value_;
    }
   private:
    bool was_set_;
    T value_;
    DISALLOW_COPY_AND_ASSIGN(Option);
  };

  // Configuration fields:
  Option<bool> spdy_enabled_;
  Option<int> max_streams_per_connection_;
  Option<int> min_threads_per_process_;
  Option<int> max_threads_per_process_;
  Option<int> max_server_push_depth_;
  Option<int> use_spdy_version_without_ssl_;
  Option<int> vlog_level_;
  // Note: Add more config options here as needed; be sure to also update the
  //   MergeFrom method in spdy_server_config.cc.

  DISALLOW_COPY_AND_ASSIGN(SpdyServerConfig);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_CONTEXT_SPDY_SERVER_CONFIG_H_
