CXX = g++
COMPILE_OPTS = -g -Wall -Werror -Wno-sign-compare -Wno-return-type \
               -funsigned-char -fdiagnostics-show-option
# TODO(sligocki): -Wno-error=sign-compare -Wno-error=return-type
# TODO(sligocki): Why is LOG(FATAL) not considered a return?
# TODO(sligocki): Why isn't -fdiagnostics-show-option working?
#GPERF = false  # Must explicitly set GPERF
#GPERF_OPTS = --ignore-case -L C++ -S 1 -t -C -c -E -G -o

SRC_DIR = /home/sligocki/src/css_parsing
PAGE_SPEED_DIR = $(SRC_DIR)/pagespeed/src
CHROMIUM_DIR = $(PAGE_SPEED_DIR)/third_party/chromium/src
GFLAGS_ROOT = $(SRC_DIR)/gflags-1.3/install
GTEST_ROOT = $(SRC_DIR)/gtest-1.5.0/install
GHASH_DIR = $(SRC_DIR)/sparsehash-1.8.1/install/include

INCLUDES = -I. -I$(SRC_DIR) -I$(CHROMIUM_DIR) -I$(GFLAGS_ROOT)/include -I$(GHASH_DIR)

PAGE_SPEED_LIBS = -L$(PAGE_SPEED_DIR)/out/Release/obj.target/base -lbase -lrt
GFLAGS_LIBS = $(GFLAGS_ROOT)/lib/libgflags.a
GTEST_LIBS = $(GTEST_ROOT)/lib/libgtest.a

LIBS = $(PAGE_SPEED_LIBS) $(GFLAGS_LIBS) $(GTEST_LIBS)

OBJ_DIR = genfiles


# Generated files are included, you do not need to build them.
#CSS_GPERFS = $(wildcard webutil/css/*.gperf)
#CSS_GPERF_CC = $(CSS_GPERFS:%.gperf=$(OBJ_DIR)/%.cc)  # Generated files.

CSS_HDRS = $(wildcard webutil/css/*.h)
CSS_SRCS = webutil/css/parser.cc webutil/css/identifier.cc webutil/css/property.cc webutil/css/selector.cc webutil/css/string_util.cc webutil/css/tostring.cc webutil/css/util.cc webutil/css/value.cc webutil/css/valuevalidator.cc $(CSS_GPERF_CC)
CSS_OBJS = $(CSS_SRCS:%.cc=$(OBJ_DIR)/%.o)

CSS_TEST_SRCS = $(wildcard webutil/css/*test.cc)
CSS_TEST_OBJS = $(CSS_TEST_SRCS:%.cc=$(OBJ_DIR)/%.o)
CSS_TESTS =     $(CSS_TEST_SRCS:%.cc=$(OBJ_DIR)/%)

HTML_HDRS = $(wildcard webutil/html/*.h)
HTML_SRCS = $(wildcard webutil/html/*.cc)
HTML_OBJS = $(HTML_SRCS:%.cc=$(OBJ_DIR)/%.o)

UNICODETEXT_HDRS = $(wildcard util/utf8/public/*.h)
UNICODETEXT_SRCS = $(wildcard util/utf8/internal/*.cc)
UNICODETEXT_OBJS = $(UNICODETEXT_SRCS:%.cc=$(OBJ_DIR)/%.o)

OTHER_SRCS = strings/ascii_ctype.cc strings/stringpiece_utils.cc
OTHER_OBJS = $(OTHER_SRCS:%.cc=$(OBJ_DIR)/%.o) third_party/utf/libutf.a

OBJS = $(CSS_OBJS) $(HTML_OBJS) $(UNICODETEXT_OBJS) $(OTHER_OBJS)

all: parse_args run_tests

PARSE_ARGS_MAIN = $(OBJ_DIR)/webutil/css/parse_arg.o
parse_args: $(PARSE_ARGS_MAIN) $(OBJS)
	$(CXX) $^ $(LIBS) -o $@

run_tests: $(CSS_TESTS)
	STATUS=0; \
	for x in $^; do \
	  echo -e "\n** Running $$x **"; \
	  ./$$x || STATUS=$$?; \
	done; \
	exit $$STATUS  # So that build fails if any test fails

TEST_MAIN = $(OBJ_DIR)/webutil/css/gtest_main.o
%test: %test.o $(TEST_MAIN) $(OBJS)
	$(CXX) $^ $(LIBS) -o $@

$(OBJ_DIR)/%.o: %.cc # TODO(sligocki): Add dep on headers.
	mkdir -p `dirname $@`
	$(CXX) $(COMPILE_OPTS) $< -c $(INCLUDES) -o $@


#$(OBJ_DIR)/webutil/css/property.cc: $(OBJ_DIR)/%.cc: %.gperf $(CSS_HDRS)
#	mkdir -p `dirname $@`
#	$(GPERF) $(GPERF_OPTS) -ZPropertyMapper $< > $@
#
#$(OBJ_DIR)/webutil/css/identifier.cc: $(OBJ_DIR)/%.cc: %.gperf $(CSS_HDRS)
#	mkdir -p `dirname $@`
#	$(GPERF) $(GPERF_OPTS) -ZIdentifierMapper $< > $@


third_party/utf/libutf.a: $(wildcard third_party/utf/*.cc) $(wildcard third_party/utf/*.h)
	make -C third_party/utf CC=$(CXX)


clean:
	chmod +w -R .
	rm -f $(OBJ_DIR) css


# Prepare files by adding license and other details needed for open-source.
prepare_files:
# Prepare webutil/html files (first by adding #include "string_using.h")
	mkdir -p webutil/html; \
	for f in `ls original/webutil/html/`; do \
	  cp apache-license.txt webutil/html/$$f; \
	  sed 's@#include <string>@\0\n#include "string_using.h"@' \
	    < original/webutil/html/$$f \
	    >> webutil/html/$$f; \
	  rm -f original/webutil/html/$$f; \
	done
# The rest of the files just need to have licenses added.
	for f in `cd original/ && find . -type f`; do \
	  mkdir -p `dirname $$f`; \
	  cat apache-license.txt original/$$f > $$f; \
	done
	rm -rf original/