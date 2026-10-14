#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "GameFramework/InputSettings.h"
#include "Engine/DataTable.h"
#include "UObject/StructOnScope.h"
#include "JsonObjectConverter.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/DataTableFactory.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"

FUnrealMCPProjectCommands::FUnrealMCPProjectCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_input_mapping"))
    {
        return HandleCreateInputMapping(Params);
    }
    else if (CommandType == TEXT("create_data_table"))
    {
        return HandleCreateDataTable(Params);
    }
    else if (CommandType == TEXT("add_row"))
    {
        return HandleAddRow(Params);
    }
    else if (CommandType == TEXT("read_row"))
    {
        return HandleReadRow(Params);
    }
    else if (CommandType == TEXT("set_asset_property"))
    {
        return HandleSetAssetProperty(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown project command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    FString Key;
    if (!Params->TryGetStringField(TEXT("key"), Key))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'key' parameter"));
    }

    // Get the input settings
    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get input settings"));
    }

    // 客户端用 input_type 区分 Action 与 Axis；不能静默按 Action 处理 Axis 请求。
    FString InputType = TEXT("Action");
    Params->TryGetStringField(TEXT("input_type"), InputType);
    const bool bAxisMapping = InputType.Equals(TEXT("Axis"), ESearchCase::IgnoreCase);

    bool bShift = false;
    bool bCtrl = false;
    bool bAlt = false;
    bool bCmd = false;
    Params->TryGetBoolField(TEXT("shift"), bShift);
    Params->TryGetBoolField(TEXT("ctrl"), bCtrl);
    Params->TryGetBoolField(TEXT("alt"), bAlt);
    Params->TryGetBoolField(TEXT("cmd"), bCmd);

    const FName ActionFName(*ActionName);
    const FKey MappingKey(*Key);

    // 重复调用不应在配置里累积重复映射。
    if (bAxisMapping)
    {
        for (const FInputAxisKeyMapping& Existing : InputSettings->GetAxisMappings())
        {
            if (Existing.AxisName == ActionFName && Existing.Key == MappingKey)
            {
                return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Axis mapping already exists: %s -> %s"), *ActionName, *Key));
            }
        }
        FInputAxisKeyMapping AxisMapping;
        AxisMapping.AxisName = ActionFName;
        AxisMapping.Key = MappingKey;
        AxisMapping.Scale = 1.0f;
        InputSettings->AddAxisMapping(AxisMapping);
    }
    else
    {
        for (const FInputActionKeyMapping& Existing : InputSettings->GetActionMappings())
        {
            if (Existing.ActionName == ActionFName && Existing.Key == MappingKey)
            {
                return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Action mapping already exists: %s -> %s"), *ActionName, *Key));
            }
        }
        FInputActionKeyMapping ActionMapping;
        ActionMapping.ActionName = ActionFName;
        ActionMapping.Key = MappingKey;
        ActionMapping.bShift = bShift;
        ActionMapping.bCtrl = bCtrl;
        ActionMapping.bAlt = bAlt;
        ActionMapping.bCmd = bCmd;
        InputSettings->AddActionMapping(ActionMapping);
    }
    InputSettings->SaveConfig();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("action_name"), ActionName);
    ResultObj->SetStringField(TEXT("key"), Key);
    ResultObj->SetStringField(TEXT("input_type"), bAxisMapping ? TEXT("Axis") : TEXT("Action"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateDataTable(const TSharedPtr<FJsonObject>& Params)
{
    // Create a new UDataTable asset at 'asset_path' using the row struct at 'struct_path'.
    // e.g. asset_path="/Game/Config/DT_MatchConfig", struct_path="/Game/Config/ST_MatchConfig"
    FString AssetPath, StructPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("struct_path"), StructPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'struct_path' parameter"));
    }

    UScriptStruct* RowStruct = Cast<UScriptStruct>(LoadObject<UObject>(nullptr, *StructPath));
    if (!RowStruct)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Row struct not found: %s"), *StructPath));
    }

    // Previous implementation used GetTransientPackage(), which returned success but
    // created an unsaved, unfindable table. Create the asset through AssetTools so it
    // appears under Content Browser and can be loaded by GameMode after an editor restart.
    const FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
    const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
    if (PackagePath.IsEmpty() || AssetName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid asset_path: %s"), *AssetPath));
    }

    FString PathError;
    if (!FUnrealMCPCommonUtils::ValidateAssetCreationPath(AssetPath, AssetName, PathError))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(PathError);
    }

    UDataTable* NewTable = LoadObject<UDataTable>(nullptr, *FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName));
    if (!NewTable)
    {
        UDataTableFactory* Factory = NewObject<UDataTableFactory>();
        Factory->Struct = RowStruct;
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        NewTable = Cast<UDataTable>(AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, UDataTable::StaticClass(), Factory));
    }

    if (!NewTable)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create persistent UDataTable asset"));
    }

    // 既有行内存必须与原行结构一致，重复创建不能替换结构。
    if (NewTable->RowStruct != RowStruct)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Existing DataTable has a different row struct"));
    }
    NewTable->MarkPackageDirty();
    if (!UEditorAssetLibrary::SaveLoadedAsset(NewTable, false))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("DataTable created but failed to save: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetStringField(TEXT("struct_path"), StructPath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleAddRow(const TSharedPtr<FJsonObject>& Params)
{
    // Add a row to an existing DataTable.
    // 'table_path' = DataTable asset path, 'row_name' = row key, 'row_data' = JSON object of struct fields.
    FString TablePath, RowName;
    if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'table_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("row_name"), RowName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'row_name' parameter"));
    }
    const TSharedPtr<FJsonObject>* RowDataPtr = nullptr;
    if (!Params->TryGetObjectField(TEXT("row_data"), RowDataPtr) || !RowDataPtr)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'row_data' parameter"));
    }

    UDataTable* Table = LoadObject<UDataTable>(nullptr, *TablePath);
    if (!Table)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("DataTable not found: %s"), *TablePath));
    }
    if (!Table->RowStruct)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("DataTable has no row struct"));
    }
    if (FName(*RowName).IsNone())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid row_name"));
    }

    // 由作用域负责初始化、销毁结构体成员和释放内存，失败路径也不会泄漏。
    FStructOnScope StructOnScope(Table->RowStruct);
    uint8* RowData = StructOnScope.GetStructMemory();
    if (!FJsonObjectConverter::JsonObjectToUStruct(RowDataPtr->ToSharedRef(), Table->RowStruct, RowData, 0, 0))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to convert row_data JSON to struct"));
    }
    Table->AddRow(FName(*RowName), *reinterpret_cast<FTableRowBase*>(RowData));
    Table->MarkPackageDirty();
    // UEditorAssetLibrary::SaveLoadedAsset proved unreliable for this asset in
    // UE4.27 (returned false with no detail). Save the package directly instead.
    UPackage* TablePackage = Table->GetOutermost();
    const FString TableFilename = FPackageName::LongPackageNameToFilename(TablePackage->GetName(), FPackageName::GetAssetPackageExtension());
    const bool bTableSaved = UPackage::SavePackage(TablePackage, Table, RF_Public | RF_Standalone,
        *TableFilename, GError, nullptr, false, true, SAVE_NoError);
    if (!bTableSaved)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Row added but failed to save DataTable: %s"), *TablePath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("table_path"), TablePath);
    ResultObj->SetStringField(TEXT("row_name"), RowName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleReadRow(const TSharedPtr<FJsonObject>& Params)
{
    // Read a row from a DataTable and return it as JSON.
    FString TablePath, RowName;
    if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'table_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("row_name"), RowName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'row_name' parameter"));
    }

    UDataTable* Table = LoadObject<UDataTable>(nullptr, *TablePath);
    if (!Table)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("DataTable not found: %s"), *TablePath));
    }
    if (!Table->RowStruct)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("DataTable has no row struct"));
    }

    uint8* RowData = Table->FindRowUnchecked(FName(*RowName));
    if (!RowData)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Row not found: %s"), *RowName));
    }

    TSharedPtr<FJsonObject> RowJson = MakeShared<FJsonObject>();
    // UStructToJsonObject takes a TSharedRef<FJsonObject> for the output.
    if (!FJsonObjectConverter::UStructToJsonObject(Table->RowStruct, RowData, RowJson.ToSharedRef(), 0, 0))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to serialize DataTable row"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("table_path"), TablePath);
    ResultObj->SetStringField(TEXT("row_name"), RowName);
    ResultObj->SetObjectField(TEXT("row_data"), RowJson);
    return ResultObj;
} 
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleSetAssetProperty(const TSharedPtr<FJsonObject>& Params)
{
    // 参数：asset_path（完整包路径，如 /Game/.../DA_GVG）、property_name、property_value。
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }
    if (!Params->HasField(TEXT("property_value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
    }

    UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
    if (!Asset)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
    }

    const TSharedPtr<FJsonValue> JsonValue = Params->Values.FindRef(TEXT("property_value"));
    FString ErrorMessage;
    if (!FUnrealMCPCommonUtils::SetObjectProperty(Asset, PropertyName, JsonValue, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to set property: %s"), *ErrorMessage));
    }

    Asset->MarkPackageDirty();
    // 沿用 add_row 的直接保存方式：UEditorAssetLibrary 对此类资产不可靠。
    UPackage* AssetPackage = Asset->GetOutermost();
    const FString AssetFilename = FPackageName::LongPackageNameToFilename(AssetPackage->GetName(), FPackageName::GetAssetPackageExtension());
    const bool bSaved = UPackage::SavePackage(AssetPackage, Asset, RF_Public | RF_Standalone,
        *AssetFilename, GError, nullptr, false, true, SAVE_NoError);
    if (!bSaved)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Property set but failed to save asset: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetStringField(TEXT("property"), PropertyName);
    return ResultObj;
}
