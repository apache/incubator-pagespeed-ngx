# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libwebp_dec',
      'type': 'static_library',
      'dependencies' : [
        'libwebp_dsp',
        'libwebp_dsp_neon',
        'libwebp_utils',
      ],
      'include_dirs': ['.'],
      'sources': [
        '<(DEPTH)/third_party/libwebp/src/dec/alpha.c',
        '<(DEPTH)/third_party/libwebp/src/dec/buffer.c',
        '<(DEPTH)/third_party/libwebp/src/dec/frame.c',
        '<(DEPTH)/third_party/libwebp/src/dec/idec.c',
        '<(DEPTH)/third_party/libwebp/src/dec/io.c',
        '<(DEPTH)/third_party/libwebp/src/dec/quant.c',
        '<(DEPTH)/third_party/libwebp/src/dec/tree.c',
        '<(DEPTH)/third_party/libwebp/src/dec/vp8.c',
        '<(DEPTH)/third_party/libwebp/src/dec/vp8l.c',
        '<(DEPTH)/third_party/libwebp/src/dec/webp.c',
      ],
    },
    {
      'target_name': 'libwebp_demux',
      'type': 'static_library',
      'include_dirs': ['.'],
      'sources': [
        'demux/demux.c',
      ],
    },
    {
      'target_name': 'libwebp_dsp',
      'type': 'static_library',
      'include_dirs': ['.'],
      'sources': [
        '<(DEPTH)/third_party/libwebp/src/dsp/alpha_processing.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/cpu.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/dec.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/dec_clip_tables.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/dec_mips32.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/dec_sse2.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/enc.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/enc_avx2.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/enc_mips32.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/enc_sse2.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/lossless.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/lossless_mips32.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/lossless_sse2.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/upsampling.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/upsampling_sse2.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/yuv.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/yuv_mips32.c',
        '<(DEPTH)/third_party/libwebp/src/dsp/yuv_sse2.c',
      ],
#      'conditions': [
#        ['OS == "android"', {
#          'includes': [ 'android/cpufeatures.gypi' ],
#        }],
#        ['order_profiling != 0', {
#          'target_conditions' : [
#            ['_toolset=="target"', {
#              'cflags!': [ '-finstrument-functions' ],
#            }],
#          ],
#        }],
#      ],
    },
    {
      'target_name': 'libwebp_dsp_neon',
      'conditions': [
        ['target_arch == "arm" and arm_version >= 7 and (arm_neon == 1 or arm_neon_optional == 1)', {
          'type': 'static_library',
          'include_dirs': ['.'],
          'sources': [
            '<(DEPTH)/third_party/libwebp/src/dsp/dec_neon.c',
            '<(DEPTH)/third_party/libwebp/src/dsp/enc_neon.c',
            '<(DEPTH)/third_party/libwebp/src/dsp/lossless_neon.c',
            '<(DEPTH)/third_party/libwebp/src/dsp/upsampling_neon.c',
          ],
          # behavior similar to *.c.neon in an Android.mk
          'cflags!': [ '-mfpu=vfpv3-d16' ],
          'cflags': [ '-mfpu=neon' ],
        },{  # "target_arch != "arm" or arm_version < 7"
          'type': 'none',
        }],
        ['order_profiling != 0', {
          'target_conditions' : [
            ['_toolset=="target"', {
              'cflags!': [ '-finstrument-functions' ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'libwebp_enc',
      'type': 'static_library',
      'include_dirs': ['.'],
      'sources': [
        '<(DEPTH)/third_party/libwebp/src/enc/alpha.c',
        '<(DEPTH)/third_party/libwebp/src/enc/analysis.c',
        '<(DEPTH)/third_party/libwebp/src/enc/backward_references.c',
        '<(DEPTH)/third_party/libwebp/src/enc/config.c',
        '<(DEPTH)/third_party/libwebp/src/enc/cost.c',
        '<(DEPTH)/third_party/libwebp/src/enc/filter.c',
        '<(DEPTH)/third_party/libwebp/src/enc/frame.c',
        '<(DEPTH)/third_party/libwebp/src/enc/histogram.c',
        '<(DEPTH)/third_party/libwebp/src/enc/iterator.c',
        '<(DEPTH)/third_party/libwebp/src/enc/picture.c',
        '<(DEPTH)/third_party/libwebp/src/enc/picture_csp.c',
        '<(DEPTH)/third_party/libwebp/src/enc/picture_psnr.c',
        '<(DEPTH)/third_party/libwebp/src/enc/picture_rescale.c',
        '<(DEPTH)/third_party/libwebp/src/enc/picture_tools.c',
        '<(DEPTH)/third_party/libwebp/src/enc/quant.c',
        '<(DEPTH)/third_party/libwebp/src/enc/syntax.c',
        '<(DEPTH)/third_party/libwebp/src/enc/token.c',
        '<(DEPTH)/third_party/libwebp/src/enc/tree.c',
        '<(DEPTH)/third_party/libwebp/src/enc/vp8l.c',
        '<(DEPTH)/third_party/libwebp/src/enc/webpenc.c',
      ],
    },
    {
      'target_name': 'libwebp_utils',
      'type': 'static_library',
      'include_dirs': ['.'],
      'sources': [
        '<(DEPTH)/third_party/libwebp/src/utils/bit_reader.c',
        '<(DEPTH)/third_party/libwebp/src/utils/bit_writer.c',
        '<(DEPTH)/third_party/libwebp/src/utils/color_cache.c',
        '<(DEPTH)/third_party/libwebp/src/utils/filters.c',
        '<(DEPTH)/third_party/libwebp/src/utils/huffman.c',
        '<(DEPTH)/third_party/libwebp/src/utils/huffman_encode.c',
        '<(DEPTH)/third_party/libwebp/src/utils/quant_levels.c',
        '<(DEPTH)/third_party/libwebp/src/utils/quant_levels_dec.c',
        '<(DEPTH)/third_party/libwebp/src/utils/random.c',
        '<(DEPTH)/third_party/libwebp/src/utils/rescaler.c',
        '<(DEPTH)/third_party/libwebp/src/utils/thread.c',
        '<(DEPTH)/third_party/libwebp/src/utils/utils.c',
      ],
    },
    {
      'target_name': 'libwebp_mux',
      'type': 'static_library',
      'include_dirs': ['.'],
      'sources': [
        '<(DEPTH)/third_party/libwebp/src/mux/muxedit.c',
        '<(DEPTH)/third_party/libwebp/src/mux/muxinternal.c',
        '<(DEPTH)/third_party/libwebp/src/mux/muxread.c',
      ],
    },
    {
      'target_name': 'libwebp_enc_mux',
      'type': 'static_library',
      'dependencies': [
        'libwebp_mux',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/libwebp/src',
      ],
    'sources': [
        '<(DEPTH)/third_party/libwebp/examples/gif2webp_util.c',
        ],
    },
    {
      'target_name': 'libwebp',
      'type': 'none',
      'dependencies' : [
        'libwebp_dec',
        'libwebp_demux',
        'libwebp_dsp',
        'libwebp_dsp_neon',
        'libwebp_enc',
        'libwebp_enc_mux',
        'libwebp_utils',
      ],
      'direct_dependent_settings': {
        'include_dirs': ['.'],
      },
      'conditions': [
        ['OS!="win"', {'product_name': 'webp'}],
      ],
    },
  ],
}
