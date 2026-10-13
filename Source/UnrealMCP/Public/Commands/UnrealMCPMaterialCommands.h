#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * 处理材质相关命令，支持创建、编辑材质资产及分配网格组件材质。
 */
class UNREALMCP_API FUnrealMCPMaterialCommands
{
public:
    FUnrealMCPMaterialCommands();

    // Handle material commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Specific material command handlers
    TSharedPtr<FJsonObject> HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddVectorParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialOnComponent(const TSharedPtr<FJsonObject>& Params);
};
