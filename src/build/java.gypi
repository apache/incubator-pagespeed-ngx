# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to build Java in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my-package_java',
#   'type': 'none',
#   'variables': {
#     'java_in_dir': 'path/to/package/root',
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#
# Required variables:
#  java_in_dir - The top-level java directory. The src should be in
#    <java_in_dir>/src.
# Optional/automatic variables:
#  additional_input_paths - These paths will be included in the 'inputs' list to
#    ensure that this target is rebuilt when one of these paths changes.
#  additional_src_dirs - Additional directories with .java files to be compiled
#    and included in the output of this target.
#  generated_src_dirs - Same as additional_src_dirs except used for .java files
#    that are generated at build time. This should be set automatically by a
#    target's dependencies. The .java files in these directories are not
#    included in the 'inputs' list (unlike additional_src_dirs).
#  input_jars_paths - The path to jars to be included in the classpath. This
#    should be filled automatically by depending on the appropriate targets.
#  javac_includes - A list of specific files to include. This is by default
#    empty, which leads to inclusion of all files specified. May include
#    wildcard, and supports '**/' for recursive path wildcards, ie.:
#    '**/MyFileRegardlessOfDirectory.java', '**/IncludedPrefix*.java'.
#  has_java_resources - Set to 1 if the java target contains an
#    Android-compatible resources folder named res.  If 1, R_package and
#    R_package_relpath must also be set.
#  R_package - The java package in which the R class (which maps resources to
#    integer IDs) should be generated, e.g. org.chromium.content.
#  R_package_relpath - Same as R_package, but replace each '.' with '/'.
#  java_strings_grd - The name of the grd file from which to generate localized
#    strings.xml files, if any.
#  res_extra_dirs - A list of extra directories containing Android resources.
#    These directories may be generated at build time.
#  res_extra_files - A list of the files in res_extra_dirs.
#  never_lint - Set to 1 to not run lint on this target.

