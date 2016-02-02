#include "TestHeaders.h"

#include "rules/empty/EmptyDoWhileStatementRule.cpp"

TEST(EmptyDoWhileStatementRuleTest, PropertyTest)
{
    EmptyDoWhileStatementRule rule;
    EXPECT_EQ(2, rule.priority());
    EXPECT_EQ("empty do/while statement", rule.name());
}

TEST(EmptyDoWhileStatementRuleTest, GoodWhileStatement)
{
    testRuleOnCode(new EmptyDoWhileStatementRule(), "void m() { do {;} while(1); }");
}

TEST(EmptyDoWhileStatementRuleTest, WhileStatementWithEmptyComponent)
{
    testRuleOnCode(new EmptyDoWhileStatementRule(), "void m() { do {} while(1); }",
        0, 1, 15, 1, 16);
}

TEST(EmptyDoWhileStatementRuleTest, WhileStatementWithNullStmt)
{
    testRuleOnCode(new EmptyDoWhileStatementRule(), "void m() { do \n;\nwhile(1); }",
        0, 2, 1, 2, 1);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
