# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['use_system_protobuf==0', {
      'conditions': [
        ['OS!="win"', {
          'variables': {
            'config_h_dir':
              '.',  # crafted for gcc/linux.
          },
        }, {  # else, OS=="win"
          'variables': {
            'config_h_dir':
              'vsprojects',  # crafted for msvc.
          },
          'target_defaults': {
            'msvs_disabled_warnings': [
              4018,  # signed/unsigned mismatch in comparison
              4244,  # implicit conversion, possible loss of data
              4355,  # 'this' used in base member initializer list
              4267,  # size_t to int truncation
              4291,  # no matching operator delete for a placement new
            ],
            'defines!': [
              'WIN32_LEAN_AND_MEAN',  # Protobuf defines this itself.
            ],
          },
        }],
        ['OS=="android"', {
          'targets': [
            {
              'target_name': 'protobuf_lite_javalib',
              'type' : 'none',
              'dependencies': [
                'protoc#host',
              ],
              'variables': {
                'script_descriptors': './protobuf_lite_java_descriptor_proto.py',
                'script_pom': './protobuf_lite_java_parse_pom.py',
                'protoc': '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
                # Variables needed by java.gypi below.
                'java_out_dir': '<(PRODUCT_DIR)/java_proto/protobuf_lite_java_descriptor_proto',
                'generated_src_dirs': ['<(java_out_dir)'],
                'java_in_dir': 'java',
                'maven_pom': '<(java_in_dir)/pom.xml',
                'javac_includes': ['<!@(<(script_pom) <(maven_pom))'],
                'additional_input_paths': [
                  '<(java_out_dir)/com/google/protobuf/DescriptorProtos.java'
                ],
              },
              'actions': [
                {
                  'action_name': 'protobuf_lite_java_gen_descriptor_proto',
                  'inputs': [
                    '<(script_descriptors)',
                    '<(protoc)',
                    'src/src/google/protobuf/descriptor.proto',
                  ],
                  'outputs': [
                    '<(java_out_dir)/com/google/protobuf/DescriptorProtos.java',
                  ],
                  'action': [
                    '<(script_descriptors)',
                    '<(protoc)',
                    '<(java_out_dir)',
                    'src',
                    'src/src/google/protobuf/descriptor.proto',
                  ],
                  'message': 'Generating descriptor protos for Java',
                },
              ],
              # Now that we have generated DescriptorProtos.java, build jar.
              'includes': ['../../build/java.gypi'],
            },
          ]
        }],
      ],
      'targets': [
        # The "lite" lib is about 1/7th the size of the heavy lib,
        # but it doesn't support some of the more exotic features of
        # protobufs, like reflection.  To generate C++ code that can link
        # against the lite version of the library, add the option line:
        #
        #   option optimize_for = LITE_RUNTIME;
        #
        # to your .proto file.
        {
          'target_name': 'protobuf_lite',
          'type': '<(component)',
          'toolsets': ['host', 'target'],
          'includes': [
            'protobuf_lite.gypi',
          ],
          # Required for component builds. See http://crbug.com/172800.
          'defines': [
            'LIBPROTOBUF_EXPORTS',
            'PROTOBUF_USE_DLLS',
          ],
          'direct_dependent_settings': {
            'defines': [
              'PROTOBUF_USE_DLLS',
            ],
          },
        },
        # This is the full, heavy protobuf lib that's needed for c++ .protos
        # that don't specify the LITE_RUNTIME option.  The protocol
        # compiler itself (protoc) falls into that category.
        #
        # DO NOT LINK AGAINST THIS TARGET IN CHROME CODE  --agl
        {
          'target_name': 'protobuf_full_do_not_use',
          'type': 'static_library',
          'toolsets': ['host','target'],
          'includes': [
            'protobuf_lite.gypi',
          ],
          'sources': [
            'src/src/google/protobuf/descriptor.h',
            'src/src/google/protobuf/descriptor.pb.h',
            'src/src/google/protobuf/descriptor_database.h',
            'src/src/google/protobuf/dynamic_message.h',
            'src/src/google/protobuf/generated_enum_reflection.h',
            'src/src/google/protobuf/generated_message_reflection.h',
            'src/src/google/protobuf/message.h',
            'src/src/google/protobuf/reflection_ops.h',
            'src/src/google/protobuf/service.h',
            'src/src/google/protobuf/text_format.h',
            'src/src/google/protobuf/wire_format.h',
            'src/src/google/protobuf/io/gzip_stream.h',
            'src/src/google/protobuf/io/printer.h',
            'src/src/google/protobuf/io/tokenizer.h',
            'src/src/google/protobuf/io/zero_copy_stream_impl.h',
            'src/src/google/protobuf/compiler/code_generator.h',
            'src/src/google/protobuf/compiler/command_line_interface.h',
            'src/src/google/protobuf/compiler/importer.h',
            'src/src/google/protobuf/compiler/java/java_doc_comment.cc',
            'src/src/google/protobuf/compiler/java/java_doc_comment.h',
            'src/src/google/protobuf/compiler/java/java_extension_lite.cc',
            'src/src/google/protobuf/compiler/java/java_extension_lite.h',
            'src/src/google/protobuf/compiler/parser.h',

            'src/src/google/protobuf/stubs/int128.cc',
            'src/src/google/protobuf/stubs/int128.h',
            'src/src/google/protobuf/stubs/strutil.cc',
            'src/src/google/protobuf/stubs/strutil.h',
            'src/src/google/protobuf/stubs/substitute.cc',
            'src/src/google/protobuf/stubs/substitute.h',
            'src/src/google/protobuf/stubs/stl_util.h',
            'src/src/google/protobuf/stubs/status.cc',
            'src/src/google/protobuf/stubs/status.h',
            'src/src/google/protobuf/stubs/stringpiece.cc',
            'src/src/google/protobuf/stubs/stringpiece.h',
            'src/src/google/protobuf/stubs/stringprintf.cc',
            'src/src/google/protobuf/stubs/stringprintf.h',
            'src/src/google/protobuf/stubs/structurally_valid.cc',
            'src/src/google/protobuf/stubs/template_util.h',
            'src/src/google/protobuf/stubs/type_traits.h',

            'src/src/google/protobuf/any.cc',
            'src/src/google/protobuf/arena.cc',
            'src/src/google/protobuf/arenastring.cc',
            'src/src/google/protobuf/descriptor.cc',
            'src/src/google/protobuf/descriptor.pb.cc',
            'src/src/google/protobuf/descriptor_database.cc',
            'src/src/google/protobuf/dynamic_message.cc',
            'src/src/google/protobuf/extension_set.cc',
            'src/src/google/protobuf/extension_set.h',
            'src/src/google/protobuf/extension_set_heavy.cc',
            'src/src/google/protobuf/generated_message_reflection.cc',
            'src/src/google/protobuf/message.cc',
            'src/src/google/protobuf/map_field.cc',
            'src/src/google/protobuf/reflection_ops.cc',
            'src/src/google/protobuf/service.cc',
            'src/src/google/protobuf/text_format.cc',
            'src/src/google/protobuf/wire_format.cc',
            # This file pulls in zlib, but it's not actually used by protoc, so
            # instead of compiling zlib for the host, let's just exclude this.
            # 'src/src/src/google/protobuf/io/gzip_stream.cc',
            'src/src/google/protobuf/io/printer.cc',
            'src/src/google/protobuf/io/strtod.cc',
            'src/src/google/protobuf/io/tokenizer.cc',
            'src/src/google/protobuf/io/zero_copy_stream_impl.cc',
            'src/src/google/protobuf/compiler/importer.cc',
            'src/src/google/protobuf/compiler/parser.cc',
          ],
          'include_dirs': [
            '<(config_h_dir)',
            'src/src',
          ],
          'all_dependent_settings': {
            'include_dirs': [
              'src/src',
            ],
          },
        },
        {
          'target_name': 'protobuf_message_differencer',
          'type': 'static_library',
          'toolsets': ['target'],
          'dependencies': [
            'protobuf_full_do_not_use',
          ],
          'sources': [
            'src/src/google/protobuf/util/field_comparator.cc',
            'src/src/google/protobuf/util/message_differencer.cc',
          ],
          'include_dirs': [
            '<(config_h_dir)',
            'src/src',
          ],
          'all_dependent_settings': {
            'include_dirs': [
              'src/src',
            ],
          },
        },
        {
          'target_name': 'protoc_lib',
          'type': '<(library)',
          'toolsets': ['host'],
          'sources': [
            'src/src/google/protobuf/compiler/code_generator.cc',
            'src/src/google/protobuf/compiler/command_line_interface.cc',
            'src/src/google/protobuf/compiler/plugin.cc',
            'src/src/google/protobuf/compiler/plugin.pb.cc',
            'src/src/google/protobuf/compiler/subprocess.cc',
            'src/src/google/protobuf/compiler/subprocess.h',
            'src/src/google/protobuf/compiler/zip_writer.cc',
            'src/src/google/protobuf/compiler/zip_writer.h',
            'src/src/google/protobuf/compiler/cpp/cpp_enum.cc',
            'src/src/google/protobuf/compiler/cpp/cpp_enum.h',
            'src/src/google/protobuf/compiler/cpp/cpp_enum_field.cc',
            'src/src/google/protobuf/compiler/cpp/cpp_enum_field.h',
            'src/src/google/protobuf/compiler/cpp/cpp_extension.cc',
            'src/src/google/protobuf/compiler/cpp/cpp_extension.h',
            'src/src/google/protobuf/compiler/cpp/cpp_field.cc',
            'src/src/google/protobuf/compiler/cpp/cpp_field.h',
            'src/src/google/protobuf/compiler/cpp/cpp_file.cc',
            'src/src/google/protobuf/compiler/cpp/cpp_file.h',
            'src/src/google/protobuf/compiler/cpp/cpp_generator.cc',
            'src/src/google/protobuf/compiler/cpp/cpp_helpers.cc',
            'src/src/google/protobuf/compiler/cpp/cpp_helpers.h',
            'src/src/google/protobuf/compiler/cpp/cpp_map_field.cc',
            'src/src/google/protobuf/compiler/cpp/cpp_map_field.h',
            'src/src/google/protobuf/compiler/cpp/cpp_message.cc',
            'src/src/google/protobuf/compiler/cpp/cpp_message.h',
            'src/src/google/protobuf/compiler/cpp/cpp_message_field.cc',
            'src/src/google/protobuf/compiler/cpp/cpp_message_field.h',
            'src/src/google/protobuf/compiler/cpp/cpp_primitive_field.cc',
            'src/src/google/protobuf/compiler/cpp/cpp_primitive_field.h',
            'src/src/google/protobuf/compiler/cpp/cpp_service.cc',
            'src/src/google/protobuf/compiler/cpp/cpp_service.h',
            'src/src/google/protobuf/compiler/cpp/cpp_string_field.cc',
            'src/src/google/protobuf/compiler/cpp/cpp_string_field.h',
            'src/src/google/protobuf/compiler/csharp/csharp_doc_comment.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_doc_comment.h',
            'src/src/google/protobuf/compiler/csharp/csharp_enum.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_enum_field.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_enum_field.h',
            'src/src/google/protobuf/compiler/csharp/csharp_enum.h',
            'src/src/google/protobuf/compiler/csharp/csharp_field_base.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_field_base.h',
            'src/src/google/protobuf/compiler/csharp/csharp_generator.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_generator.h',
            'src/src/google/protobuf/compiler/csharp/csharp_helpers.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_helpers.h',
            'src/src/google/protobuf/compiler/csharp/csharp_map_field.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_map_field.h',
            'src/src/google/protobuf/compiler/csharp/csharp_message.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_message_field.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_message_field.h',
            'src/src/google/protobuf/compiler/csharp/csharp_message.h',
            'src/src/google/protobuf/compiler/csharp/csharp_names.h',
            'src/src/google/protobuf/compiler/csharp/csharp_primitive_field.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_primitive_field.h',
            'src/src/google/protobuf/compiler/csharp/csharp_reflection_class.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_reflection_class.h',
            'src/src/google/protobuf/compiler/csharp/csharp_repeated_enum_field.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_repeated_enum_field.h',
            'src/src/google/protobuf/compiler/csharp/csharp_repeated_message_field.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_repeated_message_field.h',
            'src/src/google/protobuf/compiler/csharp/csharp_repeated_primitive_field.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_repeated_primitive_field.h',
            'src/src/google/protobuf/compiler/csharp/csharp_source_generator_base.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_source_generator_base.h',
            'src/src/google/protobuf/compiler/csharp/csharp_wrapper_field.cc',
            'src/src/google/protobuf/compiler/csharp/csharp_wrapper_field.h',
            'src/src/google/protobuf/compiler/java/java_context.cc',
            'src/src/google/protobuf/compiler/java/java_context.h',
            'src/src/google/protobuf/compiler/java/java_doc_comment.cc',
            'src/src/google/protobuf/compiler/java/java_doc_comment.h',
            'src/src/google/protobuf/compiler/java/java_enum.cc',
            'src/src/google/protobuf/compiler/java/java_enum_field.cc',
            'src/src/google/protobuf/compiler/java/java_enum_field.h',
            'src/src/google/protobuf/compiler/java/java_enum_field_lite.cc',
            'src/src/google/protobuf/compiler/java/java_enum_field_lite.h',
            'src/src/google/protobuf/compiler/java/java_enum.h',
            'src/src/google/protobuf/compiler/java/java_enum_lite.cc',
            'src/src/google/protobuf/compiler/java/java_enum_lite.h',
            'src/src/google/protobuf/compiler/java/java_extension.cc',
            'src/src/google/protobuf/compiler/java/java_extension.h',
            'src/src/google/protobuf/compiler/java/java_field.cc',
            'src/src/google/protobuf/compiler/java/java_field.h',
            'src/src/google/protobuf/compiler/java/java_file.cc',
            'src/src/google/protobuf/compiler/java/java_file.h',
            'src/src/google/protobuf/compiler/java/java_generator.cc',
            'src/src/google/protobuf/compiler/java/java_generator_factory.cc',
            'src/src/google/protobuf/compiler/java/java_generator_factory.h',
            'src/src/google/protobuf/compiler/java/java_generator.h',
            'src/src/google/protobuf/compiler/java/java_helpers.cc',
            'src/src/google/protobuf/compiler/java/java_helpers.h',
            'src/src/google/protobuf/compiler/java/java_lazy_message_field.cc',
            'src/src/google/protobuf/compiler/java/java_lazy_message_field.h',
            'src/src/google/protobuf/compiler/java/java_lazy_message_field_lite.cc',
            'src/src/google/protobuf/compiler/java/java_lazy_message_field_lite.h',
            'src/src/google/protobuf/compiler/java/java_map_field.cc',
            'src/src/google/protobuf/compiler/java/java_map_field.h',
            'src/src/google/protobuf/compiler/java/java_map_field_lite.cc',
            'src/src/google/protobuf/compiler/java/java_map_field_lite.h',
            'src/src/google/protobuf/compiler/java/java_message_builder.cc',
            'src/src/google/protobuf/compiler/java/java_message_builder.h',
            'src/src/google/protobuf/compiler/java/java_message_builder_lite.cc',
            'src/src/google/protobuf/compiler/java/java_message_builder_lite.h',
            'src/src/google/protobuf/compiler/java/java_message.cc',
            'src/src/google/protobuf/compiler/java/java_message_field.cc',
            'src/src/google/protobuf/compiler/java/java_message_field.h',
            'src/src/google/protobuf/compiler/java/java_message_field_lite.cc',
            'src/src/google/protobuf/compiler/java/java_message_field_lite.h',
            'src/src/google/protobuf/compiler/java/java_message.h',
            'src/src/google/protobuf/compiler/java/java_message_lite.cc',
            'src/src/google/protobuf/compiler/java/java_message_lite.h',
            'src/src/google/protobuf/compiler/java/java_name_resolver.cc',
            'src/src/google/protobuf/compiler/java/java_name_resolver.h',
            'src/src/google/protobuf/compiler/java/java_names.h',
            'src/src/google/protobuf/compiler/java/java_primitive_field.cc',
            'src/src/google/protobuf/compiler/java/java_primitive_field.h',
            'src/src/google/protobuf/compiler/java/java_primitive_field_lite.cc',
            'src/src/google/protobuf/compiler/java/java_primitive_field_lite.h',
            'src/src/google/protobuf/compiler/java/java_service.cc',
            'src/src/google/protobuf/compiler/java/java_service.h',
            'src/src/google/protobuf/compiler/java/java_shared_code_generator.cc',
            'src/src/google/protobuf/compiler/java/java_shared_code_generator.h',
            'src/src/google/protobuf/compiler/java/java_string_field.cc',
            'src/src/google/protobuf/compiler/java/java_string_field.h',
            'src/src/google/protobuf/compiler/java/java_string_field_lite.cc',
            'src/src/google/protobuf/compiler/java/java_string_field_lite.h',
            'src/src/google/protobuf/compiler/javanano/javanano_enum.cc',
            'src/src/google/protobuf/compiler/javanano/javanano_enum_field.cc',
            'src/src/google/protobuf/compiler/javanano/javanano_enum_field.h',
            'src/src/google/protobuf/compiler/javanano/javanano_enum.h',
            'src/src/google/protobuf/compiler/javanano/javanano_extension.cc',
            'src/src/google/protobuf/compiler/javanano/javanano_extension.h',
            'src/src/google/protobuf/compiler/javanano/javanano_field.cc',
            'src/src/google/protobuf/compiler/javanano/javanano_field.h',
            'src/src/google/protobuf/compiler/javanano/javanano_file.cc',
            'src/src/google/protobuf/compiler/javanano/javanano_file.h',
            'src/src/google/protobuf/compiler/javanano/javanano_generator.cc',
            'src/src/google/protobuf/compiler/javanano/javanano_generator.h',
            'src/src/google/protobuf/compiler/javanano/javanano_helpers.cc',
            'src/src/google/protobuf/compiler/javanano/javanano_helpers.h',
            'src/src/google/protobuf/compiler/javanano/javanano_map_field.cc',
            'src/src/google/protobuf/compiler/javanano/javanano_map_field.h',
            'src/src/google/protobuf/compiler/javanano/javanano_message.cc',
            'src/src/google/protobuf/compiler/javanano/javanano_message_field.cc',
            'src/src/google/protobuf/compiler/javanano/javanano_message_field.h',
            'src/src/google/protobuf/compiler/javanano/javanano_message.h',
            'src/src/google/protobuf/compiler/javanano/javanano_params.h',
            'src/src/google/protobuf/compiler/javanano/javanano_primitive_field.cc',
            'src/src/google/protobuf/compiler/javanano/javanano_primitive_field.h',
            'src/src/google/protobuf/compiler/js/js_generator.cc',
            'src/src/google/protobuf/compiler/js/js_generator.h',
            'src/src/google/protobuf/compiler/objectivec/objectivec_enum.cc',
            'src/src/google/protobuf/compiler/objectivec/objectivec_enum_field.cc',
            'src/src/google/protobuf/compiler/objectivec/objectivec_enum_field.h',
            'src/src/google/protobuf/compiler/objectivec/objectivec_enum.h',
            'src/src/google/protobuf/compiler/objectivec/objectivec_extension.cc',
            'src/src/google/protobuf/compiler/objectivec/objectivec_extension.h',
            'src/src/google/protobuf/compiler/objectivec/objectivec_field.cc',
            'src/src/google/protobuf/compiler/objectivec/objectivec_field.h',
            'src/src/google/protobuf/compiler/objectivec/objectivec_file.cc',
            'src/src/google/protobuf/compiler/objectivec/objectivec_file.h',
            'src/src/google/protobuf/compiler/objectivec/objectivec_generator.cc',
            'src/src/google/protobuf/compiler/objectivec/objectivec_generator.h',
            'src/src/google/protobuf/compiler/objectivec/objectivec_helpers.cc',
            'src/src/google/protobuf/compiler/objectivec/objectivec_helpers.h',
            'src/src/google/protobuf/compiler/objectivec/objectivec_map_field.cc',
            'src/src/google/protobuf/compiler/objectivec/objectivec_map_field.h',
            'src/src/google/protobuf/compiler/objectivec/objectivec_message.cc',
            'src/src/google/protobuf/compiler/objectivec/objectivec_message_field.cc',
            'src/src/google/protobuf/compiler/objectivec/objectivec_message_field.h',
            'src/src/google/protobuf/compiler/objectivec/objectivec_message.h',
            'src/src/google/protobuf/compiler/objectivec/objectivec_oneof.cc',
            'src/src/google/protobuf/compiler/objectivec/objectivec_oneof.h',
            'src/src/google/protobuf/compiler/objectivec/objectivec_primitive_field.cc',
            'src/src/google/protobuf/compiler/objectivec/objectivec_primitive_field.h',
            'src/src/google/protobuf/compiler/python/python_generator.cc',
            'src/src/google/protobuf/compiler/ruby/ruby_generator.cc',
            'src/src/google/protobuf/compiler/ruby/ruby_generator.h',
          ],
          'dependencies': [
            'protobuf_full_do_not_use',
          ],
          'include_dirs': [
            '<(config_h_dir)',
            'src/src',
          ],
        },
        {
          'target_name': 'protoc',
          'type': 'executable',
          'toolsets': ['host'],
          'sources': [
            'src/src/google/protobuf/compiler/main.cc',
          ],
          'dependencies': [
            'protoc_lib',
            'protobuf_full_do_not_use',
          ],
          'include_dirs': [
            '<(config_h_dir)',
            'src/src',
          ],
        },
        {
          # Generate the python module needed by all protoc-generated Python code.
          'target_name': 'py_proto',
          'type': 'none',
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/pyproto/google/',
              'files': [
                # google/ module gets an empty __init__.py.
                '__init__.py',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/pyproto/google/protobuf',
              'files': [
                'python/google/protobuf/__init__.py',
                'python/google/protobuf/descriptor.py',
                'python/google/protobuf/descriptor_database.py',
                'python/google/protobuf/descriptor_pool.py',
                'python/google/protobuf/message.py',
                'python/google/protobuf/message_factory.py',
                'python/google/protobuf/reflection.py',
                'python/google/protobuf/service.py',
                'python/google/protobuf/service_reflection.py',
                'python/google/protobuf/text_format.py',

                # TODO(ncarter): protoc's python generator treats
                # descriptor.proto specially, but it's not possible to trigger
                # the special treatment unless you run protoc from ./src/src
                # (the treatment is based on the path to the .proto file
                # matching a constant exactly). I'm not sure how to convince
                # gyp to execute a rule from a different directory.  Until this
                # is resolved, use a copy of descriptor_pb2.py that I manually
                # generated.
                'descriptor_pb2.py',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/pyproto/google/protobuf/internal',
              'files': [
                'python/google/protobuf/internal/__init__.py',
                'python/google/protobuf/internal/api_implementation.py',
                'python/google/protobuf/internal/containers.py',
                'python/google/protobuf/internal/cpp_message.py',
                'python/google/protobuf/internal/decoder.py',
                'python/google/protobuf/internal/encoder.py',
                'python/google/protobuf/internal/enum_type_wrapper.py',
                'python/google/protobuf/internal/generator_test.py',
                'python/google/protobuf/internal/message_listener.py',
                'python/google/protobuf/internal/python_message.py',
                'python/google/protobuf/internal/type_checkers.py',
                'python/google/protobuf/internal/wire_format.py',
              ],
            },
          ],
      #   # We can't generate a proper descriptor_pb2.py -- see earlier comment.
      #   'rules': [
      #     {
      #       'rule_name': 'genproto',
      #       'extension': 'proto',
      #       'inputs': [
      #         '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
      #       ],
      #       'variables': {
      #         # The protoc compiler requires a proto_path argument with the
      #           # directory containing the .proto file.
      #           'rule_input_relpath': 'src/src/google/protobuf',
      #         },
      #         'outputs': [
      #           '<(PRODUCT_DIR)/pyproto/google/protobuf/<(RULE_INPUT_ROOT)_pb2.py',
      #         ],
      #         'action': [
      #           '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
      #           '-I./src',
      #           '-I.',
      #           '--python_out=<(PRODUCT_DIR)/pyproto/google/protobuf',
      #           'google/protobuf/descriptor.proto',
      #         ],
      #         'message': 'Generating Python code from <(RULE_INPUT_PATH)',
      #       },
      #     ],
      #     'dependencies': [
      #       'protoc#host',
      #     ],
      #     'sources': [
      #       'src/src/google/protobuf/descriptor.proto',
      #     ],
         },
      ],
    }, { # use_system_protobuf==1
      'targets': [
        {
          'target_name': 'protobuf_lite',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              # Use full protobuf, because vanilla protobuf doesn't have
              # our custom patch to retain unknown fields in lite mode.
              '<!@(pkg-config --cflags protobuf)',
            ],
            'defines': [
              'USE_SYSTEM_PROTOBUF',

              # This macro must be defined to suppress the use
              # of dynamic_cast<>, which requires RTTI.
              'GOOGLE_PROTOBUF_NO_RTTI',
              'GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER',
            ],
          },
          'link_settings': {
            # Use full protobuf, because vanilla protobuf doesn't have
            # our custom patch to retain unknown fields in lite mode.
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other protobuf)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l protobuf)',
            ],
          },
        },
        {
          'target_name': 'protoc',
          'type': 'none',
          'toolsets': ['host', 'target'],
        },
        {
          'target_name': 'py_proto',
          'type': 'none',
        },
      ],
    }],
  ],
}