{
  'dependencies': [
    '<(DEPTH)/build/android/setup.gyp:build_output_dirs'
  ],
  'variables': {
    'android_jar': '<(android_sdk)/android.jar',
    'input_jars_paths': [ '<(android_jar)' ],
    'additional_src_dirs': [],
    'javac_includes': [],
    'jar_name': '<(_target_name).jar',
    'jar_dir': '<(PRODUCT_DIR)/lib.java',
    'jar_path': '<(intermediate_dir)/<(jar_name)',
    'jar_final_path': '<(jar_dir)/<(jar_name)',
    'jar_excluded_classes': [ '*/R.class', '*/R##*.class' ],
    'instr_stamp': '<(intermediate_dir)/instr.stamp',
    'additional_input_paths': [],
    'dex_path': '<(PRODUCT_DIR)/lib.java/<(_target_name).dex.jar',
    'generated_src_dirs': ['>@(generated_R_dirs)'],
    'generated_R_dirs': [],
    'has_java_resources%': 0,
    'java_strings_grd%': '',
    'res_extra_dirs': [],
    'res_extra_files': [],
    'res_v14_verify_only%': 0,
    'resource_input_paths': ['>@(res_extra_files)'],
    'intermediate_dir': '<(SHARED_INTERMEDIATE_DIR)/<(_target_name)',
    'classes_dir': '<(intermediate_dir)/classes',
    'compile_stamp': '<(intermediate_dir)/compile.stamp',
    'lint_stamp': '<(intermediate_dir)/lint.stamp',
    'lint_result': '<(intermediate_dir)/lint_result.xml',
    'lint_config': '<(intermediate_dir)/lint_config.xml',
    'never_lint%': 0,
    'proguard_config%': '',
    'proguard_preprocess%': '0',
    'variables': {
      'variables': {
        'proguard_preprocess%': 0,
        'emma_never_instrument%': 0,
      },
      'conditions': [
        ['proguard_preprocess == 1', {
          'javac_jar_path': '<(intermediate_dir)/<(_target_name).pre.jar'
        }, {
          'javac_jar_path': '<(jar_path)'
        }],
        ['chromium_code != 0 and emma_coverage != 0 and emma_never_instrument == 0', {
          'emma_instrument': 1,
        }, {
          'emma_instrument': 0,
        }],
      ],
    },
    'emma_instrument': '<(emma_instrument)',
    'javac_jar_path': '<(javac_jar_path)',
  },
  # This all_dependent_settings is used for java targets only. This will add the
  # jar path to the classpath of dependent java targets.
  'all_dependent_settings': {
    'variables': {
      'input_jars_paths': ['<(jar_final_path)'],
      'library_dexed_jars_paths': ['<(dex_path)'],
    },
  },
  'conditions': [
    ['has_java_resources == 1', {
      'variables': {
        'res_dir': '<(java_in_dir)/res',
        'res_crunched_dir': '<(intermediate_dir)/res_crunched',
        'res_v14_compatibility_stamp': '<(intermediate_dir)/res_v14_compatibility.stamp',
        'res_v14_compatibility_dir': '<(intermediate_dir)/res_v14_compatibility',
        'res_input_dirs': ['<(res_dir)', '<@(res_extra_dirs)'],
        'resource_input_paths': ['<!@(find <(res_dir) -type f)'],
        'R_dir': '<(intermediate_dir)/java_R',
        'R_text_file': '<(R_dir)/R.txt',
        'R_stamp': '<(intermediate_dir)/resources.stamp',
        'generated_src_dirs': ['<(R_dir)'],
        'additional_input_paths': ['<(R_stamp)',
                                   '<(res_v14_compatibility_stamp)',],
        'additional_res_dirs': [],
        'dependencies_res_input_dirs': [],
        'dependencies_res_files': [],
      },
      'all_dependent_settings': {
        'variables': {
          # Dependent jars include this target's R.java file via
          # generated_R_dirs and include its resources via
          # dependencies_res_files.
          'generated_R_dirs': ['<(R_dir)'],
          'additional_input_paths': ['<(R_stamp)',
                                     '<(res_v14_compatibility_stamp)',],
          'dependencies_res_files': ['<@(resource_input_paths)'],

          'dependencies_res_input_dirs': ['<@(res_input_dirs)'],

          # Dependent APKs include this target's resources via
          # additional_res_dirs, additional_res_packages, and
          # additional_R_text_files.
          'additional_res_dirs': ['<(res_crunched_dir)',
                                  '<(res_v14_compatibility_dir)',
                                  '<@(res_input_dirs)'],
          'additional_res_packages': ['<(R_package)'],
          'additional_R_text_files': ['<(R_text_file)'],
        },
      },
      'conditions': [
        ['java_strings_grd != ""', {
          'variables': {
            'res_grit_dir': '<(intermediate_dir)/res_grit',
            'res_input_dirs': ['<(res_grit_dir)'],
            'grit_grd_file': '<(java_in_dir)/strings/<(java_strings_grd)',
            'resource_input_paths': ['<!@pymod_do_main(grit_info <@(grit_defines) --outputs "<(res_grit_dir)" <(grit_grd_file))'],
          },
          'actions': [
            {
              'action_name': 'generate_localized_strings_xml',
              'variables': {
                'grit_additional_defines': ['-E', 'ANDROID_JAVA_TAGGED_ONLY=false'],
                'grit_out_dir': '<(res_grit_dir)',
                # resource_ids is unneeded since we don't generate .h headers.
                'grit_resource_ids': '',
              },
              'includes': ['../build/grit_action.gypi'],
            },
          ],
        }],
      ],
      'actions': [
        # Generate R.java and crunch image resources.
        {
          'action_name': 'process_resources',
          'message': 'processing resources for <(_target_name)',
          'variables': {
            'android_manifest': '<(DEPTH)/build/android/AndroidManifest.xml',
            # Include the dependencies' res dirs so that references to
            # resources in dependencies can be resolved.
            'all_res_dirs': ['<@(res_input_dirs)',
                             '>@(dependencies_res_input_dirs)',],
            # Write the inputs list to a file, so that the action command
            # line won't exceed the OS limits when calculating the checksum
            # of the list.
            'inputs_list_file': '>|(inputs_list.<(_target_name).gypcmd >@(_inputs))'
          },
          'inputs': [
            '<(DEPTH)/build/android/gyp/util/build_utils.py',
            '<(DEPTH)/build/android/gyp/process_resources.py',
            '>@(resource_input_paths)',
            '>@(dependencies_res_files)',
          ],
          'outputs': [
            '<(R_stamp)',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/gyp/process_resources.py',
            '--android-sdk', '<(android_sdk)',
            '--android-sdk-tools', '<(android_sdk_tools)',
            '--R-dir', '<(R_dir)',
            '--res-dirs', '>(all_res_dirs)',
            '--crunch-input-dir', '>(res_dir)',
            '--crunch-output-dir', '<(res_crunched_dir)',
            '--android-manifest', '<(android_manifest)',
            '--non-constant-id',
            '--custom-package', '<(R_package)',
            '--stamp', '<(R_stamp)',

            # Add hash of inputs to the command line, so if inputs change
            # (e.g. if a resource if removed), the command will be re-run.
            # TODO(newt): remove this once crbug.com/177552 is fixed in ninja.
            '--ignore=>!(md5sum >(inputs_list_file))',
          ],
        },
        # Generate API 14 resources.
        {
          'action_name': 'generate_api_14_resources_<(_target_name)',
          'message': 'Generating Android API 14 resources <(_target_name)',
          'variables' : {
            'res_v14_additional_options': [],
          },
          'conditions': [
            ['res_v14_verify_only == 1', {
              'variables': {
                'res_v14_additional_options': ['--verify-only']
              },
            }],
          ],
          'inputs': [
            '<(DEPTH)/build/android/gyp/util/build_utils.py',
            '<(DEPTH)/build/android/gyp/generate_v14_compatible_resources.py',
            '>@(resource_input_paths)',
          ],
          'outputs': [
            '<(res_v14_compatibility_stamp)',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/gyp/generate_v14_compatible_resources.py',
            '--res-dir=<(res_dir)',
            '--res-v14-compatibility-dir=<(res_v14_compatibility_dir)',
            '--stamp', '<(res_v14_compatibility_stamp)',
            '<@(res_v14_additional_options)',
          ]
        },
      ],
    }],
    ['proguard_preprocess == 1', {
      'actions': [
        {
          'action_name': 'proguard_<(_target_name)',
          'message': 'Proguard preprocessing <(_target_name) jar',
          'inputs': [
            '<(android_sdk_root)/tools/proguard/bin/proguard.sh',
            '<(DEPTH)/build/android/gyp/util/build_utils.py',
            '<(DEPTH)/build/android/gyp/proguard.py',
            '<(javac_jar_path)',
            '<(proguard_config)',
          ],
          'outputs': [
            '<(jar_path)',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/gyp/proguard.py',
            '--proguard-path=<(android_sdk_root)/tools/proguard/bin/proguard.sh',
            '--input-path=<(javac_jar_path)',
            '--output-path=<(jar_path)',
            '--proguard-config=<(proguard_config)',
            '--classpath=<(android_sdk_jar) >(input_jars_paths)',
          ]
        },
      ],
    }],
  ],
  'actions': [
    {
      'action_name': 'javac_<(_target_name)',
      'message': 'Compiling <(_target_name) java sources',
      'variables': {
        'all_src_dirs': [
          '>(java_in_dir)/src',
          '>@(additional_src_dirs)',
          '>@(generated_src_dirs)',
        ],
      },
      'inputs': [
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/javac.py',
        '>!@(find >(java_in_dir)/src >(additional_src_dirs) -name "*.java")',
        '>@(input_jars_paths)',
        '>@(additional_input_paths)',
      ],
      'outputs': [
        '<(compile_stamp)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/javac.py',
        '--output-dir=<(classes_dir)',
        '--classpath=>(input_jars_paths)',
        '--src-dirs=>(all_src_dirs)',
        '--javac-includes=<(javac_includes)',
        '--chromium-code=<(chromium_code)',
        '--stamp=<(compile_stamp)',

        # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
        '--ignore=>!(echo \'>(_inputs)\' | md5sum)',
      ]
    },
    {
      'variables': {
        'src_dirs': [
          '<(java_in_dir)/src',
          '>@(additional_src_dirs)',
        ],
        'stamp_path': '<(lint_stamp)',
        'result_path': '<(lint_result)',
        'config_path': '<(lint_config)',
      },
      'inputs': [
        '<(compile_stamp)',
      ],
      'outputs': [
        '<(lint_stamp)',
      ],
      'includes': [ 'android/lint_action.gypi' ],
    },
    {
      'action_name': 'jar_<(_target_name)',
      'message': 'Creating <(_target_name) jar',
      'inputs': [
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/util/md5_check.py',
        '<(DEPTH)/build/android/gyp/jar.py',
        '<(compile_stamp)',
      ],
      'outputs': [
        '<(javac_jar_path)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/jar.py',
        '--classes-dir=<(classes_dir)',
        '--jar-path=<(javac_jar_path)',
        '--excluded-classes=<(jar_excluded_classes)',
      ]
    },
    {
      'action_name': 'instr_jar_<(_target_name)',
      'message': 'Instrumenting <(_target_name) jar',
      'variables': {
        'input_path': '<(jar_path)',
        'output_path': '<(jar_final_path)',
        'stamp_path': '<(instr_stamp)',
        'instr_type': 'jar',
      },
      'outputs': [
        '<(jar_final_path)',
      ],
      'inputs': [
        '<(jar_path)',
      ],
      'includes': [ 'android/instr_action.gypi' ],
    },
    {
      'action_name': 'jar_toc_<(_target_name)',
      'message': 'Creating <(_target_name) jar.TOC',
      'inputs': [
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/util/md5_check.py',
        '<(DEPTH)/build/android/gyp/jar_toc.py',
        '<(jar_final_path)',
      ],
      'outputs': [
        '<(jar_final_path).TOC',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/jar_toc.py',
        '--jar-path=<(jar_final_path)',
        '--toc-path=<(jar_final_path).TOC',
      ]
    },
    {
      'action_name': 'dex_<(_target_name)',
      'variables': {
        'conditions': [
          ['emma_instrument != 0', {
            'dex_no_locals': 1,
          }],
        ],
        'dex_input_paths': [ '<(jar_final_path)' ],
        'output_path': '<(dex_path)',
      },
      'includes': [ 'android/dex_action.gypi' ],
    },
  ],
}
