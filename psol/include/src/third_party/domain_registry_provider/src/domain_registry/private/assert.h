/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DOMAIN_REGISTRY_PRIVATE_ASSERT_H_
#define DOMAIN_REGISTRY_PRIVATE_ASSERT_H_

void DoAssert(const char* file, int line, const char* cond_str, int cond);

#ifdef NDEBUG
#define DCHECK(x)
#else
#define DCHECK(x) DoAssert(__FILE__, __LINE__, #x, (x))
#endif

#endif  /* DOMAIN_REGISTRY_PRIVATE_ASSERT_H_ */
