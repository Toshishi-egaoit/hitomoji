#include "gtest/gtest.h"
#include "ChmConfig.h"
#include "ChmEngine.h"
#include "ChmEnvironment.h"
#include "ChmKeyEvent.h"
#include "ChmRomajiConverter.h"
#include "TestConfigHelper.h"
#include "TestKeyEventHelper.h"
#include <string>
#include <sstream>

namespace {

std::wstring ConvertWithPending(const std::wstring& input, bool bs = true)
{
    std::wstring converted;
    std::wstring pending;
    ChmRomajiConverter::convert(input, converted, pending, bs);
    return converted + L":" + pending;
}
ChmFuncType TypeForKey(WPARAM key, bool ctrl = false,
                       ChmEngine::State state = ChmEngine::State::Inputing)
{
    TestKeyEventHelper::SetState(false, ctrl);
    ChmKeyEvent event(key, 0, state);
    return event.GetType();
}


class EngineConfigTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        g_environment.Init();
        _oldBasePath = g_environment.GetBasePath();
        g_environment.SetBasePath(_dir.BaseDir());
        TestKeyEventHelper::Install();
        ChmKeyEvent::InitFunctionKey();
    }

    void TearDown() override
    {
        TestKeyEventHelper::Restore();
        ChmKeyEvent::InitFunctionKey();
        g_environment.SetBasePath(_oldBasePath);
    }

    void WriteConfig(const std::wstring& content)
    {
        _dir.WriteFile(L"Hitomoji.ini", content);
    }

    TempConfigDir _dir;
    std::wstring _oldBasePath;
};

} // namespace

TEST_F(EngineConfigTest, ActivateLoadsConfigFromEnvironmentBasePath)
{
    WriteConfig(
        L"[ui]\n"
        L"loglevel=debug\n"
        L"candidate-delay=321\n");

    ChmEngine engine;
	EngineActivate(engine);

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(_dir.BaseDir(), engine.GetConfig()->GetConfigPath());
    EXPECT_EQ(321, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));

    engine.Deactivate();
}

TEST_F(EngineConfigTest, ActivateUsesEmptyConfigWhenInitialLoadFails)
{
    ChmEngine engine;
	EngineActivate(engine);

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(_dir.BaseDir(), engine.GetConfig()->GetConfigPath());
    EXPECT_TRUE(engine.GetConfig()->GetBool(L"undefined", L"missing-bool"));
    EXPECT_EQ(0, engine.GetConfig()->GetLong(L"undefined", L"missing-long"));
    EXPECT_EQ(L"", engine.GetConfig()->GetString(L"undefined", L"missing-string"));

    engine.Deactivate();
}

