#include "Commands/UnrealMCPCommonUtils.h"
#include "UObject/NoExportTypes.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Selection.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabase.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"

// JSON Utilities
TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::CreateErrorResponse(const FString& Message)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), false);
    ResponseObject->SetStringField(TEXT("error"), Message);
    return ResponseObject;
}

TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), true);
    
    if (Data.IsValid())
    {
        ResponseObject->SetObjectField(TEXT("data"), Data);
    }
    
    return ResponseObject;
}

void FUnrealMCPCommonUtils::GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((int32)Value->AsNumber());
        }
    }
}

void FUnrealMCPCommonUtils::GetFloatArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((float)Value->AsNumber());
        }
    }
}

FVector2D FUnrealMCPCommonUtils::GetVector2DFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector2D Result(0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 2)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
    }
    
    return Result;
}

FVector FUnrealMCPCommonUtils::GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
        Result.Z = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

FRotator FUnrealMCPCommonUtils::GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FRotator Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.Pitch = (float)(*JsonArray)[0]->AsNumber();
        Result.Yaw = (float)(*JsonArray)[1]->AsNumber();
        Result.Roll = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

bool FUnrealMCPCommonUtils::ValidateAssetCreationPath(const FString& PackagePath, const FString& AssetName, FString& OutError)
{
    // UE4 的 IsValidLongPackageName 不拒绝双斜杠，但 CreatePackage 会触发 Fatal。
    const bool bValidPathShape = PackagePath.Len() < NAME_SIZE && !PackagePath.Contains(TEXT("//"));
    const bool bValidPackage = bValidPathShape && PackagePath.StartsWith(TEXT("/Game/")) && FPackageName::IsValidLongPackageName(PackagePath);
    const bool bValidLength = !AssetName.IsEmpty() && AssetName.Len() < NAME_SIZE;
    const bool bValidName = bValidLength && !AssetName.Equals(TEXT("None"), ESearchCase::IgnoreCase) && FName::IsValidXName(AssetName, INVALID_OBJECTNAME_CHARACTERS);
    const bool bValidObject = bValidName && FPackageName::IsValidObjectPath(PackagePath + TEXT(".") + AssetName);
    if (!bValidPackage || !bValidObject)
    {
        OutError = FString::Printf(TEXT("Invalid asset package or name: %s.%s"), *PackagePath, *AssetName);
        return false;
    }
    return true;
}

// Blueprint Utilities
UBlueprint* FUnrealMCPCommonUtils::FindBlueprint(const FString& BlueprintName)
{
    return FindBlueprintByName(BlueprintName);
}

UBlueprint* FUnrealMCPCommonUtils::FindBlueprintByName(const FString& BlueprintName)
{
    // 传入完整路径时直接加载；否则依次尝试旧默认路径和项目常用目录。
    if (BlueprintName.StartsWith(TEXT("/Game/")))
    {
        return LoadObject<UBlueprint>(nullptr, *BlueprintName);
    }

    // 短名回退：先查通用蓝图目录，再通过资产注册表按名全局查找，
    // 这样无论蓝图放在哪个目录都能被后续命令找到（与 UMG 的解析策略一致）。
    if (UBlueprint* Found = LoadObject<UBlueprint>(nullptr, *(TEXT("/Game/Blueprints/") + BlueprintName)))
    {
        return Found;
    }
    const FAssetRegistryModule& RegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> Matches;
    RegistryModule.Get().GetAssetsByPath(FName(TEXT("/Game")), Matches, true);
    TArray<FString> FoundPaths;
    for (const FAssetData& Asset : Matches)
    {
        if (Asset.AssetName.ToString() != BlueprintName)
        {
            continue;
        }
        const FString ObjectPath = Asset.ObjectPath.ToString();
        if (Cast<UBlueprint>(Asset.GetAsset()))
        {
            FoundPaths.AddUnique(ObjectPath);
        }
    }
    // 同名蓝图存在多份时必须拒绝，避免静默选错对象。
    if (FoundPaths.Num() == 1)
    {
        return LoadObject<UBlueprint>(nullptr, *FoundPaths[0]);
    }
    if (FoundPaths.Num() > 1)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCP: blueprint name '%s' is ambiguous; pass a full /Game path. Candidates: %s"),
            *BlueprintName, *FString::Join(FoundPaths, TEXT(", ")));
    }
    return nullptr;
}

