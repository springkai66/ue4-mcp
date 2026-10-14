#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Project-wide MCP commands
 */
class UNREALMCP_API FUnrealMCPProjectCommands
{
public:
    FUnrealMCPProjectCommands();

    // Handle project commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Specific project command handlers
    TSharedPtr<FJsonObject> HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params);

    // DataTable commands (V2 config basis)
    TSharedPtr<FJsonObject> HandleCreateDataTable(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddRow(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadRow(const TSharedPtr<FJsonObject>& Params);

    // 按资产路径设置任意 UObject（如 DataAsset 实例）的属性并保存
    TSharedPtr<FJsonObject> HandleSetAssetProperty(const TSharedPtr<FJsonObject>& Params);
}; 