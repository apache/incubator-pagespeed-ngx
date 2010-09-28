// Copyright 2010 and onwards Google Inc.
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

  EXPECT_EQ(0, strcasecmp(s1.c_str(), a1.c_str()));
  EXPECT_EQ(0, strcasecmp(s2.c_str(), a2.c_str()));
  EXPECT_EQ(0, strcasecmp(s3.c_str(), a3.c_str()));
}

}  // namespace net_instaweb
