# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
# 
#   http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

# Since nginx build system doesn't normally do C++, there is no CXXFLAGS for us
# to touch, and compilers are understandably unhappy with --std=c++11 on C
# files. Hence, we hack the makefile to add it for just our sources.

for ps_src_file in $PS_NGX_SRCS; do
  ps_obj_file="$NGX_OBJS/addon/src/`basename $ps_src_file .cc`.o"
  echo "$ps_obj_file : CFLAGS += --std=c++11" >> $NGX_MAKEFILE
done
