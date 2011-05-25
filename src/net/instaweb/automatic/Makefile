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

MOD_PAGESPEED_INCLUDE = \
    -I$(MOD_PAGESPEED_ROOT) \
    -I$(MOD_PAGESPEED_ROOT)/third_party/chromium/src

# TODO(jmarantz): this library-list should be automatically generated
# from the 'gyp' flow, rather than hand-entered here.

# The 'gclient' build flow uses 'xcodebuild' on Mac and 'make' on Linux.
# To make matters worse, it puts all the intermediate libraries in the same
# directory on Mac but distributes them over a directory tree on Linux.

ifeq ($(HOSTTYPE),intel-mac)

# TODO(jmarantz): specify only the Linux libraries (which have subdirs)
# and use gmake file-expressions to generate the Mac one when running on
# mac, rather than replicating the library-list.
LIB_ROOT = $(MOD_PAGESPEED_ROOT)/xcodebuild/$(BUILDTYPE)
LIBS = $(LIB_ROOT)/libbase.a \
       $(LIB_ROOT)/libbase64.a \
       $(LIB_ROOT)/libchrome_zlib.a \
       $(LIB_ROOT)/libcss_parser.a \
       $(LIB_ROOT)/libcv.a \
       $(LIB_ROOT)/libcxcore.a \
       $(LIB_ROOT)/libdgiflib.a \
       $(LIB_ROOT)/libflann.a \
       $(LIB_ROOT)/libgflags.a \
       $(LIB_ROOT)/libgiflib_core.a \
       $(LIB_ROOT)/libgoogleurl.a \
       $(LIB_ROOT)/libhighgui.a \
       $(LIB_ROOT)/libicudata.a \
       $(LIB_ROOT)/libicuuc.a \
       $(LIB_ROOT)/libinstaweb_htmlparse.a \
       $(LIB_ROOT)/libinstaweb_htmlparse_core.a \
       $(LIB_ROOT)/libinstaweb_http.a \
       $(LIB_ROOT)/libinstaweb_http_pb.a \
       $(LIB_ROOT)/libinstaweb_rewriter.a \
       $(LIB_ROOT)/libinstaweb_rewriter_base.a \
       $(LIB_ROOT)/libinstaweb_rewriter_css.a \
       $(LIB_ROOT)/libinstaweb_rewriter_html.a \
       $(LIB_ROOT)/libinstaweb_rewriter_image.a \
       $(LIB_ROOT)/libinstaweb_rewriter_javascript.a \
       $(LIB_ROOT)/libinstaweb_rewriter_pb.a \
       $(LIB_ROOT)/libinstaweb_spriter.a \
       $(LIB_ROOT)/libinstaweb_spriter_pb.a \
       $(LIB_ROOT)/libinstaweb_util.a \
       $(LIB_ROOT)/libinstaweb_util_core.a \
       $(LIB_ROOT)/libinstaweb_util_pthread.a \
       $(LIB_ROOT)/libjpeg.a \
       $(LIB_ROOT)/libjpeg_trans.a \
       $(LIB_ROOT)/liblapack.a \
       $(LIB_ROOT)/libopngreduc.a \
       $(LIB_ROOT)/libpagespeed_core.a \
       $(LIB_ROOT)/libpagespeed_jpeg_optimizer.a \
       $(LIB_ROOT)/libpagespeed_jpeg_reader.a \
       $(LIB_ROOT)/libpagespeed_jsminify.a \
       $(LIB_ROOT)/libpagespeed_output_pb.a \
       $(LIB_ROOT)/libpagespeed_png_optimizer.a \
       $(LIB_ROOT)/libpng.a \
       $(LIB_ROOT)/libprotobuf_lite.a \
       $(LIB_ROOT)/liburl_to_filename_encoder.a

else

