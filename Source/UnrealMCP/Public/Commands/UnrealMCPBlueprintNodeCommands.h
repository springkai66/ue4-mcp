#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Blueprint Node-related MCP commands
 */
class UNREALMCP_API FUnrealMCPBlueprintNodeCommands
{
public:
    FUnrealMCPBlueprintNodeCommands();

    // Handle blueprint node commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // 查询和整理图表只使用现有 Event Graph
    TSharedPtr<FJsonObject> HandleGraphLayout(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

    // Specific blueprint node command handlers
    TSharedPtr<FJsonObject> HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintGetSelfComponentReference(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintEvent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintFunctionCall(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintVariable(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintInputActionNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintSelfReference(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    // 新增：创建 Cast 节点（K2Node_DynamicCast），用于把通用引用转为具体类。
    TSharedPtr<FJsonObject> HandleAddBlueprintCastNode(const TSharedPtr<FJsonObject>& Params);
    // 新增：在指定类（如 Cast 结果）上读取成员变量或组件，解决只能取 Self 成员的限制。
    TSharedPtr<FJsonObject> HandleAddBlueprintExternalMemberGetNode(const TSharedPtr<FJsonObject>& Params);
    // 读取节点详情与全部连线（含引脚默认值）。
    TSharedPtr<FJsonObject> HandleGetBlueprintNodeDetails(const TSharedPtr<FJsonObject>& Params);
    // 删除指定节点并断开连线，保护 BeginPlay 入口。
    TSharedPtr<FJsonObject> HandleDeleteBlueprintNode(const TSharedPtr<FJsonObject>& Params);
}; 