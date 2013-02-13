# Copyright 2013 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Author: morlovich@google.com (Maksim Orlovich)
#
# This simply forwards to the Chromium's lastchange.py script, but runs it
# from the mod_pagespeed repo so it gets the mod_pagespeed revision and not
# the chromium one.
import sys
sys.path.append('util')
from lastchange import main

if __name__ == '__main__':
  sys.exit(main())