UEdGraph* FUnrealMCPCommonUtils::FindOrCreateEventGraph(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Try to find the event graph
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName().Contains(TEXT("EventGraph")))
        {
            return Graph;
        }
    }
    
    // Create a new event graph if none exists
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(TEXT("EventGraph")), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, NewGraph);
    return NewGraph;
}

// Blueprint node utilities
UK2Node_Event* FUnrealMCPCommonUtils::CreateEventNode(UEdGraph* Graph, const FString& EventName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Check for existing event node with this exact name
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Using existing event node with name %s (ID: %s)"), 
                *EventName, *EventNode->NodeGuid.ToString());
            return EventNode;
        }
    }

    // No existing node found, create a new one
    UK2Node_Event* EventNode = nullptr;
    
    // Find the function to create the event
    UClass* BlueprintClass = Blueprint->GeneratedClass;
    if (!BlueprintClass)
    {
        BlueprintClass = Blueprint->ParentClass;
    }
    if (!BlueprintClass)
    {
        return nullptr;
    }
    UFunction* EventFunction = BlueprintClass->FindFunctionByName(FName(*EventName));
    
    if (EventFunction)
    {
        EventNode = NewObject<UK2Node_Event>(Graph);
        // 覆写父类 BlueprintImplementableEvent 时必须设置 bOverrideFunction，
        // 否则只是普通自定义事件，无法接收 C++ 的事件调用。
        EventNode->EventReference.SetExternalMember(FName(*EventName), BlueprintClass);
        EventNode->bOverrideFunction = true;
        EventNode->NodePosX = Position.X;
        EventNode->NodePosY = Position.Y;
        Graph->AddNode(EventNode, true);
        EventNode->CreateNewGuid();
        EventNode->PostPlacedNewNode();
        EventNode->AllocateDefaultPins();
        EventNode->ReconstructNode();
        UE_LOG(LogTemp, Display, TEXT("Created new event node with name %s (ID: %s)"), 
            *EventName, *EventNode->NodeGuid.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find function for event name: %s"), *EventName);
    }
    
    return EventNode;
}

UK2Node_CallFunction* FUnrealMCPCommonUtils::CreateFunctionCallNode(UEdGraph* Graph, UFunction* Function, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    if (!Function)
    {
        return nullptr;
    }
    
    UK2Node_CallFunction* FunctionNode = NewObject<UK2Node_CallFunction>(Graph);
    FunctionNode->SetFromFunction(Function);
    FunctionNode->NodePosX = Position.X;
    FunctionNode->NodePosY = Position.Y;
    Graph->AddNode(FunctionNode, true);
    FunctionNode->CreateNewGuid();
    FunctionNode->PostPlacedNewNode();
    FunctionNode->AllocateDefaultPins();
    
    return FunctionNode;
}

UK2Node_VariableGet* FUnrealMCPCommonUtils::CreateVariableGetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    if (!Blueprint)
    {
        return nullptr;
    }
    if (!Blueprint->GeneratedClass)
    {
        return nullptr;
    }
    
    UK2Node_VariableGet* VariableGetNode = NewObject<UK2Node_VariableGet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableGetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableGetNode->NodePosX = Position.X;
        VariableGetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableGetNode, true);
        VariableGetNode->PostPlacedNewNode();
        VariableGetNode->AllocateDefaultPins();
        
        return VariableGetNode;
    }
    
    return nullptr;
}

