#include "gtest/gtest.h"
#include "ChmConfig.h"

TEST(ConfigTest, LoadValidFile)
{
    ChmConfig config;

	EXPECT_TRUE(config.LoadFile(L".\\testdata\\valid.ini"));

    EXPECT_TRUE(config.GetBool(L"bools", L"bool_true_1"));
    EXPECT_TRUE(config.GetBool(L"BOOLS", L"BOOL_TRUE_2"));
    EXPECT_TRUE(config.GetBool(L"Bools", L"Bool_True_3"));
    EXPECT_TRUE(config.GetBool(L"bools", L"bool_true_4"));
    EXPECT_TRUE(config.GetBool(L"bools", L"bool_true_5"));
    EXPECT_TRUE(config.GetBool(L"bools", L"bool_true_6"));
    EXPECT_FALSE(config.GetBool(L"bools", L"bool_false_1"));
    EXPECT_FALSE(config.GetBool(L"BOOLS", L"BOOL_FALSE_2"));
    EXPECT_FALSE(config.GetBool(L"Bools", L"Bool_False_3"));
    EXPECT_FALSE(config.GetBool(L"bools", L"bool_false_4"));

    EXPECT_EQ(123u, config.GetLong(L"numbers", L"long_1"));
    EXPECT_EQ(-789u, config.GetLong(L"NUMBERS", L"Long_2"));
    EXPECT_EQ(210000000u, config.GetLong(L"numbers", L"long_3"));

    EXPECT_EQ(L"hitomoji", config.GetString(L"strings", L"string_1"));
    EXPECT_EQ(L"C:\\temp\\FILE.txt", config.GetString(L"strings", L"valid_key_9"));
}

TEST(ConfigTest, WhitespaceLine)
{
    ChmConfig config;
	
	EXPECT_TRUE(config.LoadFile(L".\\testdata\\ChmConfig.null.ini"));

    EXPECT_FALSE(config.GetBool(L"undefined", L"not_exist_bool_key"));
    EXPECT_EQ(0, config.GetLong(L"undefined", L"not_exist_long_key"));
    EXPECT_EQ(L"", config.GetString(L"undefined", L"not_exist_string_key"));
}

