#
# Copyright 2011 Google Inc.
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
# Author: jmarantz@google.com (Joshua Marantz)

# This Makefile provides a mechanism to generate a Page Speed Automatic
# library (.a) from an already-built mod_pagespeed distribution.  This

# MOD_PAGESPEED_ROOT should be set to the 'src' directory where mod_pagespeed
# was built.  See http://code.google.com/p/modpagespeed/wiki/HowToBuild
#
# When running this Makefile from the 'automatic' directory then it will
# be set automatically.
MOD_PAGESPEED_ROOT = $(shell cd ../../..; pwd)

# OUTPUT_ROOT should be set to wherever you want to put output files.  Default
# is to put them in the current directory.
OUTPUT_DIR = $(PWD)

# You can use a Debug or a Release build.  Default is Release.  Feel free
# to set this to Debug to get visibility into Page Speed Automatic code.
#
# Note that this is the same Makefile variable used by the generated Makefiles
# on Linux.
BUILDTYPE = Release

# TODO(jmarantz): this library-list should be automatically generated
# from the 'gyp' flow, rather than hand-entered here.
BASE_LIBS = \
  base/libbase.a \
  base/libbase_static.a \
  build/temp_gyp/libgoogleurl.a \
  net/instaweb/libautomatic_util.a \
  net/instaweb/libhttp_core.a \
  net/instaweb/libinstaweb_delay_images_data2c.a \
  net/instaweb/libinstaweb_delay_images_opt_data2c.a \
  net/instaweb/libinstaweb_delay_images_inline_data2c.a \
  net/instaweb/libinstaweb_delay_images_inline_opt_data2c.a \
  net/instaweb/libinstaweb_htmlparse_core.a \
  net/instaweb/libinstaweb_htmlparse_core_gperf.a \
  net/instaweb/libinstaweb_http.a \
  net/instaweb/libinstaweb_http_gperf.a \
  net/instaweb/libinstaweb_http_pb.a \
  net/instaweb/libinstaweb_javascript_gperf.a \
  net/instaweb/libinstaweb_js_defer_data2c.a \
  net/instaweb/libinstaweb_js_defer_opt_data2c.a \
  net/instaweb/libinstaweb_lazyload_images_data2c.a \
  net/instaweb/libinstaweb_lazyload_images_opt_data2c.a \
  net/instaweb/libinstaweb_panel_config_pb.a \
  net/instaweb/libinstaweb_propcache_pb.a \
  net/instaweb/libinstaweb_rewriter.a \
  net/instaweb/libinstaweb_rewriter_base.a \
  net/instaweb/libinstaweb_rewriter_css.a \
  net/instaweb/libinstaweb_rewriter_html.a \
  net/instaweb/libinstaweb_rewriter_html_gperf.a \
  net/instaweb/libinstaweb_rewriter_html_option_gperf.a \
  net/instaweb/libinstaweb_rewriter_image.a \
  net/instaweb/libinstaweb_rewriter_javascript.a \
  net/instaweb/libinstaweb_rewriter_pb.a \
  net/instaweb/libinstaweb_spriter.a \
  net/instaweb/libinstaweb_spriter_pb.a \
  net/instaweb/libinstaweb_timing_pb.a \
  net/instaweb/libinstaweb_util.a \
  net/instaweb/libinstaweb_util_core.a \
  net/instaweb/libinstaweb_util_pthread.a \
  net/instaweb/libprocess_context.a \
  third_party/base64/libbase64.a \
  third_party/chromium/src/base/third_party/dynamic_annotations/libdynamic_annotations.a \
  third_party/chromium/src/net/tools/liburl_to_filename_encoder.a \
  third_party/css_parser/libcss_parser.a \
  third_party/css_parser/libutf.a \
  third_party/domain_registry_provider/src/domain_registry/libdomain_registry_lib.a \
  third_party/gflags/libgflags.a \
  third_party/icu/libicudata.a \
  third_party/icu/libicuuc.a \
  third_party/libjpeg/libjpeg.a \
  third_party/libpagespeed/src/pagespeed/core/libpagespeed_core.a \
  third_party/libpagespeed/src/pagespeed/image_compression/libpagespeed_image_converter.a \
  third_party/libpagespeed/src/pagespeed/image_compression/libpagespeed_jpeg_optimizer.a \
  third_party/libpagespeed/src/pagespeed/image_compression/libpagespeed_jpeg_reader.a \
  third_party/libpagespeed/src/pagespeed/image_compression/libpagespeed_png_optimizer.a \
  third_party/libpagespeed/src/pagespeed/js/libpagespeed_jsminify.a \
  third_party/libpagespeed/src/pagespeed/proto/libpagespeed_output_pb.a \
  third_party/libpagespeed/src/pagespeed/proto/libpagespeed_resource_pb.a \
  third_party/libpagespeed/src/pagespeed/proto/libtimeline_pb.a \
  third_party/libpagespeed/src/third_party/giflib/libdgiflib.a \
  third_party/libpagespeed/src/third_party/giflib/libgiflib_core.a \
  third_party/libpagespeed/src/third_party/optipng/libopngreduc.a \
  third_party/libpng/libpng.a \
  third_party/libwebp/libwebp_dec.a \
  third_party/libwebp/libwebp_enc.a \
  third_party/opencv/libhighgui.a \
  third_party/opencv/libopencv_imgproc.a \
  third_party/opencv/libopencv_core.a \
  third_party/protobuf/libprotobuf_lite.a \
  third_party/zlib/libzlib.a

