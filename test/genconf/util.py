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
                # TODO(oschaaf): 
                if 1==0:
                    global_writer(indent('', level) + "no writer for '"
                                  + key + "'\n")

def indent(txt, level):
    return ' ' * (level * 4) + txt

def write_void(ps, level):
    pass

def set_writer(writer):
    global global_writer
    global_writer = writer
