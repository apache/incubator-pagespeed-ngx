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
  'targets': [
    {
      # Unfortunately, the inherited protobuf target in protobuf.gyp
      # does not build gzip_stream.cc, which we require. Thus we're
      # required to define our own target that includes protobuf as
      # well as gzip_stream.cc.
      'target_name': 'instaweb_protobuf_gzip',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protobuf',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
      ],
      'sources': [
        '<(DEPTH)/third_party/protobuf2/src/src/google/protobuf/io/gzip_stream.cc',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protobuf',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
      ],
    },
  ],
}
