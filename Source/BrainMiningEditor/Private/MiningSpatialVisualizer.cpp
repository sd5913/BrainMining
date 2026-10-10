#include "MiningSpatialVisualizer.h"
#include "MiningSpatialDirectEditing.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "PrimitiveDrawingUtils.h"
#include "SceneManagement.h"
#include "ScopedTransaction.h"
#include "Engine/Selection.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "LevelEditorViewport.h"
#include "ComponentVisualizerManager.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"

TWeakObjectPtr<AMiningSpatialTunnel> FMiningSpatialSelection::Tunnel;
int32 FMiningSpatialSelection::Knot = 0;
int32 FMiningSpatialSelection::Part = 0;

bool FMiningSpatialSelection::Editable()
{
	return GEditor && !GEditor->PlayWorld && Tunnel.IsValid() && Tunnel->Knots.IsValidIndex(Knot);
}

void FMiningSpatialSelection::Select(AMiningSpatialTunnel* Value, int32 Index, int32 Handle, FEditorViewportClient* Client)
{
	Tunnel = Value; Knot = Value ? FMath::Clamp(Index, 0, Value->Knots.Num() - 1) : 0;
	Part = FMath::Clamp(Handle, 0, 2);
	Activate(Client);
	if (GEditor) GEditor->RedrawLevelEditingViewports();
}

void FMiningSpatialSelection::Activate(FEditorViewportClient* Client)
{
	if (!GUnrealEd || !GEditor || GEditor->PlayWorld) return;
	TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(UMiningSpatialPathComponent::StaticClass());
	if (!Visualizer.IsValid()) return;
	if (MiningSpatialDirectEditing::IsEnabled())
	{
		if (GUnrealEd->ComponentVisManager.GetActiveComponentVis() == Visualizer) GUnrealEd->ComponentVisManager.ClearActiveComponentVis();
		return;
	}
	if (!Editable() || Tunnel->Kind == EMiningManualKind::SideRoom)
	{
		if (GUnrealEd->ComponentVisManager.GetActiveComponentVis() == Visualizer) GUnrealEd->ComponentVisManager.ClearActiveComponentVis();
		return;
	}
	if (!Client) Client = GCurrentLevelEditingViewportClient;
	if (!Client || Client->GetWorld() != Tunnel->GetWorld()) return;
	if (GUnrealEd->ComponentVisManager.SetActiveComponentVis(Client, Visualizer)) StaticCastSharedPtr<FMiningSpatialVisualizer>(Visualizer)->Edit(Tunnel.Get(), Part);
}

bool FMiningSpatialSelection::Insert()
{
	if (!Editable() || Tunnel->Kind == EMiningManualKind::SideRoom) return false;
	const int32 Segment = FMath::Min(Knot, Tunnel->Knots.Num() - 2);
	if (Tunnel->Knots[Segment].bLocked || Tunnel->Knots[Segment + 1].bLocked) return false;
	FScopedTransaction Transaction(FText::FromString(TEXT("插入三维曲线节点"))); Tunnel->Modify();
	TArray<FMiningSpatialKnot> Resolved; FString Error;
	if (!Tunnel->ResolveKnots(Resolved, Error)) return false;
	for (FMiningSpatialKnot& Item : Resolved)
	{
		Item.Position = Tunnel->GetActorTransform().InverseTransformPosition(Item.Position);
		Item.Arrive = Tunnel->GetActorTransform().InverseTransformVectorNoScale(Item.Arrive);
		Item.Leave = Tunnel->GetActorTransform().InverseTransformVectorNoScale(Item.Leave);
	}
	if (!MiningSpatial::Insert(Resolved, Segment, 0.5)) return false;
	const int32 Count = Resolved.Num();
	Tunnel->Knots = MoveTemp(Resolved); Tunnel->Changed();
	if (Tunnel->Knots.Num() != Count) { Transaction.Cancel(); return false; }
	Select(Tunnel.Get(), Segment + 1); return true;
}

void FMiningSpatialSelection::Delete()
{
	if (!Editable() || Tunnel->Knots.Num() <= 2 || Tunnel->Knots[Knot].bLocked || (Knot == 0 && Tunnel->StartPortal.Room) || (Knot == Tunnel->Knots.Num() - 1 && Tunnel->EndPortal.Room)) return;
	FScopedTransaction Transaction(FText::FromString(TEXT("删除三维曲线节点"))); Tunnel->Modify();
	const int32 OriginalCount = Tunnel->Knots.Num(), OriginalKnot = Knot;
	Tunnel->Knots.RemoveAt(Knot); Knot = FMath::Min(Knot, Tunnel->Knots.Num() - 1); Tunnel->Changed();
	if (Tunnel->Knots.Num() == OriginalCount) { Knot = OriginalKnot; Transaction.Cancel(); }
	else Select(Tunnel.Get(), Knot);
}

