def write_cfg(key_to_writer, config, level=0):
    global global_writer

    for key in config:
   	if key + "_open" in key_to_writer:
       	   w = key_to_writer[key+"_open"]
       	   handled = w(config[key],level)
	   if not handled:
    	      if isinstance(config[key], list): 
       	      	 for index, item in enumerate(config[key]):
       	   	     w = key_to_writer[key+"_open_item"]
       	   	     handled = w(item,level+1)
	   	     if not handled:
	      	     	write_cfg(key_to_writer, item, level+2)
                     if key + "_close_item" in key_to_writer:
                         w = key_to_writer[key+"_close_item"]   
                         w(item,level+1)
	      else:
	        write_cfg(key_to_writer, config[key], level+1)

           if key + "_close" in key_to_writer:
               w = key_to_writer[key+"_close"]   
               w(config[key],level)
       	else:
            if not isinstance(config[key],str) and \
                    not isinstance(config[key],int):
                global_writer(indent("",level)+"no writer for '"+key + "'\n")

# TODO(oschaaf): remove default value from level
def indent(txt, level):
    return " " * (level*4) + txt

# TODO(oschaaf): figure out a way to no increase the indent level if write_void is used
def write_void(ps,level):
    pass

def set_writer(writer):
    global global_writer
    global_writer = writer

# TODO(oschaaf): rename to emit_val_or_default
def val_or_default(dict, key, rep):
    global global_writer
    if not key in dict:
        global_writer(rep)
    else:
        global_writer(dict[key])

def emit_pagespeed_directive(prefix, directive, val):
    global global_writer
    if isinstance(val, list):
        for index, item in enumerate(val):
            emit_pagespeed_directive(prefix, directive, item)
    else:
        #fixme
        global_writer(prefix + "pagespeed ")
    	global_writer(directive)
    	global_writer(" ")
    	global_writer(val)
    	global_writer(";\n")

