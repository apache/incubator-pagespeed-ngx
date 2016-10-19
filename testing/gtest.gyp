# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'gtest',
      'type': 'static_library',
      'sources': [
        'gtest/include/gtest/gtest-death-test.h',
        'gtest/include/gtest/gtest-message.h',
        'gtest/include/gtest/gtest-param-test.h',
        'gtest/include/gtest/gtest-printers.h',
        'gtest/include/gtest/gtest-spi.h',
        'gtest/include/gtest/gtest-test-part.h',
        'gtest/include/gtest/gtest-typed-test.h',
        'gtest/include/gtest/gtest.h',
        'gtest/include/gtest/gtest_pred_impl.h',
        'gtest/include/gtest/internal/gtest-death-test-internal.h',
        'gtest/include/gtest/internal/gtest-filepath.h',
        'gtest/include/gtest/internal/gtest-internal.h',
        'gtest/include/gtest/internal/gtest-linked_ptr.h',
        'gtest/include/gtest/internal/gtest-param-util-generated.h',
        'gtest/include/gtest/internal/gtest-param-util.h',
        'gtest/include/gtest/internal/gtest-port.h',
        'gtest/include/gtest/internal/gtest-string.h',
        'gtest/include/gtest/internal/gtest-tuple.h',
        'gtest/include/gtest/internal/gtest-type-util.h',
        'gtest/src/gtest-all.cc',
        'gtest/src/gtest-death-test.cc',
        'gtest/src/gtest-filepath.cc',
        'gtest/src/gtest-internal-inl.h',
        'gtest/src/gtest-port.cc',
        'gtest/src/gtest-printers.cc',
        'gtest/src/gtest-test-part.cc',
        'gtest/src/gtest-typed-test.cc',
        'gtest/src/gtest.cc',
      ],
      'sources!': [
        'gtest/src/gtest-all.cc',  # Not needed by our build.
      ],
      'include_dirs': [
        'gtest',
        'gtest/include',
      ],
      'dependencies': [
        'gtest_prod',
      ],
      'defines': [
        # In order to allow regex matches in gtest to be shared between Windows
        # and other systems, we tell gtest to always use it's internal engine.
        'GTEST_HAS_POSIX_RE=0',
      ],
      'all_dependent_settings': {
        'defines': [
          'GTEST_HAS_POSIX_RE=0',
        ],
      },
      'conditions': [
        ['OS == "mac" or OS == "ios"', {
          'sources': [
            'gtest_mac.h',
            'gtest_mac.mm',
            'platform_test_mac.mm'
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          },
        }],
        ['OS == "ios"', {
          'dependencies' : [
            '<(DEPTH)/testing/iossim/iossim.gyp:iossim#host',
          ],
          'direct_dependent_settings': {
            'target_conditions': [
              # Turn all tests into bundles on iOS because that's the only
              # type of executable supported for iOS.
              ['_type=="executable"', {
                'variables': {
                  # Use a variable so the path gets fixed up so it is always
                  # correct when INFOPLIST_FILE finally gets set.
                  'ios_unittest_info_plist_path':
                    '<(DEPTH)/testing/gtest_ios/unittest-Info.plist',
                },
                'mac_bundle': 1,
                'xcode_settings': {
                  'BUNDLE_ID_TEST_NAME':
                    '>!(echo ">(_target_name)" | sed -e "s/_//g")',
                  'INFOPLIST_FILE': '>(ios_unittest_info_plist_path)',
                },
                'mac_bundle_resources': [
                  '<(ios_unittest_info_plist_path)',
                  '<(DEPTH)/testing/gtest_ios/Default-568h@2x.png',
                ],
                'mac_bundle_resources!': [
                  '<(ios_unittest_info_plist_path)',
                ],
              }],
            ],
          },
        }],
        ['OS=="ios" and asan==1', {
          'direct_dependent_settings': {
            'target_conditions': [
              # Package the ASan runtime dylib into the test app bundles.
              ['_type=="executable"', {
                'postbuilds': [
                  {
                    'variables': {
                      # Define copy_asan_dylib_path in a variable ending in
                      # _path so that gyp understands it's a path and
                      # performs proper relativization during dict merging.
                      'copy_asan_dylib_path':
                        '<(DEPTH)/build/mac/copy_asan_runtime_dylib.sh',
                    },
                    'postbuild_name': 'Copy ASan runtime dylib',
                    'action': [
                      '>(copy_asan_dylib_path)',
                    ],
                  },
                ],
              }],
            ],
          },
        }],
        ['os_posix == 1', {
          'defines': [
            # gtest isn't able to figure out when RTTI is disabled for gcc
            # versions older than 4.3.2, and assumes it's enabled.  Our Mac
            # and Linux builds disable RTTI, and cannot guarantee that the
            # compiler will be 4.3.2. or newer.  The Mac, for example, uses
            # 4.2.1 as that is the latest available on that platform.  gtest
            # must be instructed that RTTI is disabled here, and for any
            # direct dependents that might include gtest headers.
            'GTEST_HAS_RTTI=0',
          ],
          'direct_dependent_settings': {
            'defines': [
              'GTEST_HAS_RTTI=0',
            ],
          },
        }],
        ['OS=="android" and android_app_abi=="x86"', {
          'defines': [
            'GTEST_HAS_CLONE=0',
          ],
          'direct_dependent_settings': {
            'defines': [
              'GTEST_HAS_CLONE=0',
            ],
          },
        }],
        ['OS=="android"', {
          # We want gtest features that use tr1::tuple, but we currently
          # don't support the variadic templates used by libstdc++'s
          # implementation. gtest supports this scenario by providing its
          # own implementation but we must opt in to it.
          'defines': [
            'GTEST_USE_OWN_TR1_TUPLE=1',
            # GTEST_USE_OWN_TR1_TUPLE only works if GTEST_HAS_TR1_TUPLE is set.
            # gtest r625 made it so that GTEST_HAS_TR1_TUPLE is set to 0
            # automatically on android, so it has to be set explicitly here.
            'GTEST_HAS_TR1_TUPLE=1',
          ],
          'direct_dependent_settings': {
            'defines': [
              'GTEST_USE_OWN_TR1_TUPLE=1',
              'GTEST_HAS_TR1_TUPLE=1',
            ],
          },
        }],
        ['OS=="win" and (MSVS_VERSION=="2012" or MSVS_VERSION=="2012e")', {
          'defines': [
            '_VARIADIC_MAX=10',
          ],
          'direct_dependent_settings': {
            'defines': [
              '_VARIADIC_MAX=10',
            ],
          },
        }],
      ],
      'direct_dependent_settings': {
        'defines': [
          'UNIT_TEST',
        ],
        'include_dirs': [
          'gtest/include',  # So that gtest headers can find themselves.
        ],
        'target_conditions': [
          ['_type=="executable"', {
            'test': 1,
            'conditions': [
              ['OS=="mac"', {
                'run_as': {
                  'action????': ['${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}'],
                },
              }],
              ['OS=="ios"', {
                'variables': {
                  # Use a variable so the path gets fixed up so it is always
                  # correct when the action finally gets used.
                  'ios_run_unittest_script_path':
                    '<(DEPTH)/testing/gtest_ios/run-unittest.sh',
                },
                'run_as': {
                  'action????': ['>(ios_run_unittest_script_path)'],
                },
              }],
              ['OS=="win"', {
                'run_as': {
                  'action????': ['$(TargetPath)', '--gtest_print_time'],
                },
              }],
            ],
          }],
        ],
        'msvs_disabled_warnings': [4800],
      },
    },
    {
      'target_name': 'gtest_main',
      'type': 'static_library',
      'dependencies': [
        'gtest',
      ],
      'sources': [
        'gtest/src/gtest_main.cc',
      ],
    },
    {
      'target_name': 'gtest_prod',
      'toolsets': ['host', 'target'],
      'type': 'none',
      'sources': [
        'gtest/include/gtest/gtest_prod.h',
      ],
    },
  ],
}
