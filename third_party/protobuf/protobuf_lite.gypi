# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    'src/src/google/protobuf/stubs/atomicops.h',
    'src/src/google/protobuf/stubs/atomicops_internals_arm_gcc.h',
    'src/src/google/protobuf/stubs/atomicops_internals_atomicword_compat.h',
    'src/src/google/protobuf/stubs/atomicops_internals_macosx.h',
    'src/src/google/protobuf/stubs/atomicops_internals_mips_gcc.h',
    'src/src/google/protobuf/stubs/atomicops_internals_x86_gcc.cc',
    'src/src/google/protobuf/stubs/atomicops_internals_x86_gcc.h',
    'src/src/google/protobuf/stubs/atomicops_internals_x86_msvc.cc',
    'src/src/google/protobuf/stubs/atomicops_internals_x86_msvc.h',
    'src/src/google/protobuf/stubs/common.h',
    'src/src/google/protobuf/stubs/once.h',
    'src/src/google/protobuf/stubs/platform_macros.h',
    'src/src/google/protobuf/extension_set.h',
    'src/src/google/protobuf/generated_message_util.h',
    'src/src/google/protobuf/message_lite.h',
    'src/src/google/protobuf/repeated_field.h',
    'src/src/google/protobuf/unknown_field_set.cc',
    'src/src/google/protobuf/unknown_field_set.h',
    'src/src/google/protobuf/wire_format_lite.h',
    'src/src/google/protobuf/wire_format_lite_inl.h',
    'src/src/google/protobuf/io/coded_stream.h',
    'src/src/google/protobuf/io/zero_copy_stream.h',
    'src/src/google/protobuf/io/zero_copy_stream_impl_lite.h',

    'src/src/google/protobuf/stubs/common.cc',
    'src/src/google/protobuf/stubs/once.cc',
    'src/src/google/protobuf/stubs/hash.h',
    'src/src/google/protobuf/stubs/map-util.h',
    'src/src/google/protobuf/extension_set.cc',
    'src/src/google/protobuf/generated_message_util.cc',
    'src/src/google/protobuf/message_lite.cc',
    'src/src/google/protobuf/repeated_field.cc',
    'src/src/google/protobuf/wire_format_lite.cc',
    'src/src/google/protobuf/io/coded_stream.cc',
    'src/src/google/protobuf/io/coded_stream_inl.h',
    'src/src/google/protobuf/io/zero_copy_stream.cc',
    'src/src/google/protobuf/io/zero_copy_stream_impl_lite.cc',
    '<(config_h_dir)/config.h',
  ],
  'include_dirs': [
    '<(config_h_dir)',
    'src/src',
  ],
  # This macro must be defined to suppress the use of dynamic_cast<>,
  # which requires RTTI.
  'defines': [
    'GOOGLE_PROTOBUF_NO_RTTI',
    'GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER',
    'HAVE_PTHREAD',
  ],
  'direct_dependent_settings': {
    'include_dirs': [
      '<(config_h_dir)',
      'src/src',
    ],
    'defines': [
      'GOOGLE_PROTOBUF_NO_RTTI',
      'GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER',
    ],
    # TODO(jschuh): http://crbug.com/167187 size_t -> int.
    'msvs_disabled_warnings': [ 4267 ],
  },
}