# The 'gclient' build flow uses 'xcodebuild' on Mac and 'make' on Linux.
# To make matters worse, it puts all the intermediate libraries in the same
# directory on Mac but distributes them over a directory tree on Linux.

ifeq ($(HOSTTYPE),intel-mac)

# on MacOS the xcodebuild system puts all the libraries in the same directory
# so we strip it with 'notdir'.
INCLUDE_ARCH = arch/mac/ia32
LIBS = $(addprefix $(MOD_PAGESPEED_ROOT)/xcodebuild/$(BUILDTYPE)/,$(notdir $(BASE_LIBS)))
BINDIR = /Developer/usr/bin
CXX = $(BINDIR)/gcc-4.2 -arch i386 -fvisibility=hidden
SYSLIBS = -Wl,-search_paths_first -framework AppKit -lpthread -lstdc++

build_libraries:
	cd $(MOD_PAGESPEED_ROOT)/build && xcodebuild -project all.xcodeproj \
		-configuration $(BUILDTYPE) -target pagespeed_automatic

# TODO(jmarantz): here's a potential egrep expression to make the build less
# noisy.
#		| egrep -v '(BUILD NATIVE|Check dependencies|$^|PhaseScriptExecution|BUILD AGGREGATE|.build/Script-|Nothing to be done for|cd /)'

else

# TODO(jmarantz): test this on 32-bit systems.  Add fail handler for non-linux
# non-mac.
ifeq ($(HOSTTYPE),x86_64-linux)
 INCLUDE_ARCH = arch/linux/x64
else
 INCLUDE_ARCH = arch/linux/ia32
endif
INCLUDE_ARCH = arch/linux/x64
LIBS = $(addprefix $(MOD_PAGESPEED_ROOT)/out/$(BUILDTYPE)/obj.target/, $(BASE_LIBS))
SYSLIBS = -lpthread -lrt
CXX = /usr/bin/g++

build_libraries:
	cd $(MOD_PAGESPEED_ROOT) && make BUILDTYPE=$(BUILDTYPE)

endif

INCLUDE_PATH = \
    -I$(MOD_PAGESPEED_ROOT) \
    -I$(MOD_PAGESPEED_ROOT)/third_party/chromium/src \
    -I$(MOD_PAGESPEED_ROOT)/third_party/gflags/gen/$(INCLUDE_ARCH)/include
OBJS          = $(OUTPUT_DIR)/static_rewriter.o \
		$(OUTPUT_DIR)/pagespeed_automatic.a
EXE           = $(OUTPUT_DIR)/static_rewriter
EXAMPLE_ROOT  = $(MOD_PAGESPEED_ROOT)/install/mod_pagespeed_example
EXAMPLES      = $(OUTPUT_DIR)/collapse_whitespace.html

exe : $(EXE)

# Specifying 'build_libraries' as a dependency but recursively making
# 'static_rewriter' allows 'make -j' to work.  This is needed because
# we don't articulate all the dependencies required to build the libraries.
all : build_libraries
	$(MAKE) examples

# Flags used to compile static_rewriter.cc.  Note that these are not
# the flags used to compile pagespeed_automatic, which is compiled
# with either BUILDTYPE=Release or BUILDTYPE=Debug
CXXFLAGS = -g

$(OUTPUT_DIR)/pagespeed_automatic.a : $(LIBS)
	@echo Merging libraries ...
	@./merge_libraries.sh $@ $(LIBS)


$(OUTPUT_DIR)/static_rewriter.o : static_rewriter.cc
	$(CXX) $(INCLUDE_PATH) $(CXXFLAGS) $(MOD_PAGESPEED_INCLUDE) -c $< -o $@

$(EXE) : $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(SYSLIBS)

$(OUTPUT_DIR)/collapse_whitespace.html : $(EXE) $(EXAMPLE_ROOT)/collapse_whitespace.html
	$(EXE) --rewriters=collapse_whitespace $(EXAMPLE_ROOT) $(OUTPUT_DIR) \
                collapse_whitespace.html

examples: $(EXAMPLES)

clean:
	rm -f $(OUTPUT_DIR)/static_rewriter $(EXE) $(OBJS) $(EXAMPLES)

echo_libs :
	echo LIBS = $(LIBS)