UK2Node_VariableSet* FUnrealMCPCommonUtils::CreateVariableSetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    if (!Blueprint)
    {
        return nullptr;
    }
    if (!Blueprint->GeneratedClass)
    {
        return nullptr;
    }
    
    UK2Node_VariableSet* VariableSetNode = NewObject<UK2Node_VariableSet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableSetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableSetNode->NodePosX = Position.X;
        VariableSetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableSetNode, true);
        VariableSetNode->PostPlacedNewNode();
        VariableSetNode->AllocateDefaultPins();
        
        return VariableSetNode;
    }
    
    return nullptr;
}

UK2Node_InputAction* FUnrealMCPCommonUtils::CreateInputActionNode(UEdGraph* Graph, const FString& ActionName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_InputAction* InputActionNode = NewObject<UK2Node_InputAction>(Graph);
    InputActionNode->InputActionName = FName(*ActionName);
    InputActionNode->NodePosX = Position.X;
    InputActionNode->NodePosY = Position.Y;
    Graph->AddNode(InputActionNode, true);
    InputActionNode->CreateNewGuid();
    InputActionNode->PostPlacedNewNode();
    InputActionNode->AllocateDefaultPins();
    
    return InputActionNode;
}

UK2Node_Self* FUnrealMCPCommonUtils::CreateSelfReferenceNode(UEdGraph* Graph, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(Graph);
    SelfNode->NodePosX = Position.X;
    SelfNode->NodePosY = Position.Y;
    Graph->AddNode(SelfNode, true);
    SelfNode->CreateNewGuid();
    SelfNode->PostPlacedNewNode();
    SelfNode->AllocateDefaultPins();
    
    return SelfNode;
}

bool FUnrealMCPCommonUtils::ConnectGraphNodes(UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName, 
                                           UEdGraphNode* TargetNode, const FString& TargetPinName)
{
    if (!Graph)
    {
        return false;
    }
    if (!SourceNode)
    {
        return false;
    }
    if (!TargetNode)
    {
        return false;
    }
    if (SourceNode->GetGraph() != Graph || TargetNode->GetGraph() != Graph)
    {
        return false;
    }
    
    UEdGraphPin* SourcePin = FindPin(SourceNode, SourcePinName, EGPD_Output);
    UEdGraphPin* TargetPin = FindPin(TargetNode, TargetPinName, EGPD_Input);
    
    if (SourcePin && TargetPin)
    {
        const UEdGraphSchema* Schema = Graph->GetSchema();
        if (!Schema)
        {
            return false;
        }
        return Schema->TryCreateConnection(SourcePin, TargetPin);
    }
    
    return false;
}

UEdGraphPin* FUnrealMCPCommonUtils::FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return nullptr;
    }
    
    // Log all pins for debugging
    UE_LOG(LogTemp, Display, TEXT("FindPin: Looking for pin '%s' (Direction: %d) in node '%s'"), 
           *PinName, (int32)Direction, *Node->GetName());
    
    for (UEdGraphPin* Pin : Node->Pins)
    {
        UE_LOG(LogTemp, Display, TEXT("  - Available pin: '%s', Direction: %d, Category: %s"), 
               *Pin->PinName.ToString(), (int32)Pin->Direction, *Pin->PinType.PinCategory.ToString());
    }
    
    // First try exact match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString() == PinName && (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Display, TEXT("  - Found exact matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If no exact match and we're looking for a component reference, try case-insensitive match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) && 
            (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Display, TEXT("  - Found case-insensitive matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If we're looking for a component output and didn't find it by name, try to find the first data output pin
    if (Direction == EGPD_Output && Cast<UK2Node_VariableGet>(Node) != nullptr)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                UE_LOG(LogTemp, Display, TEXT("  - Found fallback data output pin: '%s'"), *Pin->PinName.ToString());
                return Pin;
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("  - No matching pin found for '%s'"), *PinName);
    return nullptr;
}

// Actor utilities
TSharedPtr<FJsonValue> FUnrealMCPCommonUtils::ActorToJson(AActor* Actor)
{
    if (!Actor)
    {
        return MakeShared<FJsonValueNull>();
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    
    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);
    
    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);
    
    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);
    
    return MakeShared<FJsonValueObject>(ActorObject);
}

TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::ActorToJsonObject(AActor* Actor, bool bDetailed)
{
    if (!Actor)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);

    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);

    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);

    // When bDetailed is true, also enumerate EditAnywhere/BlueprintReadWrite UPROPERTY
    // fields via reflection so the AI can read actor state (Team, OwningTeam, CaptureProgress,
    // MatchDuration, DefaultGameMode, etc.) — not just the transform.
    if (bDetailed)
    {
        TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
        UClass* ActorClass = Actor->GetClass();
        for (TFieldIterator<FProperty> It(ActorClass); It; ++It)
        {
            FProperty* Prop = *It;
            if (!Prop->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
            {
                continue;
            }

            const FString PropName = Prop->GetName();
            const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Actor);

            // Try common property types. Unsupported types are silently skipped.
            // UE 4.27 uses F*Property (FByteProperty, FInt32Property, etc.).
            if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
            {
                PropsObj->SetNumberField(PropName, (int32)ByteProp->GetPropertyValue(ValuePtr));
            }
            else if (FIntProperty* Int32Prop = CastField<FIntProperty>(Prop))
            {
                PropsObj->SetNumberField(PropName, Int32Prop->GetPropertyValue(ValuePtr));
            }
            else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
            {
                PropsObj->SetNumberField(PropName, FloatProp->GetPropertyValue(ValuePtr));
            }
            else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
            {
                PropsObj->SetBoolField(PropName, BoolProp->GetPropertyValue(ValuePtr));
            }
            else if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
            {
                PropsObj->SetStringField(PropName, StrProp->GetPropertyValue(ValuePtr));
            }
            else if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
            {
                PropsObj->SetStringField(PropName, NameProp->GetPropertyValue(ValuePtr).ToString());
            }
            else if (FClassProperty* ClassProp = CastField<FClassProperty>(Prop))
            {
                UClass* ClassVal = Cast<UClass>(ClassProp->GetPropertyValue(ValuePtr));
                PropsObj->SetStringField(PropName, ClassVal ? ClassVal->GetPathName() : TEXT("None"));
            }
            else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
            {
                UObject* ObjVal = ObjProp->GetPropertyValue(ValuePtr);
                PropsObj->SetStringField(PropName, ObjVal ? ObjVal->GetPathName() : TEXT("None"));
            }
            // Vector/Struct/Array properties are skipped for brevity — AI can use
            // the existing location/rotation/scale fields above for spatial state.
        }
        ActorObject->SetObjectField(TEXT("properties"), PropsObj);
    }

    return ActorObject;
}

UK2Node_Event* FUnrealMCPCommonUtils::FindExistingEventNode(UEdGraph* Graph, const FString& EventName)
{
    if (!Graph)
    {
        return nullptr;
    }

    // Look for existing event nodes
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Found existing event node with name: %s"), *EventName);
            return EventNode;
        }
    }

    return nullptr;
}

