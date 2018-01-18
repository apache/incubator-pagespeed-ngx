for ps_src_file in $PS_NGX_SRCS; do
  ps_obj_file="$NGX_OBJS/addon/src/`basename $ps_src_file .cc`.o"
  echo "$ps_obj_file : CFLAGS += --std=c++11" >> $NGX_MAKEFILE
done
