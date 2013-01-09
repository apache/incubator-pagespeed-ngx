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
 *
 * Functions to search the registry tables. These should not
 * need to be invoked directly.
 */

#ifndef DOMAIN_REGISTRY_PRIVATE_TRIE_SEARCH_H_
#define DOMAIN_REGISTRY_PRIVATE_TRIE_SEARCH_H_

#include <stdlib.h>

#include "domain_registry/private/registry_types.h"
#include "domain_registry/private/trie_node.h"

/*
 * Find a TrieNode under the given parent node with the specified
 * name. If parent is NULL then the search is performed at the root
 * TrieNode.
 */
const struct TrieNode* FindRegistryNode(const char* component,
                                        const struct TrieNode* parent);

/*
 * Find a leaf TrieNode under the given parent node with the specified
 * name. If parent does not have all leaf children (i.e. if
 * HasLeafChildren(parent) returns zero), will assert and return
 * NULL. If parent is NULL then the search is performed at the root
 * TrieNode.
 */
const char* FindRegistryLeafNode(const char* component,
                                 const struct TrieNode* parent);

/* Get the hostname part for the given string table offset. */
const char* GetHostnamePart(size_t offset);

/* Does the given node have all leaf children? */
int HasLeafChildren(const struct TrieNode* node);

/*
 * Initialize the registry tables. Called at system startup by
 * InitializeDomainRegistry().
 */
void SetRegistryTables(const char* string_table,
                       const struct TrieNode* node_table,
                       size_t num_root_children,
                       const REGISTRY_U16* leaf_node_table,
                       size_t leaf_node_table_offset);

#endif  /* DOMAIN_REGISTRY_PRIVATE_TRIE_SEARCH_H_ */
