#include "gtest/gtest.h"
#include "ChmRomajiConverter.h"

static std::wstring ConvertWithPending(const std::wstring& input,
                                       bool disp = true,
                                       bool bs = true)
{
    std::wstring converted;
    std::wstring pending;

    ChmRomajiConverter::convert(input, converted, pending, disp, bs);
    return converted + L":" + pending;
}

//
// 基本
//
TEST(ConvertTest, BasicVowel)
{
    EXPECT_EQ(L"あ:", ConvertWithPending(L"a"));
}

TEST(ConvertTest, BasicConsonant)
{
    EXPECT_EQ(L"か:", ConvertWithPending(L"ka"));
}

TEST(ConvertTest, Nn)
{
    EXPECT_EQ(L"ん:", ConvertWithPending(L"nn"));
}

//
// 拗音
//
TEST(ConvertTest, Youon)
{
    EXPECT_EQ(L"きゃ:", ConvertWithPending(L"kya"));
    EXPECT_EQ(L"しゃ:", ConvertWithPending(L"sha"));
    EXPECT_EQ(L"りょ:", ConvertWithPending(L"ryo"));
}

//
// 促音
//
TEST(ConvertTest, Sokuon)
{
    EXPECT_EQ(L"った:", ConvertWithPending(L"tta"));
    EXPECT_EQ(L"っか:", ConvertWithPending(L"kka"));
}

//
// 記号
//
TEST(ConvertTest, Symbols)
{
    EXPECT_EQ(L"。:", ConvertWithPending(L"."));
    EXPECT_EQ(L"、:", ConvertWithPending(L","));
    EXPECT_EQ(L"？:", ConvertWithPending(L"?"));
    EXPECT_EQ(L"「:", ConvertWithPending(L"["));
}

//
// pending
//
TEST(ConvertTest, PendingCases)
{
    EXPECT_EQ(L":k", ConvertWithPending(L"k"));
    EXPECT_EQ(L":ky", ConvertWithPending(L"ky"));
    EXPECT_EQ(L"っ:t", ConvertWithPending(L"tt"));
}

//
// フラグ差分
//
TEST(ConvertTest, DisplayOff)
{
    // isDisplayKana = false → 生文字出力
    EXPECT_EQ(L"ka:", ConvertWithPending(L"ka", false));
    EXPECT_EQ(L"ka:k", ConvertWithPending(L"kak", false));
}

TEST(ConvertTest, BackspaceMode)
{
    ConvertWithPending(L"ka", true, true);
    EXPECT_EQ(2, ChmRomajiConverter::GetLastRawUnitLength());

    EXPECT_EQ(L":k", ConvertWithPending(L"k"));
    EXPECT_EQ(1, ChmRomajiConverter::GetLastRawUnitLength());
    EXPECT_EQ(L":ky", ConvertWithPending(L"ky"));
    EXPECT_EQ(2, ChmRomajiConverter::GetLastRawUnitLength());
    EXPECT_EQ(L":chm", ConvertWithPending(L"chm"));
    EXPECT_EQ(3, ChmRomajiConverter::GetLastRawUnitLength());
    EXPECT_EQ(L"chま:", ConvertWithPending(L"chma"));
    EXPECT_EQ(1, ChmRomajiConverter::GetLastRawUnitLength());
    EXPECT_EQ(L"っ:t", ConvertWithPending(L"tt"));
    EXPECT_EQ(1, ChmRomajiConverter::GetLastRawUnitLength());

    EXPECT_EQ(L":ky", ConvertWithPending(L"ky",true,false));
    EXPECT_EQ(1, ChmRomajiConverter::GetLastRawUnitLength());
}

//
// RawLength検証
//
TEST(ConvertTest, RawLengthNormal)
{
    ConvertWithPending(L"ka");
    EXPECT_EQ(1, ChmRomajiConverter::GetLastRawUnitLength());
}

TEST(ConvertTest, RawLengthPending)
{
    ConvertWithPending(L"k");
    EXPECT_EQ(1, ChmRomajiConverter::GetLastRawUnitLength());
    ConvertWithPending(L"ch");
    EXPECT_EQ(2, ChmRomajiConverter::GetLastRawUnitLength());
}
