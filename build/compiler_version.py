#!/usr/bin/python

# Copyright 2010 Google Inc.
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

# This script is wrapper for the Chromium version of compiler_version.py.

import os

script_dir = os.path.dirname(__file__)
chrome_src = os.path.normpath(os.path.join(script_dir, os.pardir, 'third_party', 'chromium', 'src'))

execfile(os.path.join(chrome_src, 'build', 'compiler_version.py'))
