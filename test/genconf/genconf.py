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
import compiler
from compiler.ast import Const
from compiler.ast import Dict
from compiler.ast import List
from compiler.ast import Node
from compiler.ast import UnarySub
import copy
import os
import re
from templite import Templite
from util import Error

def parse_python_struct(file_contents):
    ast = compiler.parse(file_contents)
    c1 = ast.getChildren()
    c2 = c1[1].getChildren()
    c3 = c2[0].getChildren()
    return c3[0]

def get_key_path(o):
    # true/false from  end up here:
    if len(o.getChildren()) == 1:
        return o.getChildren()[0]

    # handle reference to other element
    c1 = o.getChildren()[0]
    c2 = o.getChildren()[1]

    if not isinstance(c2, Node):
        if isinstance(c1.getChildren()[0], str):
            return '.%s.%s' % (c1.getChildren()[0], c2)
        else:
            return '%s.%s' % (get_key_path(c1), c2)
    else:
        return get_key_path(c2)

def ast_node_to_dict(node, dest, lookup, parent_key, key_to_node):
    key_to_node[parent_key] = node

    if Node is None:
        return None
    elif isinstance(node, Dict):
        dest = OrderedDict()
        c = node.getChildren()
        for n in range(0, len(c), 2):
            key = c[n].getChildren()[0]
            key_to_node["%s.%s" % (parent_key, key)] = c[n]
            dest[key] = ast_node_to_dict(c[n + 1], dest, lookup,
                    '%s.%s' % (parent_key, key), key_to_node)
            lookup[parent_key] = dest
    elif isinstance(node, List):
        dest = []
        for (index, child) in enumerate(node.getChildren()):
            key_to_node["%s.%s" % (parent_key, str(index))] = child
            cn = ast_node_to_dict(child, None, lookup, parent_key +
                                  '.' + repr(index), key_to_node)
            dest.append(cn)
            lookup[parent_key] = dest
    elif isinstance(node, UnarySub):
        if parent_key in lookup:
            raise Error("line %d: %s: already defined" %
                        (node.lineno,parent_key))
        lookup[parent_key] = '-' + repr(node.getChildren()[0].getChildren()[0])
        return '-' + repr(node.getChildren()[0].getChildren()[0])
    elif isinstance(node, Const):
        if parent_key in lookup:
            raise Error("line %d: %s: already defined" % (node.lineno,
                                                          parent_key))
        lookup[parent_key] = node.getChildren()[0]
        return node.getChildren()[0]
    elif isinstance(node, Node):
        key_path = get_key_path(node)
        if key_path in lookup:
            val = lookup[key_path]
            lookup[parent_key] = val
        else:
            raise Error("line %d: Lookup failed for [%s]" % (node.lineno,
                                                             repr(node)))
        return val
    return dest

# Given a line like:
#   #cond1,cond2,!cond3,cond4 foo bar baz
# then if cond1 and cond2 and not cond3 and cond4 return
#   foo bar baz
# otherwise return an empty line
def replace_comments(conditions, s):
    matched_conditions = s.group(1)
    config = s.group(2)

    for condition in matched_conditions.split(","):
        if not matches_condition(condition, conditions):
            return s.group(0)
    return config

# condition: something like 'cond1' or '!cond2'
# conditions: a list of currently enabled conditions
# return whether this condition is in conditions (or, if it's a negated
# condition, whether it's not in conditions)
def matches_condition(condition, conditions):
    if condition.startswith("!"):
        return condition[1:] not in conditions
    else:
        return condition in conditions

def fill_placeholders(placeholders, match):
    placeholder = match.group(1)
    if placeholder not in placeholders:
        raise Error("placeholder [%s] not found" % placeholder)
    else:
        return str(placeholders[placeholder])

def pre_process_text(cfg, conditions, placeholders):
    re_conditional_lines = r'^[ \t]*#([^ \s\t]*)([^\r\n]*\r?\n?)$'
    cfg = re.sub(re_conditional_lines, lambda x: \
                 replace_comments(conditions, x), cfg,
                 flags=re.MULTILINE)
    re_placeholders = r'@@([^\s]*)@@'
    cfg = re.sub(re_placeholders, lambda x: \
                 fill_placeholders(placeholders, x), cfg)
    return cfg