TEST_F(EngineConfigTest, InitConfigReplacesExistingConfigAndFunctionKeysAfterSuccessfulReload)
{
    WriteConfig(
        L"[ui]\n"
        L"candidate-delay=111\n"
        L"[function-key]\n"
        L"CTRL+Y=cancel\n");

    ChmEngine engine;
	EngineActivate(engine);

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(111, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_EQ(ChmFuncType::Cancel, TypeForKey('Y', true));

    WriteConfig(
        L"[ui]\n"
        L"candidate-delay=222\n"
        L"[function-key]\n"
        L"CTRL+Y=finish-raw\n");
    engine.InitConfig();

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(222, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_EQ(ChmFuncType::CompFinishKey, TypeForKey('Y', true));

    engine.Deactivate();
}

TEST_F(EngineConfigTest, InitConfigKeepsExistingConfigAndFunctionKeysAfterFailedReload)
{
    WriteConfig(
        L"[ui]\n"
        L"candidate-delay=111\n"
        L"[function-key]\n"
        L"CTRL+Y=cancel\n");

    ChmEngine engine;
	EngineActivate(engine);

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(111, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_EQ(ChmFuncType::Cancel, TypeForKey('Y', true));

    WriteConfig(
        L"[ui\n"
        L"candidate-delay=222\n"
        L"[function-key]\n"
        L"CTRL+Y=finish-raw\n");
    engine.InitConfig();

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(111, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_EQ(ChmFuncType::Cancel, TypeForKey('Y', true));

    engine.Deactivate();
}
TEST_F(EngineConfigTest, InitConfigKeepsKeyTableOverridesAfterFailedReload)
{
    WriteConfig(
        L"[ui]\n"
        L"candidate-delay=111\n"
        L"[key-table]\n"
        L"wi=うぃ\n");

    ChmEngine engine;
	EngineActivate(engine);

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(111, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_EQ(L"うぃ:", ConvertWithPending(L"wi"));

    WriteConfig(
        L"[ui\n"
        L"candidate-delay=222\n"
        L"[key-table]\n"
        L"wi=ゐ\n");
    engine.InitConfig();

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(111, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_EQ(L"うぃ:", ConvertWithPending(L"wi"));

    engine.Deactivate();
}

TEST_F(EngineConfigTest, ActivateReloadAndReactivateAcquireFreshDefaultStreams)
{
    int acquisitions = 0;
    ChmEngine engine([&]() -> std::unique_ptr<std::wistream> {
        ++acquisitions;
        return std::make_unique<std::wistringstream>(
            L"[ui]\ncandidate-delay=" + std::to_wstring(500 + acquisitions) +
            L"\n[function-key]\nCTRL+Y=finish-raw\n[key-table]\n/half=\u00BD\n");
    });
    std::istringstream dictionary;
    ASSERT_TRUE(engine.Activate(dictionary));
    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(1, acquisitions);
    EXPECT_EQ(501, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_EQ(ChmFuncType::CompFinishKey, TypeForKey('Y', true));
    EXPECT_EQ(L"\u00BD:", ConvertWithPending(L"/half"));
    ASSERT_TRUE(engine.InitConfig());
    EXPECT_EQ(2, acquisitions);
    EXPECT_EQ(502, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    engine.Deactivate();
    ASSERT_TRUE(engine.Activate(dictionary));
    EXPECT_EQ(3, acquisitions);
    EXPECT_EQ(503, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    engine.Deactivate();
}

TEST_F(EngineConfigTest, UserOverridesFreshDefaultsAndReloadRestoresRemovedOverrides)
{
    WriteConfig(L"[ui]\ncandidate-delay=123\n");
    int acquisitions = 0;
    ChmEngine engine([&]() -> std::unique_ptr<std::wistream> {
        ++acquisitions;
        return std::make_unique<std::wistringstream>(L"[ui]\ncandidate-delay=500\nwide-space=false\n");
    });
    std::istringstream dictionary;
    ASSERT_TRUE(engine.Activate(dictionary));
    EXPECT_EQ(1, acquisitions);
    EXPECT_EQ(123, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_FALSE(engine.GetConfig()->GetBool(L"ui", L"wide-space"));
    WriteConfig(L"# use defaults\n");
    ASSERT_TRUE(engine.InitConfig());
    EXPECT_EQ(2, acquisitions);
    EXPECT_EQ(500, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_FALSE(engine.GetConfig()->GetBool(L"ui", L"wide-space"));
    engine.Deactivate();
}

TEST_F(EngineConfigTest, FailedResourceAcquisitionKeepsExistingConfig)
{
    int acquisitions = 0;
    ChmEngine engine([&]() -> std::unique_ptr<std::wistream> {
        if (++acquisitions == 2) return nullptr;
        return std::make_unique<std::wistringstream>(L"[ui]\ncandidate-delay=500\n[key-table]\n/half=\u00BD\n");
    });
    std::istringstream dictionary;
    ASSERT_TRUE(engine.Activate(dictionary));
    ChmConfig* previous = engine.GetConfig();
    EXPECT_FALSE(engine.InitConfig());
    EXPECT_EQ(previous, engine.GetConfig());
    EXPECT_EQ(500, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_EQ(L"\u00BD:", ConvertWithPending(L"/half"));
    engine.Deactivate();
}

TEST_F(EngineConfigTest, InvalidFreshDefaultsKeepExistingConfigAndSymbols)
{
    int acquisitions = 0;
    ChmEngine engine([&]() -> std::unique_ptr<std::wistream> {
        if (++acquisitions == 2)
            return std::make_unique<std::wistringstream>(L"[ui\n[key-table]\n/half=wrong\n");
        return std::make_unique<std::wistringstream>(L"[ui]\ncandidate-delay=500\n[key-table]\n/half=\u00BD\n");
    });
    std::istringstream dictionary;
    ASSERT_TRUE(engine.Activate(dictionary));
    ChmConfig* previous = engine.GetConfig();
    EXPECT_FALSE(engine.InitConfig());
    EXPECT_EQ(previous, engine.GetConfig());
    EXPECT_EQ(L"\u00BD:", ConvertWithPending(L"/half"));
    engine.Deactivate();
}

TEST_F(EngineConfigTest, InitialUserSyntaxErrorAcquiresDefaultsAgain)
{
    WriteConfig(L"[ui\n[key-table]\n/half=wrong\n");
    int acquisitions = 0;
    ChmEngine engine([&]() -> std::unique_ptr<std::wistream> {
        ++acquisitions;
        return std::make_unique<std::wistringstream>(L"[ui]\ncandidate-delay=500\n[key-table]\n/half=\u00BD\n");
    });
    std::istringstream dictionary;
    ASSERT_TRUE(engine.Activate(dictionary));
    EXPECT_EQ(2, acquisitions);
    EXPECT_EQ(500, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_FALSE(engine.GetConfig()->HasErrors());
    EXPECT_EQ(L"\u00BD:", ConvertWithPending(L"/half"));
    engine.Deactivate();
}

#ifdef _DEBUG
TEST_F(EngineConfigTest, DebugReloadKeyAlsoAcquiresFreshDefaults)
{
    int acquisitions = 0;
    ChmEngine engine([&]() -> std::unique_ptr<std::wistream> {
        ++acquisitions;
        return std::make_unique<std::wistringstream>(L"[ui]\ncandidate-delay=500\n");
    });
    std::istringstream dictionary;
    ASSERT_TRUE(engine.Activate(dictionary));
    TestKeyEventHelper::SetState(true, true);
    ChmKeyEvent key('R', 0, ChmEngine::State::None);
    ASSERT_EQ(ChmFuncType::ReloadIni, key.GetType());
    bool endComposition = false;
    ASSERT_TRUE(engine.UpdateOther(key, endComposition));
    EXPECT_EQ(2, acquisitions);
    engine.Deactivate();
}
#endif