LIB_ROOT          = $(MOD_PAGESPEED_ROOT)/out/$(BUILDTYPE)/obj.target
INSTAWEB_LIB      = $(LIB_ROOT)/net/instaweb
THIRD_PARTY_LIB   = $(LIB_ROOT)/third_party
LIBS = \
  $(LIB_ROOT)/base/libbase.a \
  $(INSTAWEB_LIB)/libinstaweb_rewriter_javascript.a \
  $(INSTAWEB_LIB)/libinstaweb_rewriter_pb.a \
  $(INSTAWEB_LIB)/libinstaweb_util.a \
  $(INSTAWEB_LIB)/libinstaweb_spriter.a \
  $(INSTAWEB_LIB)/libinstaweb_util_core.a \
  $(INSTAWEB_LIB)/libinstaweb_spriter_pb.a \
  $(INSTAWEB_LIB)/libinstaweb_rewriter_base.a \
  $(INSTAWEB_LIB)/libinstaweb_http.a \
  $(INSTAWEB_LIB)/libinstaweb_htmlparse.a \
  $(INSTAWEB_LIB)/libinstaweb_rewriter_image.a \
  $(INSTAWEB_LIB)/libinstaweb_util_pthread.a \
  $(INSTAWEB_LIB)/libinstaweb_rewriter_html.a \
  $(INSTAWEB_LIB)/libinstaweb_rewriter_css.a \
  $(INSTAWEB_LIB)/libinstaweb_http_pb.a \
  $(INSTAWEB_LIB)/libinstaweb_htmlparse_core.a \
  $(INSTAWEB_LIB)/libinstaweb_rewriter.a \
  $(THIRD_PARTY_LIB)/libpng/libpng.a \
  $(THIRD_PARTY_LIB)/base64/libbase64.a \
  $(THIRD_PARTY_LIB)/gflags/libgflags.a \
  $(THIRD_PARTY_LIB)/icu/libicuuc.a \
  $(THIRD_PARTY_LIB)/icu/libicudata.a \
  $(THIRD_PARTY_LIB)/libpagespeed/src/pagespeed/jsminify/libpagespeed_jsminify.a \
  $(THIRD_PARTY_LIB)/libpagespeed/src/pagespeed/image_compression/libpagespeed_jpeg_reader.a \
  $(THIRD_PARTY_LIB)/libpagespeed/src/pagespeed/image_compression/libpagespeed_jpeg_optimizer.a \
  $(THIRD_PARTY_LIB)/libpagespeed/src/pagespeed/image_compression/libpagespeed_png_optimizer.a \
  $(THIRD_PARTY_LIB)/libpagespeed/src/pagespeed/core/libpagespeed_core.a \
  $(THIRD_PARTY_LIB)/libpagespeed/src/pagespeed/proto/libpagespeed_output_pb.a \
  $(THIRD_PARTY_LIB)/libpagespeed/src/third_party/giflib/libdgiflib.a \
  $(THIRD_PARTY_LIB)/libpagespeed/src/third_party/giflib/libgiflib_core.a \
  $(THIRD_PARTY_LIB)/libpagespeed/src/third_party/optipng/libopngreduc.a \
  $(THIRD_PARTY_LIB)/libpagespeed/src/third_party/libjpeg_trans/libjpeg_trans.a \
  $(THIRD_PARTY_LIB)/protobuf/libprotobuf_lite.a \
  $(THIRD_PARTY_LIB)/opencv/libcv.a \
  $(THIRD_PARTY_LIB)/opencv/liblapack.a \
  $(THIRD_PARTY_LIB)/opencv/libhighgui.a \
  $(THIRD_PARTY_LIB)/opencv/libflann.a \
  $(THIRD_PARTY_LIB)/opencv/libcxcore.a \
  $(THIRD_PARTY_LIB)/chromium/src/net/tools/liburl_to_filename_encoder.a \
  $(THIRD_PARTY_LIB)/libjpeg/libjpeg.a \
  $(THIRD_PARTY_LIB)/css_parser/libcss_parser.a \
  $(THIRD_PARTY_LIB)/zlib/libchrome_zlib.a \
  $(LIB_ROOT)/build/temp_gyp/libgoogleurl.a

endif

EXE = $(OUTPUT_DIR)/static_rewriter

exe : $(EXE)

# Specifying 'build_libraries' as a dependency but recursively making
# 'static_rewriter' allows 'make -j' to work.  This is needed because
# we don't articulate all the dependencies required to build the libraries.
all : build_libraries
	$(MAKE) exe

ifeq ($(HOSTTYPE),intel-mac)
build_libraries:
	cd $(MOD_PAGESPEED_ROOT)/build && xcodebuild -project all.xcodeproj \
		-configuration $(BUILDTYPE) -target pagespeed_automatic

# TODO(jmarantz): here's a potential egrep expression to make the build less
# noisy.
#		| egrep -v '(BUILD NATIVE|Check dependencies|$^|PhaseScriptExecution|BUILD AGGREGATE|.build/Script-|Nothing to be done for|cd /)'

BINDIR = /Developer/usr/bin
CXX = $(BINDIR)/gcc-4.2 -arch i386 -fvisibility=hidden
SYSLIBS = -Wl,-search_paths_first -framework AppKit -lpthread -lstdc++

else

CXX = /usr/bin/g++
SYSLIBS = -lpthread -lrt

build_libraries:
	cd $(MOD_PAGESPEED_ROOT) && make BUILDTYPE=$(BUILDTYPE)

endif

# Flags used to compile static_rewriter.cc.  Note that these are not
# the flags used to compile pagespeed_automatic, which is compiled
# with either BUILDTYPE=Release or BUILDTYPE=Debug
CXXFLAGS = -g

$(OUTPUT_DIR)/pagespeed_automatic.a : $(LIBS)
	@echo Merging libraries ...
	@./merge_libraries.sh $@ $(LIBS)


$(OUTPUT_DIR)/static_rewriter.o : static_rewriter.cc
	$(CXX) $(MOD_PAGESPEED_INCLUDE) $(CXXFLAGS) $(MOD_PAGESPEED_INCLUDE) -c $< -o $@

OBJS = $(OUTPUT_DIR)/static_rewriter.o $(OUTPUT_DIR)/pagespeed_automatic.a

$(EXE) : $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(SYSLIBS)

clean:
	rm -f $(OUTPUT_DIR)/static_rewriter $(EXE) $(OBJS)
