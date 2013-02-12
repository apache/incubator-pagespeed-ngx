# Copyright 2013 Google Inc.
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
#
# Author: oschaaf@gmail.com (Otto van der Schaaf)


class Error(Exception):
    pass

def write_cfg(key_to_writer, config, level=0):
    global global_writer

    for key in config:
        next_level = level
        if key + '_open' in key_to_writer:
            w = key_to_writer[key + '_open']
            if w != write_void:
                next_level = next_level + 1
            handled = w(config[key], level)
            if not handled:
                if key + '_open_item' in key_to_writer \
                    and isinstance(config[key], list):
                    for (index, item) in enumerate(config[key]):
                        w = key_to_writer[key + '_open_item']
                        handled = w(item, next_level)
                        if not handled:
                            write_cfg(key_to_writer, item, level + 1)
                        if key + '_close_item' in key_to_writer:
                            w = key_to_writer[key + '_close_item']
                            w(item, next_level)
                else:
                    write_cfg(key_to_writer, config[key], level)

            if key + '_close' in key_to_writer:
                w = key_to_writer[key + '_close']
                w(config[key], level)
        else:
            if not isinstance(config[key], str) \
                and not isinstance(config[key], int):
                raise Error("no tranform handler for [%s]" % key)

def indent(txt, level):
    lines = txt.split("\n")
    r = []
    for line in lines:
        if not line: continue
        r.append("%s%s\n" % (' ' * (level * 4), line))
        
    return ''.join(r)

def emit_indent(txt,level):
    global global_writer
    global_writer(indent(txt,level))

def write_void(ps, level):
    pass

def set_writer(writer):
    global global_writer
    global_writer = writer
