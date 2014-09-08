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

#include <vector>
#include <utility>

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

namespace {

class DecisionTreeTest : public ::testing::Test {
 protected:
  typedef DecisionTree::Node Node;
};

TEST_F(DecisionTreeTest, CreateTree) {
  const Node nodes[] = {
    {0, 0.5, -1.0, &nodes[1], &nodes[2]},   // node 0, inner
    {-1, -1.0, 0.7, NULL, NULL},            // node 1, leaf
    {1, 30.0, -1.0, &nodes[3], &nodes[4]},  // node 2, inner
    {-1, -1.0, 1.0, NULL, NULL},            // node 3, leaf
    {-1, -1.0, 0.0, NULL, NULL}             // node 4, leaf
  };
  DecisionTree tree(nodes, arraysize(nodes));
  EXPECT_EQ(tree.num_features(), 2);
}

TEST_F(DecisionTreeTest, PredictionTest) {
  // Build tree that looks like this:
  //        X[0] <= 0.5
  //       /           \
  //      /             \
  // X[2] <= 0.9    X[1] <= 30.0
  //   /    \         /        \
  //  /      \       /          \
  // 0.4     0.2    1.0         0.0
  const Node nodes[] = {
    {0, 0.5, -1.0, &nodes[1], &nodes[4]},   // node 0, inner
    {2, 0.9, -1.0, &nodes[2], &nodes[3]},   // node 1, inner
    {-1, -1.0, 0.4, NULL, NULL},            // node 2, leaf
    {-1, -1.0, 0.2, NULL, NULL},            // node 3, leaf
    {1, 30.0, -1.0, &nodes[5], &nodes[6]},  // node 4, inner
    {-1, -1.0, 1.0, NULL, NULL},            // node 5, leaf
    {-1, -1.0, 0.0, NULL, NULL}             // node 6, leaf
  };
  DecisionTree tree(nodes, arraysize(nodes));

  std::vector<double> sample(3, 0.0);
  EXPECT_EQ(0.4, tree.Predict(sample));
  sample[0] = 0.45;
  EXPECT_EQ(0.4, tree.Predict(sample));
  sample[2] = 1.0;
  EXPECT_EQ(0.2, tree.Predict(sample));
  sample[0] = 0.6;
  EXPECT_EQ(1.0, tree.Predict(sample));
  sample[1] = 45.2;
  EXPECT_EQ(0.0, tree.Predict(sample));
}

class DecisionTreeDeathTest : public DecisionTreeTest {
 protected:
  DecisionTreeDeathTest() {
  }
};

TEST_F(DecisionTreeDeathTest, OneChildDeathTest) {
  const Node nodes[] = {
    {0, 0.5, -1.0, &nodes[1], &nodes[2]},  // node 0, inner
    {-1, -1.0, 0.7, NULL, NULL},           // node 1, leaf
    {1, 30.0, -1.0, &nodes[3], NULL},      // node 2, inner
    {-1, -1.0, 1.0, NULL, NULL}            // node 3, leaf
  };
  int num_nodes = arraysize(nodes);
  ASSERT_DEATH(DecisionTree tree(nodes, num_nodes), "Inner node has one child");
}

TEST_F(DecisionTreeDeathTest, UnreachableNodesDeathTest) {
  const Node nodes[] = {
    {0, 0.5, -1.0, &nodes[1], &nodes[2]},  // node 0, inner
    {-1, -1.0, 0.7, NULL, NULL},           // node 1, leaf
    {-1, -1.0, 0.3, NULL, NULL},           // node 2, leaf
    {-1, -1.0, 1.0, NULL, NULL}            // node 3, leaf
  };
  int num_nodes = arraysize(nodes);
  ASSERT_DEATH(DecisionTree tree(nodes, num_nodes), "Unreachable nodes");
}

TEST_F(DecisionTreeDeathTest, ExtraneousNodesDeathTest) {
  const Node extra_node = {-1, -1.0, 1.0, NULL, NULL};
  const Node nodes[] = {
    {0, 0.5, -1.0, &nodes[1], &nodes[2]},    // node 0, inner
    {-1, -1.0, 0.7, NULL, NULL},             // node 1, leaf
    {1, 0.1, -1.0, &nodes[3], &extra_node},  // node 2, inner
    {-1, -1.0, 1.0, NULL, NULL}              // node 3, leaf
  };
  int num_nodes = arraysize(nodes);
  ASSERT_DEATH(DecisionTree tree(nodes, num_nodes), "Extraneous nodes");
}

TEST_F(DecisionTreeDeathTest, InvalidFeatureIndexDeathTest) {
  const Node nodes[] = {
    {-10, 0.5, -1.0, &nodes[1], &nodes[2]},  // node 0, inner
    {-1, -1.0, 0.7, NULL, NULL},             // node 1, leaf
    {-1, -1.0, 0.3, NULL, NULL}              // node 2, leaf
  };
  int num_nodes = arraysize(nodes);
  ASSERT_DEATH(DecisionTree tree(nodes, num_nodes), "Invalid feature index");
}

TEST_F(DecisionTreeDeathTest, InvalidConfidenceDeathTest) {
  const Node nodes[] = {
    {0, 0.5, -1.0, &nodes[1], &nodes[2]},  // node 0, inner
    {-1, -1.0, 1.7, NULL, NULL},           // node 1, leaf
    {-1, -1.0, 0.3, NULL, NULL}            // node 2, leaf
  };
  int num_nodes = arraysize(nodes);
  ASSERT_DEATH(DecisionTree tree(nodes, num_nodes), "Invalid confidence 1.7");
}

}  // namespace
}  // namespace net_instaweb
