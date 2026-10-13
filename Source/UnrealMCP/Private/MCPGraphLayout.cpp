#include "MCPGraphLayout.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "NodeFactory.h"
#include "SGraphNode.h"
#include "Framework/Application/SlateApplication.h"
#include "EdGraphNode_Comment.h"

namespace MCPGraphLayout
{
    namespace
    {
        const int32 Grid = 16;
        const float Gap = 64.0f;

        int32 Snap(float Value)
        {
            return FMath::CeilToInt(Value / Grid) * Grid;
        }

        bool Overlaps(const FVector2D& Position, const FVector2D& Size, UEdGraphNode* Other)
        {
            bool bEstimated = false;
            const FVector2D OtherSize = Measure(Other, bEstimated);
            return Position.X < Other->NodePosX + OtherSize.X + Gap && Position.X + Size.X + Gap > Other->NodePosX
                && Position.Y < Other->NodePosY + OtherSize.Y + Gap && Position.Y + Size.Y + Gap > Other->NodePosY;
        }

        bool HasExecution(UEdGraphNode* Node)
        {
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                {
                    return true;
                }
            }
            return false;
        }

        TArray<UEdGraphNode*> Neighbours(UEdGraphNode* Node, EEdGraphPinDirection Direction, bool bExecution)
        {
            TArray<UEdGraphNode*> Result;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin->Direction != Direction)
                {
                    continue;
                }
                if ((Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) != bExecution)
                {
                    continue;
                }
                for (UEdGraphPin* Linked : Pin->LinkedTo)
                {
                    Result.AddUnique(Linked->GetOwningNode());
                }
            }
            Result.Sort([](const UEdGraphNode& A, const UEdGraphNode& B) { return A.NodeGuid < B.NodeGuid; });
            return Result;
        }
    }

    FVector2D Measure(UEdGraphNode* Node, bool& bEstimated)
    {
        bEstimated = true;
        if (FSlateApplication::IsInitialized())
        {
            TSharedPtr<SGraphNode> Widget = FNodeFactory::CreateNodeWidget(Node);
            if (Widget.IsValid())
            {
                Widget->SlatePrepass(1.0f);
                const FVector2D Size = Widget->GetDesiredSize();
                if (Size.X > 0.0f && Size.Y > 0.0f)
                {
                    bEstimated = false;
                    return Size;
                }
            }
        }
        // 无可用 Slate 尺寸时保守估算，并在命令结果中逐个标注
        return FVector2D(320.0f, FMath::Max(128.0f, 64.0f + Node->Pins.Num() * 24.0f));
    }

    void PlaceNewNode(UEdGraphNode* Node, const TSharedPtr<FJsonObject>& Params)
    {
        if (Params->HasField(TEXT("node_position")))
        {
            return;
        }
        bool bEstimated = false;
        const FVector2D Size = Measure(Node, bEstimated);
        FVector2D Position(0.0f, 0.0f);
        UEdGraph* Graph = Node->GetGraph();
        bool bMoved = true;
        while (bMoved)
        {
            bMoved = false;
            for (UEdGraphNode* Other : Graph->Nodes)
            {
                if (Other == Node)
                {
                    continue;
                }
                if (Other->IsA<UEdGraphNode_Comment>())
                {
                    continue;
                }
                if (Overlaps(Position, Size, Other))
                {
                    bool bOtherEstimated = false;
                    Position.Y = Snap(Other->NodePosY + Measure(Other, bOtherEstimated).Y + Gap);
                    bMoved = true;
                }
            }
        }
        Node->NodePosX = Snap(Position.X);
        Node->NodePosY = Snap(Position.Y);
    }

    TSharedPtr<FJsonObject> Describe(UEdGraph* Graph)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> Nodes;
        TArray<TSharedPtr<FJsonValue>> Connections;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            bool bEstimated = false;
            const FVector2D Size = Measure(Node, bEstimated);
            TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
            Item->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
            Item->SetStringField(TEXT("class"), Node->GetClass()->GetPathName());
            Item->SetNumberField(TEXT("x"), Node->NodePosX);
            Item->SetNumberField(TEXT("y"), Node->NodePosY);
            Item->SetNumberField(TEXT("width"), Size.X);
            Item->SetNumberField(TEXT("height"), Size.Y);
            Item->SetStringField(TEXT("size_source"), bEstimated ? TEXT("estimate") : TEXT("slate_desired_size"));
            Nodes.Add(MakeShared<FJsonValueObject>(Item));
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin->Direction != EGPD_Output)
                {
                    continue;
                }
                for (UEdGraphPin* Linked : Pin->LinkedTo)
                {
                    TSharedPtr<FJsonObject> Edge = MakeShared<FJsonObject>();
                    Edge->SetStringField(TEXT("source_node_id"), Node->NodeGuid.ToString());
                    Edge->SetStringField(TEXT("source_pin"), Pin->PinName.ToString());
                    Edge->SetStringField(TEXT("target_node_id"), Linked->GetOwningNode()->NodeGuid.ToString());
                    Edge->SetStringField(TEXT("target_pin"), Linked->PinName.ToString());
                    Edge->SetBoolField(TEXT("execution"), Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
                    Connections.Add(MakeShared<FJsonValueObject>(Edge));
                }
            }
        }
        Result->SetArrayField(TEXT("nodes"), Nodes);
        Result->SetArrayField(TEXT("connections"), Connections);
        return Result;
    }

    TSharedPtr<FJsonObject> Format(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
    {
        bool bWholeGraph = false;
        Params->TryGetBoolField(TEXT("whole_graph"), bWholeGraph);
        const TArray<TSharedPtr<FJsonValue>>* Ids = nullptr;
        const bool bHasIds = Params->TryGetArrayField(TEXT("node_ids"), Ids);
        if (bWholeGraph == bHasIds)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Choose node_ids or whole_graph=true"));
        }
        TSet<UEdGraphNode*> Selected;
        if (bHasIds)
        {
            for (const TSharedPtr<FJsonValue>& Id : *Ids)
            {
                FString Value;
                if (!Id->TryGetString(Value))
                {
                    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("node_ids must contain strings"));
                }
                UEdGraphNode* Found = nullptr;
                for (UEdGraphNode* Node : Graph->Nodes)
                {
                    if (Node->NodeGuid.ToString() == Value)
                    {
                        Found = Node;
                        break;
                    }
                }
                if (!Found)
                {
                    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Unknown node_id in selection"));
                }
                if (Found->IsA<UEdGraphNode_Comment>())
                {
                    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Comment nodes cannot be selected for automatic layout"));
                }
                Selected.Add(Found);
            }
        }
        else
        {
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (!Node->IsA<UEdGraphNode_Comment>())
                {
                    Selected.Add(Node);
                }
            }
        }
        TArray<UEdGraphNode*> Ordered = Selected.Array();
        Ordered.Sort([](const UEdGraphNode& A, const UEdGraphNode& B) { return A.NodeGuid < B.NodeGuid; });
        TMap<UEdGraphNode*, FVector2D> Sizes;
        for (UEdGraphNode* Node : Ordered)
        {
            bool bEstimated = false;
            Sizes.Add(Node, Measure(Node, bEstimated));
        }
        // 先按执行流遍历，同链水平排列；已有节点仅作为固定障碍
        TMap<UEdGraphNode*, FVector2D> Positions;
        TSet<UEdGraphNode*> Visiting;
        TArray<FString> Warnings;
        float NextRow = 0.0f;
        TFunction<void(UEdGraphNode*, float, float)> Visit;
        Visit = [&](UEdGraphNode* Node, float X, float Y)
        {
            if (!Selected.Contains(Node))
            {
                return;
            }
            if (Visiting.Contains(Node))
            {
                Warnings.AddUnique(TEXT("Execution cycle retained; back edges may cross"));
                return;
            }
            if (Positions.Contains(Node))
            {
                Warnings.AddUnique(TEXT("Shared execution node retained; converging wires may cross"));
                return;
            }
            FVector2D Position(Snap(X), Snap(Y));
            bool bCollision = true;
            while (bCollision)
            {
                bCollision = false;
                for (UEdGraphNode* Other : Graph->Nodes)
                {
                    if (Selected.Contains(Other) || Other->IsA<UEdGraphNode_Comment>())
                    {
                        continue;
                    }
                    if (Overlaps(Position, Sizes[Node], Other))
                    {
                        bool bEstimated = false;
                        Position.Y = Snap(Other->NodePosY + Measure(Other, bEstimated).Y + Gap);
                        bCollision = true;
                    }
                }
                for (const auto& Pair : Positions)
                {
                    const FVector2D& P = Pair.Value;
                    const FVector2D& S = Sizes[Pair.Key];
                    const bool bHorizontal = Position.X < P.X + S.X + Gap && Position.X + Sizes[Node].X + Gap > P.X;
                    const bool bVertical = Position.Y < P.Y + S.Y + Gap && Position.Y + Sizes[Node].Y + Gap > P.Y;
                    if (bHorizontal && bVertical)
                    {
                        Position.Y = Snap(P.Y + S.Y + Gap);
                        bCollision = true;
                    }
                }
            }
            Positions.Add(Node, Position);
            NextRow = FMath::Max(NextRow, Position.Y + Sizes[Node].Y + Gap);
            Visiting.Add(Node);
            int32 Branch = 0;
            for (UEdGraphNode* Child : Neighbours(Node, EGPD_Output, true))
            {
                Visit(Child, Position.X + Sizes[Node].X + Gap, Branch++ == 0 ? Position.Y : NextRow);
            }
            Visiting.Remove(Node);
        };
        for (UEdGraphNode* Node : Ordered)
        {
            if (!HasExecution(Node))
            {
                continue;
            }
            bool bSelectedParent = false;
            float X = 0.0f;
            float Y = NextRow;
            for (UEdGraphNode* Parent : Neighbours(Node, EGPD_Input, true))
            {
                if (Selected.Contains(Parent))
                {
                    bSelectedParent = true;
                }
                else
                {
                    bool bEstimated = false;
                    X = FMath::Max(X, Parent->NodePosX + Measure(Parent, bEstimated).X + Gap);
                    Y = FMath::Max(Y, static_cast<float>(Parent->NodePosY));
                }
            }
            if (!bSelectedParent)
            {
                Visit(Node, X, Y);
            }
        }
        // 没有根的循环仍按稳定 GUID 顺序布局，保证遍历终止
        for (UEdGraphNode* Node : Ordered)
        {
            if (HasExecution(Node))
            {
                if (!Positions.Contains(Node))
                {
                    Visit(Node, 0.0f, NextRow);
                }
            }
        }
        for (UEdGraphNode* Node : Ordered)
        {
            if (Positions.Contains(Node))
            {
                continue;
            }
            float X = 0.0f;
            float Y = NextRow;
            for (UEdGraphNode* Consumer : Neighbours(Node, EGPD_Output, false))
            {
                const FVector2D* P = Positions.Find(Consumer);
                if (P)
                {
                    X = FMath::Max(0.0f, P->X - Sizes[Node].X - Gap);
                    Y = P->Y + Sizes[Consumer].Y + Gap;
                    break;
                }
            }
            Visit(Node, X, Y);
        }
        Graph->Modify();
        for (const auto& Pair : Positions)
        {
            Pair.Key->Modify();
            Pair.Key->NodePosX = Snap(Pair.Value.X);
            Pair.Key->NodePosY = Snap(Pair.Value.Y);
        }
        Graph->NotifyGraphChanged();
        TSharedPtr<FJsonObject> Result = Describe(Graph);
        TArray<TSharedPtr<FJsonValue>> Messages;
        for (const FString& Warning : Warnings)
        {
            Messages.Add(MakeShared<FJsonValueString>(Warning));
        }
        Result->SetArrayField(TEXT("warnings"), Messages);
        Result->SetNumberField(TEXT("formatted_count"), Positions.Num());
        return Result;
    }
}
