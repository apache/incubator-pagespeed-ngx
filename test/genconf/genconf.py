#!/usr/bin/python
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


class Error(Exception):
    pass

def parse_python_struct(file_contents):
    ast = compiler.parse(file_contents)
    c1 = ast.getChildren()
    c2 = c1[1].getChildren()
    c3 = c2[0].getChildren()
    return c3[0]

def flatten_attribute(o):
    # true/false from  end up here:
    if len(o.getChildren()) == 1:
        return o.getChildren()[0]

    # handle reference to other element
    c1 = o.getChildren()[0]
    c2 = o.getChildren()[1]

    if not isinstance(c2, Node):
        if isinstance(c1.getChildren()[0], str):
            return '.' + c1.getChildren()[0] + '.' + c2
        else:
            return flatten_attribute(c1) + '.' + c2
    else:
        return flatten_attribute(c2)

def ast_node_to_dict(
    node,
    dest=None,
    lookup={},
    parent_key='',
    ):
    if Node is None:
        return None
    elif isinstance(node, Dict):
        dest = OrderedDict()
        c = node.getChildren()
        for n in range(0, len(c), 2):
            key = c[n].getChildren()[0]
            dest[key] = ast_node_to_dict(c[n + 1], dest, lookup,
                    parent_key + '.' + key)
            lookup[parent_key] = dest
    elif isinstance(node, List):
        dest = []
        for (index, child) in enumerate(node.getChildren()):
            cn = ast_node_to_dict(child, None, lookup, parent_key +
                                  '.' + repr(index))
            dest.append(cn)
            lookup[parent_key] = dest
    elif isinstance(node, UnarySub):
        if parent_key in lookup:
            raise Error(parent_key + ": already defined")
        lookup[parent_key] = '-' + repr(node.getChildren()[0].getChildren()[0])
        return '-' + repr(node.getChildren()[0].getChildren()[0])
    elif isinstance(node, Const):
        if parent_key in lookup:
            raise Error(parent_key + ": already defined")
        lookup[parent_key] = node.getChildren()[0]
        return node.getChildren()[0]
    elif isinstance(node, Node):
        flattened = flatten_attribute(node)
        val = '#LOOKUP_FAILED [' + repr(node) + ']!'
        if flattened in lookup:
            val = lookup[flattened]
            lookup[parent_key] = val
        else:
            raise Error(val)
        return val
    return dest

def replace_comments(conditions, s):
    condition = s.group(1)
    config = s.group(2)

    # TODO(oschaaf): handle comments
    if condition in conditions:
        return config
    else:
        return s.group(0)

def fill_placeholders(placeholders, match):
    placeholder = match.group(1)
    if placeholder not in placeholders:
        raise Exception("placeholder '" + placeholder + "' not found")
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
    re_empty_line = r'^\s*$'
    cfg = re.sub(re_empty_line, '', cfg, flags=re.MULTILINE)
    return cfg

def pre_process_ifdefs(cfg,conditions):
    lines = cfg.split("\n")
    ifstack = [True]
    ret = []

    for line in lines:
        if line.startswith("#ifdef"):
            condition = line[len("#ifdef"):].strip()
            ifstack.append(condition in conditions)
        if line.startswith("#ifndef "):
            condition = line[len("#ifndef"):].strip()
            ifstack.append(not condition in conditions)
        elif line.startswith("#endif"):
            ifstack.pop()
        else:
            # TODO(oschaaf): bound check
            if not False in ifstack:
                ret.append(line)

    # TODO(oschaaf): ensure ifstack length equals 1 here
    return "\n".join(ret)

def copy_locations_to_virtual_hosts(config):
    # we clone locations that are defined at the root level
    # to all defined servers. that way, we do not have
    # to rely on inheritance being available in the targeted
    # server configuration mechanisms
    # we delete these locations from the root configuration
    # after we have performed the clones

    move = ["locations"]
    servers = config["servers"]
    for m in move:
        for server in servers:
            if not m in server:
               server[m] = []
            server[m].extend(copy.deepcopy(config[m]))
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

    config_file = open(pyconf_path)
    config_text = config_file.read()
    config_text = pre_process_ifdefs(config_text, conditions)
    config_text = pre_process_text(config_text, conditions,
                                   placeholders)

    ast = parse_python_struct(config_text)
    config = ast_node_to_dict(ast)
    template_file = open(template_path)
    template_text = template_file.read()
    template_text = pre_process_text(template_text, conditions,
                                     placeholders)
    template = Templite(template_text)

    copy_locations_to_virtual_hosts(config)
    move_configuration_to_locations(config)
    text = template.render(config=config)
    return text
