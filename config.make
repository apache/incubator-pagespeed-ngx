# Since nginx build system doesn't normally do C++, there is no CXXFLAGS for us
# to touch, and compilers are understandably unhappy with --std=c++11 on C
# files. Hence, we hack the makefile to add it for just our sources.
for ps_src_file in $PS_NGX_SRCS; do
  ps_obj_file="$NGX_OBJS/addon/src/`basename $ps_src_file .cc`.o"
  echo "$ps_obj_file : CFLAGS += --std=c++11" >> $NGX_MAKEFILE
done
