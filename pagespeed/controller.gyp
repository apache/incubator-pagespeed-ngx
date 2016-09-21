# Copyright 2015 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'variables': {
    'instaweb_root': '..',
    # Setting chromium_code to 1 turns on extra warnings. Also, if the compiler
    # is whitelisted in our common.gypi, those warnings will get treated as
    # errors.
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'pagespeed_controller_proto',
      'variables': {
        'instaweb_protoc_subdir': 'pagespeed/controller',
        'has_services': 1,
      },
      'sources': [
        'controller/controller.proto',
      ],
      'includes': [
        '../net/instaweb/protoc.gypi',
      ],
    },
    {
      'target_name': 'pagespeed_grpc_test_proto',
      'variables': {
        'instaweb_protoc_subdir': 'pagespeed/controller',
        'has_services': 1,
      },
      'sources': [
        'controller/grpc_test.proto',
      ],
      'includes': [
        '../net/instaweb/protoc.gypi',
      ],
    },
    {
      'target_name': 'pagespeed_controller',
      # xcode build names libraries just based on the target_name, so
      # if this were merely base then its libbase.a would clash with
      # Chromium libbase.a
      'type': '<(library)',
      'sources': [
        'controller/central_controller.cc',
        'controller/central_controller_rpc_client.cc',
        'controller/central_controller_rpc_server.cc',
        'controller/compatible_central_controller.cc',
        'controller/expensive_operation_callback.cc',
        'controller/expensive_operation_rpc_context.cc',
        'controller/expensive_operation_rpc_handler.cc',
        'controller/in_process_central_controller.cc',
        'controller/named_lock_schedule_rewrite_controller.cc',
        'controller/popularity_contest_schedule_rewrite_controller.cc',
        'controller/queued_expensive_operation_controller.cc',
        'controller/schedule_rewrite_callback.cc',
        'controller/schedule_rewrite_rpc_context.cc',
        'controller/schedule_rewrite_rpc_handler.cc',
        'controller/work_bound_expensive_operation_controller.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        ':pagespeed_controller_proto',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/grpc/grpc.gyp:grpc_cpp',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/base/base.gyp:base',
      ],
    },
  ],
}
