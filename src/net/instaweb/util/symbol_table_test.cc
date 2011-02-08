/*
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the symbol table

#include "net/instaweb/util/public/symbol_table.h"
#include "base/logging.h"
#include "net/instaweb/util/public/gtest.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class SymbolTableTest : public testing::Test {
};

TEST_F(SymbolTableTest, TestInternSensitive) {
  SymbolTableSensitive symbol_table;
  std::string s1("hello");
  std::string s2("hello");
  std::string s3("goodbye");
  std::string s4("Goodbye");
  EXPECT_NE(s1.c_str(), s2.c_str());
  Atom a1 = symbol_table.Intern(s1);
  Atom a2 = symbol_table.Intern(s2);
  Atom a3 = symbol_table.Intern(s3);
  Atom a4 = symbol_table.Intern(s4);
  EXPECT_TRUE(a1 == a2);
  EXPECT_EQ(a1.c_str(), a2.c_str());
  EXPECT_FALSE(a1 == a3);
  EXPECT_NE(a1.c_str(), a3.c_str());
  EXPECT_FALSE(a3 == a4);

  EXPECT_EQ(s1, a1.c_str());
  EXPECT_EQ(s2, a2.c_str());
  EXPECT_EQ(s3, a3.c_str());
  EXPECT_EQ(s4, a4.c_str());

  Atom empty = symbol_table.Intern("");
  EXPECT_TRUE(Atom() == empty);
}

TEST_F(SymbolTableTest, TestInternInsensitive) {
  SymbolTableInsensitive symbol_table;
  std::string s1("hello");
  std::string s2("Hello");
  std::string s3("goodbye");
  Atom a1 = symbol_table.Intern(s1);
  Atom a2 = symbol_table.Intern(s2);
  Atom a3 = symbol_table.Intern(s3);
  EXPECT_TRUE(a1 == a2);
  EXPECT_EQ(a1.c_str(), a2.c_str());
  EXPECT_FALSE(a1 == a3);
  EXPECT_NE(a1.c_str(), a3.c_str());

  EXPECT_EQ(0, StringCaseCompare(s1.c_str(), a1.c_str()));
  EXPECT_EQ(0, StringCaseCompare(s2.c_str(), a2.c_str()));
  EXPECT_EQ(0, StringCaseCompare(s3.c_str(), a3.c_str()));

  Atom empty = symbol_table.Intern("");
  EXPECT_TRUE(Atom() == empty);
}

}  // namespace net_instaweb
