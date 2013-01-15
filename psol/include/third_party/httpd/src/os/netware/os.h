/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file netware/os.h
 * @brief This file in included in all Apache source code. It contains definitions
 * of facilities available on _this_ operating system (HAVE_* macros),
 * and prototypes of OS specific functions defined in os.c or os-inline.c
 *
 * @defgroup APACHE_OS_NETWARE netware
 * @ingroup  APACHE_OS
 * @{
 */

#ifndef APACHE_OS_H
#define APACHE_OS_H

#ifndef PLATFORM
#define PLATFORM "NETWARE"
#endif

#include <screen.h>

AP_DECLARE_DATA extern int hold_screen_on_exit; /* Indicates whether the screen should be held open on exit*/

#define CASE_BLIND_FILESYSTEM
#define NO_WRITEV

#define APACHE_MPM_DIR  "server/mpm/netware" /* generated on unix */

#define getpid NXThreadGetId

/* Hold the screen open if there is an exit code and the hold_screen_on_exit flag >= 0 or the
   hold_screen_on_exit > 0.  If the hold_screen_on_exit flag is < 0 then close the screen no 
   matter what the exit code is. */
#define exit(s) {if((s||hold_screen_on_exit)&&(hold_screen_on_exit>=0)){pressanykey();}apr_terminate();exit(s);}

#endif   /* ! APACHE_OS_H */
/** @} */