struct HMiningSpatialPoint : HComponentVisProxy
{
	DECLARE_HIT_PROXY();
	int32 Knot, Handle;
	HMiningSpatialPoint(const UActorComponent* Component, int32 Index, int32 InHandle) : HComponentVisProxy(Component, HPP_Foreground), Knot(Index), Handle(InHandle) {}
};
IMPLEMENT_HIT_PROXY(HMiningSpatialPoint, HComponentVisProxy);

FMiningSpatialVisualizer::~FMiningSpatialVisualizer() = default;

void FMiningSpatialVisualizer::Edit(AMiningSpatialTunnel* Source, int32 SelectedHandle)
{
	if (Edited != Source || Handle != SelectedHandle) Transaction.Reset();
	Edited = Source; Handle = SelectedHandle;
}

static void EditableKnots(const AMiningSpatialTunnel& Source, TArray<FMiningSpatialKnot>& Knots)
{
	FString Error;
	if (Source.ResolveKnots(Knots, Error)) return;
	Knots = Source.Knots;
	for (FMiningSpatialKnot& Knot : Knots)
	{
		Knot.Position = Source.GetActorTransform().TransformPosition(Knot.Position);
		Knot.Arrive = Source.GetActorTransform().TransformVectorNoScale(Knot.Arrive);
		Knot.Leave = Source.GetActorTransform().TransformVectorNoScale(Knot.Leave);
	}
}

void FMiningSpatialVisualizer::Refresh(AMiningSpatialTunnel* Source) const
{
	const uint32 Hash = HashCombineFast(Source->GetGeometryHash(), GetTypeHash(IsValid(Source->Region) ? Source->Region->CurveTolerance : 8.f));
	if (CachedSource == Source && Hash == CachedHash) return;
	CachedSource = Source; CachedHash = Hash; CachedRings.Reset();
	TArray<FMiningSpatialKnot> Knots; FString Error;
	if (Source->ResolveKnots(Knots, Error) && !MiningSpatial::Sample(Knots, Source->GetActorTransform().TransformVectorNoScale(Source->InitialUp), IsValid(Source->Region) ? Source->Region->CurveTolerance : 8, CachedRings, Error, nullptr, Source->Region && Source->Region->bAutomaticBedrock)) CachedRings.Reset();
}

bool FMiningSpatialVisualizer::Ring(MiningSpatial::FRing& Result) const
{
	if (!Edited.IsValid()) return false;
	Refresh(Edited.Get());
	for (const MiningSpatial::FRing& Item : CachedRings)
		if (Item.Knot == FMiningSpatialSelection::Knot) { Result = Item; return true; }
	return false;
}

void FMiningSpatialVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	AMiningSpatialTunnel* Source = Component ? Cast<AMiningSpatialTunnel>(Component->GetOwner()) : nullptr;
	if (!Source || (GEditor && GEditor->PlayWorld) || MiningSpatialDirectEditing::IsEnabled()) return;
	const FLinearColor Cyan(0.15f, 0.8f, 1), Orange(1, 0.5f, 0.05f), Green(0.15f, 1, 0.45f);
	if (Source->Kind == EMiningManualKind::SideRoom)
	{
		DrawWireBox(PDI, Source->GetActorTransform().ToMatrixNoScale(), FBox(-Source->RoomSize * 0.5, Source->RoomSize * 0.5), Green, SDPG_Foreground, 1.5f, 0, true);
		return;
	}
	Refresh(Source);
	for (int32 Index = 0; Index < CachedRings.Num(); ++Index)
	{
		const MiningSpatial::FRing& Item = CachedRings[Index];
		const bool bSelected = Source == FMiningSpatialSelection::Tunnel && Item.Knot == FMiningSpatialSelection::Knot;
		if (Item.Knot != INDEX_NONE || Index % 6 == 0)
			for (int32 Vertex = 0; Vertex < Item.Profile.Num(); ++Vertex)
			{
				const FVector2D First = Item.Profile[Vertex], Last = Item.Profile[(Vertex + 1) % Item.Profile.Num()];
				PDI->DrawLine(Item.Center + Item.Right * First.X + Item.Up * First.Y, Item.Center + Item.Right * Last.X + Item.Up * Last.Y, bSelected ? Orange : Cyan, SDPG_Foreground, bSelected ? 2.5f : 1, 0, true);
			}
		if (Index)
		{
			const MiningSpatial::FRing& Previous = CachedRings[Index - 1];
			PDI->DrawLine(Previous.Center, Item.Center, Green, SDPG_Foreground, 2, 0, true);
			for (int32 Vertex = 0; Vertex < Item.Profile.Num(); Vertex += FMath::Max(1, Item.Profile.Num() / 4))
				PDI->DrawLine(Previous.Center + Previous.Right * Previous.Profile[Vertex].X + Previous.Up * Previous.Profile[Vertex].Y, Item.Center + Item.Right * Item.Profile[Vertex].X + Item.Up * Item.Profile[Vertex].Y, Cyan, SDPG_Foreground, 1, 0, true);
		}
	}
	TArray<FMiningSpatialKnot> Resolved; EditableKnots(*Source, Resolved);
	for (int32 Index = 0; Index < Resolved.Num(); ++Index)
	{
		const FMiningSpatialKnot& Item = Resolved[Index];
		const bool bSelected = Source == FMiningSpatialSelection::Tunnel && Index == FMiningSpatialSelection::Knot;
		if (CachedRings.IsEmpty() && Index)
			for (int32 Step = 0; Step < 24; ++Step) PDI->DrawLine(MiningSpatial::Evaluate(Resolved[Index - 1], Item, Step / 24.0), MiningSpatial::Evaluate(Resolved[Index - 1], Item, (Step + 1) / 24.0), FLinearColor::Red, SDPG_Foreground, 2, 0, true);
		PDI->SetHitProxy(new HMiningSpatialPoint(Component, Index, 0));
		PDI->DrawPoint(Item.Position, Item.bLocked ? FLinearColor::Gray : bSelected ? Orange : FLinearColor::White, 15, SDPG_Foreground);
		PDI->SetHitProxy(nullptr);
		if (bSelected)
			for (int32 Part = 1; Part <= 2; ++Part)
			{
				if ((Index == 0 && Part == 1) || (Index == Resolved.Num() - 1 && Part == 2)) continue;
				const FVector Position = Item.Position + (Part == 1 ? Item.Arrive : Item.Leave);
				PDI->DrawLine(Item.Position, Position, Orange, SDPG_Foreground, 1.5f, 0, true);
				PDI->SetHitProxy(new HMiningSpatialPoint(Component, Index, Part)); PDI->DrawPoint(Position, Orange, 12, SDPG_Foreground); PDI->SetHitProxy(nullptr);
			}
	}
}

void FMiningSpatialVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	const AMiningSpatialTunnel* Source = Component ? Cast<AMiningSpatialTunnel>(Component->GetOwner()) : nullptr;
	if (!Source || Source != FMiningSpatialSelection::Tunnel || Source->Kind == EMiningManualKind::SideRoom || !GEngine || (GEditor && GEditor->PlayWorld) || MiningSpatialDirectEditing::IsEnabled()) return;
	TArray<FMiningSpatialKnot> Resolved; EditableKnots(*Source, Resolved);
	for (int32 Index = 0; Index < Resolved.Num(); ++Index)
	{
		FVector2D Screen;
		if (Resolved[Index].Position.ContainsNaN() || !View->WorldToPixel(Resolved[Index].Position, Screen) || !View->UnscaledViewRect.Contains(FIntPoint(Screen.X, Screen.Y))) continue;
		const bool bSelected = Index == FMiningSpatialSelection::Knot;
		const FString Name = bSelected ? FString::Printf(TEXT("节点 %d · 当前"), Index + 1) : FString::FromInt(Index + 1);
		FCanvasTextItem Label(Screen / Canvas->GetDPIScale() + FVector2D(10, -18), FText::FromString(Name), GEngine->GetSmallFont(), bSelected ? FLinearColor(1, 0.65f, 0.1f) : FLinearColor::White);
		Label.EnableShadow(FLinearColor::Black); Canvas->DrawItem(Label);
	}
}

bool FMiningSpatialVisualizer::VisProxyHandleClick(FEditorViewportClient* Client, HComponentVisProxy* Proxy, const FViewportClick& Click)
{
	if (!Proxy || !Proxy->IsA(HMiningSpatialPoint::StaticGetType()) || !GEditor || GEditor->PlayWorld) return false;
	HMiningSpatialPoint* Point = static_cast<HMiningSpatialPoint*>(Proxy);
	AMiningSpatialTunnel* Source = Point->Component.IsValid() ? Cast<AMiningSpatialTunnel>(Point->Component->GetOwner()) : nullptr;
	if (!Source) return false;
	FMiningSpatialSelection::Select(Source, Point->Knot, Point->Handle, Client); return true;
}

