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

#ifndef DOMAIN_REGISTRY_PRIVATE_TRIE_NODE_H_
#define DOMAIN_REGISTRY_PRIVATE_TRIE_NODE_H_

#pragma pack(push)
#pragma pack(1)

/*
 * TrieNode represents a single node in a Trie. It uses 5 bytes of
 * storage.
 */
struct TrieNode {
  /*
   * Index in the string table for the hostname-part associated with
   * this node.
   */
  unsigned int string_table_offset  : 15;

  /*
   * Offset of the first child of this node in the node table. All
   * children are stored adjacent to each other, sorted
   * lexicographically by their hostname parts.
   */
  unsigned int first_child_offset   : 13;

  /*
   * Number of children of this node.
   */
  unsigned int num_children         : 11;

  /*
   * Whether this node is a "terminal" node. A terminal node is one
   * that represents the end of a sequence of nodes in the trie. For
   * instance if the sequences "com.foo.bar" and "com.foo" are added
   * to the trie, "bar" and "foo" are terminal nodes, since they are
   * both at the end of their sequences.
   */
  unsigned int is_terminal          :  1;
};

#pragma pack(pop)

#endif  /* DOMAIN_REGISTRY_PRIVATE_TRIE_NODE_H_ */
