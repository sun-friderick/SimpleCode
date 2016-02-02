#include "TestHeaders.h"

#include "rules/convention/DefaultLabelNotLastInSwitchStatementRule.cpp"

TEST(DefaultLabelNotLastInSwitchStatementRuleTest, PropertyTest)
{
    DefaultLabelNotLastInSwitchStatementRule rule;
    EXPECT_EQ(3, rule.priority());
    EXPECT_EQ("default label not last in switch statement", rule.name());
}

TEST(DefaultLabelNotLastInSwitchStatementRuleTest, DefaultIsLast)
{
    testRuleOnCode(new DefaultLabelNotLastInSwitchStatementRule(), "void aMethod(int a) { switch(a){\n\
case 1:     \n\
\tbreak;    \n\
default:    \n\
\tbreak;    \n\
} }");
}

TEST(DefaultLabelNotLastInSwitchStatementRuleTest, DefaultInTheMiddle)
{
    testRuleOnCode(new DefaultLabelNotLastInSwitchStatementRule(), "void aMethod(int a) { switch(a){\n\
case 1:     \n\
\tbreak;    \n\
default:    \n\
\tbreak;    \n\
case 2:     \n\
\tbreak;    \n\
} }",
        0, 4, 1, 5, 2);
}

TEST(DefaultLabelNotLastInSwitchStatementRuleTest, SwitchNoDefault)
{
    testRuleOnCode(new DefaultLabelNotLastInSwitchStatementRule(), "void aMethod(int a) { switch(a){\n\
case 1:     \n\
\tbreak;    \n\
case 2:     \n\
\tbreak;    \n\
} }");
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
