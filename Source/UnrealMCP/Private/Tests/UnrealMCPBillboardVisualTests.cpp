#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/UnrealMCPUMGCommands.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "UnrealMCPBridge.h"
#include "WidgetBlueprint.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

// 真实工程视觉验证：PIE 中把冠军广告牌加入视口并截图落盘。
// 依赖目标工程的 UI_GVGChampionBoard 资产，隔离宿主没有该资产；
// 用 "UnrealMCP.Visual" 前缀单独运行，不进入默认 UnrealMCP 集合。
// 截图对比在测试外部完成；用例断言命令链路与渲染前置条件成立。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPUMGBillboardVisualTest, "UnrealMCPVisual.BillboardPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace UnrealMCPVisualHelpers
{
    // ExecuteCommand 返回 JSON 字符串；解析出 status/error 便于断言。
    TSharedPtr<FJsonObject> Parse(const FString& Response)
    {
        TSharedPtr<FJsonObject> Object;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);
        if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
        {
            Object = MakeShared<FJsonObject>();
            Object->SetStringField(TEXT("status"), TEXT("error"));
            Object->SetStringField(TEXT("error"), Response.Left(200));
        }
        return Object;
    }
}

bool FMCPUMGBillboardVisualTest::RunTest(const FString& Parameters)
{
    using namespace UnrealMCPVisualHelpers;
    UUnrealMCPBridge* Bridge = GEditor ? GEditor->GetEditorSubsystem<UUnrealMCPBridge>() : nullptr;
    if (!TestNotNull(TEXT("Bridge available"), Bridge))
    {
        return false;
    }
    // 蓝图已存在于目标工程；视觉验证只消费它，不创建不修改。
    const TCHAR* BlueprintPath = TEXT("/Game/Resources/CoreBlueprints/UI/GVG/UI_GVGChampionBoard");
    auto TreeParams = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Tree = Parse(Bridge->ExecuteCommand(TEXT("get_widget_tree"), TreeParams));
    TestEqual(TEXT("Billboard tree readable"), Tree->GetStringField(TEXT("status")), FString(TEXT("success")));

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    const FString OutputDir = FPaths::ProjectSavedDir() / TEXT("UnrealMCPVisual");
    if (!PlatformFile.CreateDirectoryTree(*OutputDir))
    {
        AddError(TEXT("Could not create screenshot output directory"));
        return false;
    }
    const FString ScreenshotPath = OutputDir / TEXT("GvgChampionBoardPIE.png");
    IFileManager::Get().Delete(*ScreenshotPath);

    auto ViewportParams = MakeShared<FJsonObject>();
    ViewportParams->SetStringField(TEXT("blueprint_name"), BlueprintPath);
    TSharedPtr<FJsonObject> Viewport = Parse(Bridge->ExecuteCommand(TEXT("add_widget_to_viewport"), ViewportParams));
    if (Viewport->GetStringField(TEXT("status")) == TEXT("error"))
    {
        AddError(FString::Printf(TEXT("AddToViewport failed: %s"), *Viewport->GetStringField(TEXT("error"))));
        return false;
    }

    // 给 Slate 一帧完成布局与绘制
    FPlatformProcess::Sleep(1.5f);
    auto ShotParams = MakeShared<FJsonObject>();
    ShotParams->SetStringField(TEXT("filepath"), ScreenshotPath);
    TSharedPtr<FJsonObject> Shot = Parse(Bridge->ExecuteCommand(TEXT("take_screenshot"), ShotParams));
    TestFalse(TEXT("Screenshot captured"), Shot->GetStringField(TEXT("status")) == TEXT("error"));
    TestTrue(TEXT("Screenshot file exists"), IFileManager::Get().FileExists(*ScreenshotPath));
    return !HasAnyErrors();
}
#endif