bool FMiningSpatialVisualizer::GetWidgetLocation(const FEditorViewportClient* Client, FVector& Location) const
{
	if (!FMiningSpatialSelection::Editable() || Edited != FMiningSpatialSelection::Tunnel) return false;
	TArray<FMiningSpatialKnot> Resolved; EditableKnots(*Edited, Resolved);
	if (!Resolved.IsValidIndex(FMiningSpatialSelection::Knot)) return false;
	const FMiningSpatialKnot& Item = Resolved[FMiningSpatialSelection::Knot];
	Location = Item.Position + (Handle == 1 ? Item.Arrive : Handle == 2 ? Item.Leave : FVector::ZeroVector); return true;
}

bool FMiningSpatialVisualizer::GetCustomInputCoordinateSystem(const FEditorViewportClient* Client, FMatrix& Matrix) const
{
	MiningSpatial::FRing Item;
	if (Client->GetWidgetMode() == UE::Widget::WM_Translate || !Ring(Item)) return false;
	Matrix = FMatrix::Identity; Matrix.SetAxes(&Item.Tangent, &Item.Right, &Item.Up); return true;
}

void FMiningSpatialVisualizer::TrackingStarted(FEditorViewportClient* Client)
{
	if (FMiningSpatialSelection::Editable() && Edited == FMiningSpatialSelection::Tunnel)
	{
		Transaction = MakeUnique<FScopedTransaction>(FText::FromString(TEXT("编辑三维隧道节点/截面")));
		Edited->Modify();
	}
}

void FMiningSpatialVisualizer::TrackingStopped(FEditorViewportClient* Client, bool bDidMove)
{
	if (Edited.IsValid()) Edited->Changed(); Transaction.Reset();
}

bool FMiningSpatialVisualizer::HandleInputDelta(FEditorViewportClient* Client, FViewport* Viewport, FVector& Translate, FRotator& Rotate, FVector& Scale)
{
	if (!FMiningSpatialSelection::Editable() || Edited != FMiningSpatialSelection::Tunnel) return false;
	FMiningSpatialKnot& Item = Edited->Knots[FMiningSpatialSelection::Knot];
	if (Item.bLocked) return true;
	if (!Transaction) TrackingStarted(Client);
	const int32 Index = FMiningSpatialSelection::Knot;
	const bool bStart = Index == 0 && Edited->StartPortal.Room, bEnd = Index == Edited->Knots.Num() - 1 && Edited->EndPortal.Room;
	if (!Translate.IsNearlyZero())
	{
		const FVector LocalDelta = Edited->GetActorTransform().InverseTransformVectorNoScale(Translate);
		if (Handle == 0 && !bStart && !bEnd) Item.Position += LocalDelta;
		else if (Handle > 0)
		{
			FVector& TangentHandle = Handle == 1 ? Item.Arrive : Item.Leave;
			if ((bStart && Handle == 2) || (bEnd && Handle == 1))
			{
				TArray<FMiningSpatialKnot> Resolved; FString Error;
				if (Edited->ResolveKnots(Resolved, Error))
				{
					const FVector Direction = (Handle == 1 ? Resolved[Index].Arrive : Resolved[Index].Leave).GetSafeNormal();
					TangentHandle = Edited->GetActorTransform().InverseTransformVectorNoScale(Direction * FMath::Max(50.0, TangentHandle.Length() + FVector::DotProduct(Translate, Direction)));
				}
			}
			else TangentHandle += LocalDelta;
		}
	}
	MiningSpatial::FRing Frame;
	if (Handle == 0 && Ring(Frame))
	{
		if (!Rotate.IsNearlyZero()) Item.Roll += FMath::RadiansToDegrees(FVector::DotProduct(Rotate.Quaternion().ToRotationVector(), Frame.Tangent));
		if (!Scale.IsNearlyZero()) Item.SectionScale = (Item.SectionScale * FVector2D(FMath::Max(0.05, 1 + Scale.Y), FMath::Max(0.05, 1 + Scale.Z))).ClampAxes(0.05, 20);
	}
	Edited->Changed(); return true;
}

bool FMiningSpatialVisualizer::HandleInputKey(FEditorViewportClient* Client, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (Event != IE_Pressed || !FMiningSpatialSelection::Editable() || Edited != FMiningSpatialSelection::Tunnel) return false;
	if (Key == EKeys::Insert) { FMiningSpatialSelection::Insert(); return true; }
	if (Key == EKeys::Delete) { FMiningSpatialSelection::Delete(); return true; }
	return false;
}

UActorComponent* FMiningSpatialVisualizer::GetEditedComponent() const { return Edited.IsValid() ? Edited->Path.Get() : nullptr; }
void FMiningSpatialVisualizer::EndEditing() { Transaction.Reset(); Edited.Reset(); }
