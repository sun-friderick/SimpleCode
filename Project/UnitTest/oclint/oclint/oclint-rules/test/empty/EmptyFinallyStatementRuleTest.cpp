#include "TestHeaders.h"

#include "rules/empty/EmptyFinallyStatementRule.cpp"

TEST(EmptyFinallyStatementRuleTest, PropertyTest)
{
    EmptyFinallyStatementRule rule;
    EXPECT_EQ(2, rule.priority());
    EXPECT_EQ("empty finally statement", rule.name());
}

TEST(EmptyFinallyStatementRuleTest, NonEmptyObjCFinallyStmt)
{
    testRuleOnObjCCode(new EmptyFinallyStatementRule(), "void m() { @try { ; } @catch(id ex) { ; } @finally { ; } }");
}

TEST(EmptyFinallyStatementRuleTest, EmptyObjCFinallyWithEmptyCompoundStmt)
{
    testRuleOnObjCCode(new EmptyFinallyStatementRule(), "void m() { @try { ; } @catch(id ex) { ; } @finally {} }",
        0, 1, 52, 1, 53);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
