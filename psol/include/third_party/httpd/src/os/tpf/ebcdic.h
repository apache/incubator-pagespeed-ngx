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
 * @file  tpf/ebcdic.h
 * @brief EBCDIC/ASCII converson function declarations
 *
 * @addtogroup APACHE_OS_TPF
 * @{
 */
 
#include <sys/types.h>

extern const unsigned char os_toascii[256];
extern const unsigned char os_toebcdic[256];
void ebcdic2ascii(void *dest, const void *srce, size_t count);
void ebcdic2ascii_strictly(unsigned char *dest, const unsigned char *srce, size_t count);
void ascii2ebcdic(void *dest, const void *srce, size_t count);
/** @} */