bool FUnrealMCPCommonUtils::SetObjectProperty(UObject* Object, const FString& PropertyName, 
                                     const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    if (!Object)
    {
        OutErrorMessage = TEXT("Invalid object");
        return false;
    }

    if (!Value.IsValid())
    {
        OutErrorMessage = TEXT("Invalid JSON property value");
        return false;
    }
    FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        OutErrorMessage = FString::Printf(TEXT("Property not found: %s"), *PropertyName);
        return false;
    }

    const bool bWrongBool = Property->IsA<FBoolProperty>() && Value->Type != EJson::Boolean;
    const bool bWrongString = Property->IsA<FStrProperty>() && Value->Type != EJson::String;
    const bool bNumericProperty = Property->IsA<FIntProperty>() || Property->IsA<FFloatProperty>();
    const bool bWrongNumber = bNumericProperty && Value->Type != EJson::Number;
    if (bWrongBool || bWrongString || bWrongNumber)
    {
        OutErrorMessage = FString::Printf(TEXT("JSON type does not match property %s"), *PropertyName);
        return false;
    }
    void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);
    
    // Handle different property types
    if (Property->IsA<FBoolProperty>())
    {
        ((FBoolProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsBool());
        return true;
    }
    else if (Property->IsA<FIntProperty>())
    {
        int32 IntValue = static_cast<int32>(Value->AsNumber());
        FIntProperty* IntProperty = CastField<FIntProperty>(Property);
        if (IntProperty)
        {
            IntProperty->SetPropertyValue_InContainer(Object, IntValue);
            return true;
        }
    }
    else if (Property->IsA<FFloatProperty>())
    {
        ((FFloatProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsNumber());
        return true;
    }
    else if (Property->IsA<FStrProperty>())
    {
        ((FStrProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsString());
        return true;
    }
    else if (Property->IsA<FByteProperty>())
    {
        FByteProperty* ByteProp = CastField<FByteProperty>(Property);
        UEnum* EnumDef = ByteProp ? ByteProp->GetIntPropertyEnum() : nullptr;
        
        // If this is a TEnumAsByte property (has associated enum)
        if (EnumDef)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
                ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %d"), 
                      *PropertyName, ByteValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    uint8 ByteValue = FCString::Atoi(*EnumValueName);
                    ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %d"), 
                          *PropertyName, *EnumValueName, ByteValue);
                    return true;
                }
                
                // Handle qualified enum names (e.g., "Player0" or "EAutoReceiveInput::Player0")
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(EnumValue));
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
        else
        {
            // Regular byte property
            uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
            ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
            return true;
        }
    }
    else if (Property->IsA<FEnumProperty>())
    {
        FEnumProperty* EnumProp = CastField<FEnumProperty>(Property);
        UEnum* EnumDef = EnumProp ? EnumProp->GetEnum() : nullptr;
        FNumericProperty* UnderlyingNumericProp = EnumProp ? EnumProp->GetUnderlyingProperty() : nullptr;
        
        if (EnumDef && UnderlyingNumericProp)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                int64 EnumValue = static_cast<int64>(Value->AsNumber());
                UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %lld"), 
                      *PropertyName, EnumValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    int64 EnumValue = FCString::Atoi64(*EnumValueName);
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                
                // Handle qualified enum names
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
    }
    // FClassProperty (TSubclassOf<T> / raw UClass* references, e.g. WorldSettings.GameModeOverride,
    // GameMode's TSubclassOf<APawn> DefaultPawnClass, etc). Must be checked before any FObjectProperty
    // branch, since FClassProperty derives from FObjectProperty and would otherwise be misclassified.
    else if (Property->IsA<FClassProperty>())
    {
        FClassProperty* ClassProp = CastField<FClassProperty>(Property);
        if (ClassProp)
        {
            if (Value->Type != EJson::String)
            {
                OutErrorMessage = FString::Printf(TEXT("Expected a string class path for property %s"), *PropertyName);
                return false;
            }

            FString ClassPath = Value->AsString();
            if (ClassPath.IsEmpty())
            {
                // Allow clearing the class reference.
                ClassProp->SetObjectPropertyValue_InContainer(Object, nullptr);
                return true;
            }

            UClass* MetaClass = ClassProp->MetaClass ? ClassProp->MetaClass : UObject::StaticClass();
            UClass* ClassValue = LoadClass<UObject>(nullptr, *ClassPath);
            if (!ClassValue)
            {
                OutErrorMessage = FString::Printf(TEXT("Failed to load class '%s' for property %s"), *ClassPath, *PropertyName);
                return false;
            }

            if (!ClassValue->IsChildOf(MetaClass))
            {
                OutErrorMessage = FString::Printf(TEXT("Class '%s' is not a valid %s for property %s"),
                                                *ClassPath, *MetaClass->GetName(), *PropertyName);
                return false;
            }

            ClassProp->SetObjectPropertyValue_InContainer(Object, ClassValue);
            UE_LOG(LogTemp, Display, TEXT("Setting class property %s to: %s"), *PropertyName, *ClassPath);
            return true;
        }
    }
    else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        // 支持常用结构体属性的 JSON 数组或字符串赋值；未支持的结构体仍报错。
        const UScriptStruct* StructDef = StructProp->Struct;
        if (!StructDef)
        {
            OutErrorMessage = TEXT("Struct property has no struct definition");
            return false;
        }
        const FString StructName = StructDef->GetName();

        TArray<double> Numbers;
        if (Value->Type == EJson::Array)
        {
            // 4.26 的 AsArray() 返回 const TArray&，不能解引用指针。
            const TArray<TSharedPtr<FJsonValue>>& Array = Value->AsArray();
            for (const TSharedPtr<FJsonValue>& Element : Array)
            {
                if (!Element.IsValid() || Element->Type != EJson::Number)
                {
                    OutErrorMessage = TEXT("Struct array must contain only numbers");
                    return false;
                }
                Numbers.Add(Element->AsNumber());
            }
        }
        else if (Value->Type == EJson::String)
        {
            // 优先使用 UE 原生解析器，兼容 X=1 Y=2 等格式。
            const FString Text = Value->AsString();
            if (StructDef == TBaseStructure<FVector2D>::Get())
            {
                FVector2D Parsed;
                if (Parsed.InitFromString(Text))
                {
                    *static_cast<FVector2D*>(PropertyAddr) = Parsed;
                    return true;
                }
            }
            if (StructDef == TBaseStructure<FVector>::Get())
            {
                FVector Parsed;
                if (Parsed.InitFromString(Text))
                {
                    *static_cast<FVector*>(PropertyAddr) = Parsed;
                    return true;
                }
            }
            if (StructDef == TBaseStructure<FRotator>::Get())
            {
                FRotator Parsed;
                if (Parsed.InitFromString(Text))
                {
                    *static_cast<FRotator*>(PropertyAddr) = Parsed;
                    return true;
                }
            }
            // 兼容纯数字逗号列表，但非法数值不能静默变成零。
            TArray<FString> Parts;
            Text.ParseIntoArray(Parts, TEXT(","), true);
            for (const FString& Part : Parts)
            {
                FString NumPart = Part;
                if (Part.Contains(TEXT("=")))
                {
                    FString Key;
                    Part.Split(TEXT("="), &Key, &NumPart);
                }
                NumPart.TrimStartInline();
                NumPart.TrimEndInline();
                if (!NumPart.IsNumeric())
                {
                    OutErrorMessage = TEXT("Invalid struct numeric value");
                    return false;
                }
                Numbers.Add(FCString::Atod(*NumPart));
            }
        }

        if (StructDef == TBaseStructure<FVector2D>::Get())
        {
            if (Numbers.Num() < 2)
            {
                OutErrorMessage = FString::Printf(TEXT("Vector2D property %s needs 2 numbers"), *PropertyName);
                return false;
            }
            *static_cast<FVector2D*>(PropertyAddr) = FVector2D(Numbers[0], Numbers[1]);
            UE_LOG(LogTemp, Display, TEXT("Setting Vector2D property %s to (%f, %f)"), *PropertyName, Numbers[0], Numbers[1]);
            return true;
        }
        if (StructDef == TBaseStructure<FVector>::Get())
        {
            if (Numbers.Num() < 3)
            {
                OutErrorMessage = FString::Printf(TEXT("Vector property %s needs 3 numbers"), *PropertyName);
                return false;
            }
            *static_cast<FVector*>(PropertyAddr) = FVector(Numbers[0], Numbers[1], Numbers[2]);
            UE_LOG(LogTemp, Display, TEXT("Setting Vector property %s to (%f, %f, %f)"), *PropertyName, Numbers[0], Numbers[1], Numbers[2]);
            return true;
        }
        if (StructDef == TBaseStructure<FRotator>::Get())
        {
            if (Numbers.Num() < 3)
            {
                OutErrorMessage = FString::Printf(TEXT("Rotator property %s needs 3 numbers"), *PropertyName);
                return false;
            }
            *static_cast<FRotator*>(PropertyAddr) = FRotator(Numbers[0], Numbers[1], Numbers[2]);
            UE_LOG(LogTemp, Display, TEXT("Setting Rotator property %s to (P%f, Y%f, R%f)"), *PropertyName, Numbers[0], Numbers[1], Numbers[2]);
            return true;
        }
        // FIntPoint（如 WidgetComponent 的 DrawSize）：与 FVector2D 同形态的整数坐标。
        // FIntPoint 无 TBaseStructure 特化（仅 IMPLEMENT_STRUCT 注册），按结构体名匹配。
        if (StructName == TEXT("IntPoint"))
        {
            if (Numbers.Num() < 2)
            {
                OutErrorMessage = FString::Printf(TEXT("IntPoint property %s needs 2 numbers"), *PropertyName);
                return false;
            }
            *static_cast<FIntPoint*>(PropertyAddr) = FIntPoint(
                static_cast<int32>(Numbers[0]), static_cast<int32>(Numbers[1]));
            UE_LOG(LogTemp, Display, TEXT("Setting IntPoint property %s to (%d, %d)"),
                *PropertyName, static_cast<int32>(Numbers[0]), static_cast<int32>(Numbers[1]));
            return true;
        }

        OutErrorMessage = FString::Printf(TEXT("Unsupported struct type: %s for property %s"), *StructName, *PropertyName);
        return false;
    }
    else if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
    {
        // TSet 属性：接受 JSON 数组；元素支持字符串（FName）或数字（枚举整值）。
        if (Value->Type != EJson::Array)
        {
            OutErrorMessage = FString::Printf(TEXT("Set property %s needs a JSON array"), *PropertyName);
            return false;
        }
        FScriptSetHelper SetHelper(SetProp, PropertyAddr);
        SetHelper.EmptyElements();
        const TArray<TSharedPtr<FJsonValue>>& Elements = Value->AsArray();
        for (const TSharedPtr<FJsonValue>& Element : Elements)
        {
            if (!Element.IsValid())
            {
                OutErrorMessage = TEXT("Set array contains invalid element");
                return false;
            }
            // 逐元素写入再入集合；不支持元素类型时给出明确错误。
            if (CastField<FNameProperty>(SetHelper.GetElementProperty()))
            {
                if (Element->Type != EJson::String)
                {
                    OutErrorMessage = TEXT("FName set elements must be strings");
                    return false;
                }
                const FName Entry(*Element->AsString());
                SetHelper.AddElement(&Entry);
            }
            else if (CastField<FIntProperty>(SetHelper.GetElementProperty()))
            {
                if (Element->Type != EJson::Number)
                {
                    OutErrorMessage = TEXT("Integer set elements must be numbers");
                    return false;
                }
                const int32 Entry = static_cast<int32>(Element->AsNumber());
                SetHelper.AddElement(&Entry);
            }
            else if (CastField<FStrProperty>(SetHelper.GetElementProperty()))
            {
                if (Element->Type != EJson::String)
                {
                    OutErrorMessage = TEXT("String set elements must be strings");
                    return false;
                }
                const FString Entry = Element->AsString();
                SetHelper.AddElement(&Entry);
            }
            else
            {
                OutErrorMessage = FString::Printf(TEXT("Unsupported set element type: %s for property %s"),
                    *SetHelper.GetElementProperty()->GetClass()->GetName(), *PropertyName);
                return false;
            }
        }
        SetHelper.Rehash();
        UE_LOG(LogTemp, Display, TEXT("Setting set property %s with %d elements"), *PropertyName, Elements.Num());
        return true;
    }

    OutErrorMessage = FString::Printf(TEXT("Unsupported property type: %s for property %s"),
                                    *Property->GetClass()->GetName(), *PropertyName);
    return false;
}