def pre_process_ifdefs(cfg,conditions):
    lines = cfg.split("\n")
    ifstack = [True]
    ret = []

    for line in lines:
        if line.startswith("#ifdef"):
            condition = line[len("#ifdef"):].strip()
            ifstack.append(condition in conditions)
            ret.append("## filtered out")
        elif line.startswith("#ifndef"):
            condition = line[len("#ifndef"):].strip()
            ifstack.append(not condition in conditions)
            ret.append("## filtered out")
        elif line.startswith("#endif"):
            if len(ifstack) == 1:
                raise Error("unmatched #endif found in input")
            ifstack.pop()
            ret.append("## filtered out")
        else:
            if not False in ifstack:
                ret.append(line)
            else:
                ret.append("## filtered out")

    if not len(ifstack) == 1:
        raise Error("#ifdef not matched with an #endif")

    return "\n".join(ret)

def copy_prefix(key_to_node, prefix, new_prefix):
    copy_list = {}
    for key in key_to_node:
        if key[:len(prefix)] == prefix:
            copy_list[new_prefix + key[len(prefix):]] = key_to_node[key]

    for key in copy_list:
        key_to_node[key] = copy_list[key]

def copy_locations_to_virtual_hosts(config, key_to_node):
    # we clone locations that are defined at the root level
    # to all defined servers. that way, we do not have
    # to rely on inheritance being available in the targeted
    # server configuration mechanisms
    # we delete these locations from the root configuration
    # after we have performed the clones

    move = ["locations"]
    servers = config["servers"]
    for m in move:
        for server_index, server in enumerate(servers):
            if not m in server:
               server[m] = []

            offset = len(server[m])
            for (location_index, location) in enumerate(config[m]):
                new_prefix = ".servers.%s.%s.%s" % (server_index, m,
                                                    location_index + offset)
                server[m].append(copy.deepcopy(location))
                copy_prefix(key_to_node,
                            ".locations.%s" % location_index, new_prefix)

        del config[m]

def move_configuration_to_locations(config):
    # this inspects all locations, and clones any
    # directives that should be inherited down to them
    # so we do not have to rely on external inheritance
    # mechanisms
    if not "servers" in config: return

    servers = config["servers"]

    for server in servers:
        if "locations" in server:
            locations = server["locations"]
            for location in locations:
                merge_location_config(config,server,location)
            if "headers" in server:
                del server["headers"]

    if "headers" in config:
        del config["headers"]

def merge_location_config(root,server,location):
    # pagespeed directives take care of inheriting properly
    # themselves, but in nginx for example the headers are not
    # inherited like you would expect at first sight. we
    # merge these headers ourselves to the location ourselves here
    result = []

    if "headers" in root:
        result.extend(root["headers"])
    if "headers" in server:
        result.extend(server["headers"])
    if "headers" in location:
        result.extend(location["headers"])

    location["headers"] = copy.deepcopy(result)

def execute_template(pyconf_path, conditions,
                     placeholders, template_path):

    if not os.path.exists(pyconf_path):
        raise Error("Input configuration not found at [%s]" % pyconf_path)
    if not os.path.exists(template_path):
        raise Error("Expected a transformation script at [%s]" % template_path)

    config_text = ""

    with open(pyconf_path) as config_file:
        config_text = config_file.read()
    original_len = len(config_text.split("\n"))
    config_text = pre_process_ifdefs(config_text, conditions)
    new_len = len(config_text.split("\n"))

    # Make sure the number of lines doesn't get changed by the preprocessor.
    # We want to be able to report correct line numbers for templates that
    # choke on a .pyconf file
    if (new_len != original_len):
        raise Error("preprocessor step 1: linecount changed from %s to %s:\n"
                    % (original_len, new_len))

    config_text = pre_process_text(config_text, conditions,
                                   placeholders)
    new_len = len(config_text.split("\n"))

    if (new_len != original_len): 
        raise Error("preprocessor step 2: linecount changed from %s to %s:\n"
                    % (original_len, new_len))

    ast = parse_python_struct(config_text)
    key_to_node = {}
    config = ast_node_to_dict(ast, None, {}, '', key_to_node)
    template_file = ""

    with open(template_path) as template_file:
        template_text = template_file.read()

    template_text = pre_process_text(template_text, conditions,
                                     placeholders)
    template = Templite(template_text)

    copy_locations_to_virtual_hosts(config, key_to_node)
    move_configuration_to_locations(config)
    text = template.render(config=config, key_to_node=key_to_node)
    return text
