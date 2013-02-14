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

from collections import OrderedDict
import sys
import traceback
import warnings

class Error(Exception):
    pass

def pyconf_fatal(lineno, e = None):
    message = "Exception while processing line [%s] from pyconf" % lineno
    if e: message = "%s\n\n%s\n" % (message,traceback.format_exc())
    sys.exit(message)

def exec_template_call(func, lineno, item, level):
    try:
        return func(item, level)
    except Exception as e:
        pyconf_fatal(lineno, e)
        raise

def check_pagespeed_supported(directives):
    global global_pagespeed_unsupported
    pagespeed_unsupported = None

    try:
        pagespeed_unsupported = global_pagespeed_unsupported
    except NameError:
        pagespeed_unsupported = None

    if not pagespeed_unsupported:
        return directives

    supported_directives = OrderedDict()

    for directive in directives:
        if directive.lower() in map(str.lower, pagespeed_unsupported):
            warnings.warn("%s not supported, skipping" % directive)
        else:
            supported_directives[directive] = directives[directive]

    return supported_directives

def write_cfg(key_to_writer, config, key_to_node, level=0, parent_key = ""):
    global global_writer

    for key in config:
        key_path = "%s.%s" % (parent_key, key)
        next_level = level
        lineno = "unknown"
        handled = False

        if key_path in key_to_node:
            lineno = key_to_node[key_path].lineno

        if key + '_open' in key_to_writer:
            w = key_to_writer[key + '_open']
            if w != write_void:
                next_level = next_level + 1

            if key.lower() == "pagespeed":
                config[key] = check_pagespeed_supported(config[key])

            handled = exec_template_call(w, lineno, config[key], level)

            if not handled:
                if key + '_open_item' in key_to_writer \
                    and isinstance(config[key], list):
                    for (index, item) in enumerate(config[key]):
                        w = key_to_writer[key + '_open_item']
                        child_key_path = "%s.%s" % (key_path, str(index))
                        handled = exec_template_call(
                            w, lineno, item, next_level)

                        if not handled:
                            write_cfg(key_to_writer, item, key_to_node,
                                      level + 1, child_key_path)
                        if key + '_close_item' in key_to_writer:
                            w = key_to_writer[key + '_close_item']
                            exec_template_call(w, lineno, item, next_level)
                else:
                    write_cfg(key_to_writer, config[key], key_to_node, level
                              ,key_path)

            if key + '_close' in key_to_writer:
                w = key_to_writer[key + '_close']
                exec_template_call(w, lineno, config[key], level)
        else:
            if not isinstance(config[key], str) \
                and not isinstance(config[key], int):
                raise Error("no tranform handler for [%s] at line [%s]"
                            % (key_path, lineno))

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

def set_pagespeed_unsupported(pagespeed_unsupported):
    global global_pagespeed_unsupported
    global_pagespeed_unsupported = pagespeed_unsupported
