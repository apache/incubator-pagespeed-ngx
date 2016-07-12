# Copyright 2009 Google Inc.
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
  'type': '<(library)',
  'variables': {
    'has_services%': 0,
    # TODO(cheesy): Just remove these variables, since they are basically
    # global, plus we depend directly on protoc anyway.
    'protoc_executable%': '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
    'protoc_out_dir%': '<(SHARED_INTERMEDIATE_DIR)/protoc_out/instaweb',
  },
  'rules': [
    {
      'rule_name': 'genproto',
      'extension': 'proto',
      # Re-inject outputs of this rule as generated sources.
      'process_outputs_as_sources': 1,
      'variables': {
        'protoc_args': [
          '--proto_path=<(protoc_out_dir)/',
          '--cpp_out=<(protoc_out_dir)',
        ],
      },
      'inputs': [
        '<(protoc_executable)',
        '<(DEPTH)/build/fix_proto_and_invoke_protoc',
      ],
      'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
      'outputs': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/<(RULE_INPUT_ROOT).pb.h',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/<(RULE_INPUT_ROOT).pb.cc',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/<(RULE_INPUT_ROOT).proto',
      ],
      'conditions': [
        ['has_services != 0', {
          'variables': {
            'protoc_args': [
              '--plugin=protoc-gen-grpc=<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)grpc_cpp_plugin<(EXECUTABLE_SUFFIX)',
              '--grpc_out=services_namespace=grpc:<(protoc_out_dir)',
            ],
          },
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)grpc_cpp_plugin<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(protoc_out_dir)/<(instaweb_protoc_subdir)/<(RULE_INPUT_ROOT).grpc.pb.h',
            '<(protoc_out_dir)/<(instaweb_protoc_subdir)/<(RULE_INPUT_ROOT).grpc.pb.cc',
          ],
        }],
      ],
      'action': [
        '<(DEPTH)/build/fix_proto_and_invoke_protoc',
        '<(instaweb_root)/<(instaweb_protoc_subdir)/<(RULE_INPUT_NAME)',  # Input proto.
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/<(RULE_INPUT_ROOT).proto',  # Output proto.
        '<(protoc_executable)',
        '<@(protoc_args)',
      ],
    },
  ],
  'dependencies': [
    '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
    '<(DEPTH)/third_party/protobuf/protobuf.gyp:protoc#host',
  ],
  'conditions': [
    ['has_services != 0', {
      'dependencies': [
        '<(DEPTH)/third_party/grpc/grpc.gyp:grpc_cpp',  # For grpc headers.
        '<(DEPTH)/third_party/grpc/grpc.gyp:grpc_cpp_plugin#host',
      ],
    }],
  ],
  'include_dirs': [
    '<(protoc_out_dir)',
    '<(DEPTH)',
  ],
  'export_dependent_settings': [
    '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
  ],
  'hard_dependency': 1,
  'all_dependent_settings': {
    'hard_dependency': 1,
    'include_dirs': [
      '<(protoc_out_dir)',
      '<(DEPTH)/third_party/protobuf/src',
    ],
  },
}
