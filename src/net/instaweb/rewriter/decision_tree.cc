/*
 * Copyright 2014 Google Inc.
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

// Author: stevensr@google.com (Ryan Stevens)

#include "net/instaweb/rewriter/public/decision_tree.h"

#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/stdio_file_system.h"

namespace net_instaweb {

DecisionTree::DecisionTree(const Node* nodes, int num_nodes)
    : nodes_(nodes),
      num_nodes_(num_nodes),
      num_features_(0) {

#ifndef NDEBUG
  DCHECK_LT(0, num_nodes_);
  SanityCheck();
#endif

  int max_feature_index = -1;
  for (int i = 0; i < num_nodes_; ++i) {
    if (nodes[i].feature_index > max_feature_index) {
      max_feature_index = nodes[i].feature_index;
    }
  }
  num_features_ = max_feature_index + 1;
}

DecisionTree::~DecisionTree() {}

const DecisionTree::Node* DecisionTree::Root() {
  return &nodes_[0];
}

double DecisionTree::Predict(std::vector<double> const& sample) {
  DCHECK_EQ(static_cast<int>(sample.size()), num_features_);
  const Node* cur = Root();
  while (cur != NULL && !cur->IsLeafNode()) {
    if (sample[cur->feature_index] <= cur->feature_threshold) {
      cur = cur->left;
    } else {
      cur = cur->right;
    }
  }
  CHECK(cur != NULL);
  return cur->confidence;
}

void DecisionTree::SanityCheck() {
  // We will do a tree traversal to to ensure the following invariants about
  // a constructed decision tree:
  // 1) All nodes are reachable.
  // 2) All nodes have 2 (inner nodes) or 0 (leaf nodes) children.
  // 3) All inner nodes have a feature_index > 0.
  // 4) All leaf nodes have a 0.0 <= confidence <= 1.0.
  const Node* root = Root();
  int num_observed_nodes = 0;
  SanityCheckTraversal(root, &num_observed_nodes);
  DCHECK_GE(num_observed_nodes, num_nodes_) << "Unreachable nodes";
  DCHECK_LE(num_observed_nodes, num_nodes_) << "Extraneous nodes";
}

void DecisionTree::SanityCheckTraversal(const Node* cur, int* num_nodes) {
  (*num_nodes)++;
  DCHECK((cur->left != NULL && cur->right != NULL) ||
         (cur->left == NULL && cur->right == NULL))
      << "Inner node has one child";
  if (cur->IsLeafNode()) {
    DCHECK(cur->confidence >= 0.0 && cur->confidence <= 1.0)
        << "Invalid confidence " << cur->confidence;
  } else {
    DCHECK_LE(0, cur->feature_index) << "Invalid feature index";
    SanityCheckTraversal(cur->left, num_nodes);
    SanityCheckTraversal(cur->right, num_nodes);
  }
}

}  // namespace net_instaweb
