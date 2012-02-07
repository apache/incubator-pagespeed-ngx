# Copyright 2009 Google Inc.
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

{
  'variables': {
    'opencv_root': '<(DEPTH)/third_party/opencv',
    'opencv_src': '<(opencv_root)/src',
    'opencv_gen': '<(DEPTH)/third_party/opencv/gen/arch/<(OS)/<(target_arch)',
    'use_system_opencv%': 0,
  },
  'conditions': [
    ['use_system_opencv==0', {
      'targets': [
       {
          'target_name': 'opencv_core',
          'type': '<(library)',
          'dependencies': [
            '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
          ],
          'sources': [
            'src/opencv/modules/core/src/alloc.cpp',
            'src/opencv/modules/core/src/arithm.cpp',
            'src/opencv/modules/core/src/array.cpp',
            'src/opencv/modules/core/src/cmdparser.cpp',
            'src/opencv/modules/core/src/convert.cpp',
            'src/opencv/modules/core/src/copy.cpp',
            'src/opencv/modules/core/src/datastructs.cpp',
            'src/opencv/modules/core/src/drawing.cpp',
            'src/opencv/modules/core/src/dxt.cpp',
            'src/opencv/modules/core/src/lapack.cpp',
            'src/opencv/modules/core/src/mathfuncs.cpp',
            'src/opencv/modules/core/src/matmul.cpp',
            'src/opencv/modules/core/src/matop.cpp',
            'src/opencv/modules/core/src/matrix.cpp',
            'src/opencv/modules/core/src/out.cpp',
            # Since persistence.cpp would pull in the gzip I/O layer for zlib,
            # we simply stub out the one symbol that's referenced to it
            # and not actually called.
            '<(DEPTH)/third_party/opencv/persistence_stub.cpp',
            'src/opencv/modules/core/src/precomp.cpp',
            'src/opencv/modules/core/src/rand.cpp',
            'src/opencv/modules/core/src/stat.cpp',
            'src/opencv/modules/core/src/system.cpp',
            'src/opencv/modules/core/src/tables.cpp',
          ],
         'include_dirs': [
            '<(opencv_src)/opencv/include',
            '<(opencv_src)/opencv/modules/core/include',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(opencv_src)/opencv/include',
              '<(opencv_src)/opencv/modules/core/include',
            ],
          },
        },
        {
          'target_name': 'opencv_imgproc',
          'type': '<(library)',
          'dependencies': [
            'opencv_core',
          ],
          'sources': [
            'src/opencv/modules/imgproc/src/accum.cpp',
            'src/opencv/modules/imgproc/src/approx.cpp',
            'src/opencv/modules/imgproc/src/canny.cpp',
            'src/opencv/modules/imgproc/src/color.cpp',
            'src/opencv/modules/imgproc/src/contours.cpp',
            'src/opencv/modules/imgproc/src/convhull.cpp',
            'src/opencv/modules/imgproc/src/corner.cpp',
            'src/opencv/modules/imgproc/src/cornersubpix.cpp',
            'src/opencv/modules/imgproc/src/deriv.cpp',
            'src/opencv/modules/imgproc/src/distransform.cpp',
            'src/opencv/modules/imgproc/src/emd.cpp',
            'src/opencv/modules/imgproc/src/featureselect.cpp',
            'src/opencv/modules/imgproc/src/featuretree.cpp',
            'src/opencv/modules/imgproc/src/filter.cpp',
            'src/opencv/modules/imgproc/src/floodfill.cpp',
            'src/opencv/modules/imgproc/src/geometry.cpp',
            'src/opencv/modules/imgproc/src/grabcut.cpp',
            'src/opencv/modules/imgproc/src/histogram.cpp',
            'src/opencv/modules/imgproc/src/hough.cpp',
            'src/opencv/modules/imgproc/src/imgwarp.cpp',
            'src/opencv/modules/imgproc/src/inpaint.cpp',
            'src/opencv/modules/imgproc/src/kdtree.cpp',
            'src/opencv/modules/imgproc/src/linefit.cpp',
            'src/opencv/modules/imgproc/src/lsh.cpp',
            'src/opencv/modules/imgproc/src/matchcontours.cpp',
            'src/opencv/modules/imgproc/src/moments.cpp',
            'src/opencv/modules/imgproc/src/morph.cpp',
            'src/opencv/modules/imgproc/src/precomp.cpp',
            'src/opencv/modules/imgproc/src/pyramids.cpp',
            'src/opencv/modules/imgproc/src/pyrsegmentation.cpp',
            'src/opencv/modules/imgproc/src/rotcalipers.cpp',
            'src/opencv/modules/imgproc/src/samplers.cpp',
            'src/opencv/modules/imgproc/src/segmentation.cpp',
            'src/opencv/modules/imgproc/src/shapedescr.cpp',
            'src/opencv/modules/imgproc/src/smooth.cpp',
            'src/opencv/modules/imgproc/src/spilltree.cpp',
            'src/opencv/modules/imgproc/src/subdivision2d.cpp',
            'src/opencv/modules/imgproc/src/sumpixels.cpp',
            'src/opencv/modules/imgproc/src/tables.cpp',
            'src/opencv/modules/imgproc/src/templmatch.cpp',
            'src/opencv/modules/imgproc/src/thresh.cpp',
            'src/opencv/modules/imgproc/src/undistort.cpp',
            'src/opencv/modules/imgproc/src/utils.cpp',
          ],
          'include_dirs': [
            '<(opencv_src)/opencv/modules/imgproc/include',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(opencv_src)/opencv/modules/imgproc/include',
            ],
          },
          'export_dependent_settings': [
            'opencv_core',
          ],
        },
        {
          'target_name': 'highgui',
          'type': '<(library)',
          'dependencies': [
            'opencv_imgproc',
            '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
            '<(DEPTH)/third_party/libjpeg/libjpeg.gyp:libjpeg',
            '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
          ],
          'sources': [
            'src/opencv/modules/highgui/src/bitstrm.cpp',
            'src/opencv/modules/highgui/src/cap.cpp',
            'src/opencv/modules/highgui/src/grfmt_base.cpp',
            'src/opencv/modules/highgui/src/grfmt_bmp.cpp',
            'src/opencv/modules/highgui/src/grfmt_jpeg.cpp',
            'src/opencv/modules/highgui/src/grfmt_png.cpp',
            'src/opencv/modules/highgui/src/grfmt_pxm.cpp',
            'src/opencv/modules/highgui/src/grfmt_sunras.cpp',
            'src/opencv/modules/highgui/src/grfmt_tiff.cpp',
            'src/opencv/modules/highgui/src/loadsave.cpp',
            'src/opencv/modules/highgui/src/precomp.cpp',
            'src/opencv/modules/highgui/src/utils.cpp',
          ],
          'include_dirs': [
            '<(opencv_gen)/include',
            '<(opencv_src)/opencv/modules/highgui/include',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(opencv_src)/opencv/modules/highgui/include',
            ],
          },
          'export_dependent_settings': [
            'opencv_imgproc',
          ],
        },
      ],
    },
    { # use_system_opencv
      'targets': [
        {
          'target_name': 'highgui',
          'type': 'settings',
          'cflags': [
            '<!@(pkg-config --cflags opencv)',
          ],
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_OPENCV',
            ],
            'cflags+': [
              '<!@(pkg-config --cflags-only-I opencv)',
            ],
          },
          'defines': [
            'USE_SYSTEM_OPENCV',
          ],
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other opencv)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l opencv)',
            ],
          },
        },
      ],
    }],
  ],
}

