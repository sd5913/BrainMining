#include "MiningDeveloperMapComponent.h"
#include "MiningTunnelGenerator.h"
#include "GeneratedMiningRock.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "CanvasItem.h"
#include "GlobalRenderResources.h"
#include "InputCoreTypes.h"
#include "UnrealClient.h"

void UMiningDeveloperMapComponent::ResetView()
{
	if (!bVisible) return;
	EndMapControl();
	View = EMiningMapView::Orbit3D; OrbitRotation = FRotator(-30, -45, 0);
	PanOffset = FVector2D::ZeroVector; Zoom = 1; bFollowPlayer = false;
}

void UMiningDeveloperMapComponent::OrbitBy(float YawDegrees, float PitchDegrees)
{
	if (!bVisible || View != EMiningMapView::Orbit3D || !FMath::IsFinite(YawDegrees) || !FMath::IsFinite(PitchDegrees)) return;
	OrbitRotation.Yaw = FRotator::NormalizeAxis(OrbitRotation.Yaw + YawDegrees);
	OrbitRotation.Pitch = FMath::Clamp(OrbitRotation.Pitch + PitchDegrees, -85.0, 85.0);
}

void UMiningDeveloperMapComponent::PanBy(FVector2D Offset)
{
	if (!bVisible || View != EMiningMapView::Orbit3D || Offset.ContainsNaN()) return;
	PanOffset = (PanOffset + Offset).ClampAxes(-10000000, 10000000);
}

void UMiningDeveloperMapComponent::BeginMapControl()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (bControllingMap || !bVisible || View != EMiningMapView::Orbit3D || (IsValid(Layout) && !Layout->DeveloperMapSettings.bEnabled)) return;
	AHUD* HUD = Cast<AHUD>(GetOwner());
	if (!HUD || !IsValid(HUD->PlayerOwner) || !HUD->PlayerOwner->IsLocalController()) return;
	ControlController = HUD->PlayerOwner;
	ControlController->SetIgnoreLookInput(true);
	bControllingMap = true;
#endif
}

void UMiningDeveloperMapComponent::EndMapControl()
{
	if (bControllingMap && ControlController.IsValid()) ControlController->SetIgnoreLookInput(false);
	ControlController.Reset(); bControllingMap = false;
}

void UMiningDeveloperMapComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndMapControl();
	Super::EndPlay(EndPlayReason);
}

void UMiningDeveloperMapComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	AHUD* HUD = Cast<AHUD>(GetOwner());
	APlayerController* Controller = HUD ? HUD->PlayerOwner.Get() : nullptr;
	UGameViewportClient* GameViewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!IsValid(Controller) || !GameViewport || !GameViewport->Viewport || !GameViewport->Viewport->HasFocus()
		|| !bVisible || View != EMiningMapView::Orbit3D || (IsValid(Layout) && !Layout->DeveloperMapSettings.bEnabled)
		|| (!Controller->IsInputKeyDown(EKeys::LeftAlt) && !Controller->IsInputKeyDown(EKeys::RightAlt)))
	{
		EndMapControl(); return;
	}
	BeginMapControl();
	if (!bControllingMap) return;
	float MouseX = 0, MouseY = 0;
	Controller->GetInputMouseDelta(MouseX, MouseY);
	if (Controller->IsInputKeyDown(EKeys::LeftShift) || Controller->IsInputKeyDown(EKeys::RightShift))
		PanBy(FVector2D(-MouseX, MouseY) / FMath::Max(0.0001, LastScale));
	else OrbitBy(-MouseX * 0.4f, -MouseY * 0.4f);
	if (Controller->WasInputKeyJustPressed(EKeys::MouseScrollUp)) ZoomIn();
	if (Controller->WasInputKeyJustPressed(EKeys::MouseScrollDown)) ZoomOut();
#endif
}

FVector2D UMiningDeveloperMapComponent::ProjectPoint(const FVector& Point) const
{
	return View == EMiningMapView::Orbit3D ? FVector2D(MiningDeveloperMap::Project3D(Point, OrbitRotation)) : MiningDeveloperMap::Project(Point, View);
}

FLinearColor UMiningDeveloperMapComponent::RegionColor(const FMiningMapRegion& Region) const
{
	FLinearColor Color = Region.Kind == EMiningMapRegionKind::Room ? FLinearColor(0.2f, 0.85f, 0.58f)
		: Region.Kind == EMiningMapRegionKind::Branch && IsValid(Layout) && Layout->bHollowRegionPrototype ? FLinearColor(0.1f, 0.9f, 0.88f)
		: Region.Kind == EMiningMapRegionKind::GuidePlug ? FLinearColor(0.76f, 0.3f, 1)
		: Region.Kind == EMiningMapRegionKind::RockRegion ? FLinearColor(0.58f, 0.34f, 0.15f)
		: Region.Kind == EMiningMapRegionKind::Tunnel && IsValid(Layout) && Layout->bHollowRegionPrototype ? FLinearColor(0.3f, 0.7f, 0.9f)
		: Region.Kind == EMiningMapRegionKind::HiddenEntrance ? FLinearColor(1, 0.28f, 0.7f)
		: Region.Kind == EMiningMapRegionKind::Tunnel || Region.bRockFilled ? FLinearColor(0.9f, 0.58f, 0.2f) : FLinearColor(0.22f, 0.63f, 1);
	if (IsValid(Layout) && Region.Kind != EMiningMapRegionKind::Room && Layout->GeneratedSegments.IsValidIndex(Region.GroupIndex))
	{
		const AGeneratedMiningRock* Segment = Layout->GeneratedSegments[Region.GroupIndex];
		if (IsValid(Segment) && Segment->SegmentState == EMiningSegmentState::Recycled) Color = FLinearColor(0.32f, 0.35f, 0.4f);
		else if (IsValid(Segment) && Segment->SegmentState == EMiningSegmentState::Frozen) Color = FMath::Lerp(Color, FLinearColor(0.4f, 0.43f, 0.45f), 0.6f);
	}
	return Color;
}

void UMiningDeveloperMapComponent::Refresh3DProjection()
{
	const FVector ViewDirection = OrbitRotation.Vector();
	const FVector LightDirection = FRotationMatrix(OrbitRotation).TransformVector(FVector(-1, -0.4, 0.8).GetSafeNormal());
	FBox2D AllBounds(ForceInit);
	for (int32 Index = 0; Index < SurfaceRegions.Num(); ++Index)
	{
		const MiningDeveloperMap::FSurface& Surface = SurfaceRegions[Index];
		TArray<FVector> Projected;
		for (const FVector& Vertex : Surface.Vertices)
		{
			const FVector Point = MiningDeveloperMap::Project3D(Vertex, OrbitRotation);
			Projected.Add(Point);
			if (Point.ContainsNaN()) continue;
			AllBounds += FVector2D(Point);
			if (Regions[Index].Kind != EMiningMapRegionKind::RockRegion) ProjectedBounds += FVector2D(Point);
		}
		if (Regions[Index].Kind != EMiningMapRegionKind::RockRegion)
		{
			for (const FIntVector& Triangle : Surface.Triangles)
			{
				FProjectedFace Face;
				Face.First = FVector2D(Projected[Triangle.X]); Face.Second = FVector2D(Projected[Triangle.Y]); Face.Third = FVector2D(Projected[Triangle.Z]);
				Face.Depth = (Projected[Triangle.X].Z + Projected[Triangle.Y].Z + Projected[Triangle.Z].Z) / 3;
				Face.Region = Index;
				const FVector Normal = FVector::CrossProduct(Surface.Vertices[Triangle.Y] - Surface.Vertices[Triangle.X], Surface.Vertices[Triangle.Z] - Surface.Vertices[Triangle.X]).GetSafeNormal();
				Face.Shade = 0.45f + 0.45f * FMath::Abs(FVector::DotProduct(Normal, LightDirection));
				if (FMath::Abs(FVector2D::CrossProduct(Face.Second - Face.First, Face.Third - Face.First)) > 0.001) ProjectedFaces.Add(Face);
			}
		}
		for (const MiningDeveloperMap::FSurfaceEdge& Edge : Surface.Edges)
		{
			if (!Edge.Visible(ViewDirection)) continue;
			FProjectedEdge Result;
			Result.First = FVector2D(Projected[Edge.Vertices.X]); Result.Second = FVector2D(Projected[Edge.Vertices.Y]); Result.Region = Index;
			ProjectedEdges.Add(Result);
		}
	}
	if (!ProjectedBounds.bIsValid) ProjectedBounds = AllBounds;
	ProjectedFaces.StableSort([](const FProjectedFace& First, const FProjectedFace& Second) { return First.Depth > Second.Depth; });
	DisplayTriangleCount = ProjectedFaces.Num();
}

void UMiningDeveloperMapComponent::Draw3D(UCanvas* Canvas, const FBox2D& Graph, const FVector2D& Center, double Scale, float UiScale)
{
	const auto Screen = [&Graph, &Center, Scale](const FVector2D& Point) { return Graph.GetCenter() + (Point - Center) * Scale; };
	TArray<FLinearColor> Colors;
	for (const FMiningMapRegion& Region : Regions) Colors.Add(RegionColor(Region));
	TArray<FCanvasUVTri> Triangles;
	Triangles.Reserve(ProjectedFaces.Num());
	const auto Append = [&Triangles](const FVector2D& First, const FVector2D& Second, const FVector2D& Third, const FLinearColor& Color)
	{
		FCanvasUVTri Triangle;
		Triangle.V0_Pos = First; Triangle.V1_Pos = Second; Triangle.V2_Pos = Third;
		Triangle.V0_Color = Color; Triangle.V1_Color = Color; Triangle.V2_Color = Color;
		Triangles.Add(Triangle);
	};
	for (const FProjectedFace& Face : ProjectedFaces)
	{
		const FVector2D First = Screen(Face.First), Second = Screen(Face.Second), Third = Screen(Face.Third);
		FBox2D Bounds(ForceInit); Bounds += First; Bounds += Second; Bounds += Third;
		if (!Bounds.Intersect(Graph)) continue;
		FLinearColor Color = Colors[Face.Region] * (Face.Region == CurrentRegionIndex ? FMath::Min(1.2f, Face.Shade + 0.3f) : Face.Shade);
		Color.A = 0.9f;
		if (Graph.IsInsideOrOn(First) && Graph.IsInsideOrOn(Second) && Graph.IsInsideOrOn(Third)) Append(First, Second, Third, Color);
		else
		{
			const TArray<FVector2D> Clipped = MiningDeveloperMap::ClipPolygon({First, Second, Third}, Graph);
			for (int32 Index = 1; Index + 1 < Clipped.Num(); ++Index) Append(Clipped[0], Clipped[Index], Clipped[Index + 1], Color);
		}
	}
	if (!Triangles.IsEmpty())
	{
		FCanvasTriangleItem Item(Triangles, GWhiteTexture);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}
	for (const FProjectedEdge& Edge : ProjectedEdges)
	{
		FVector2D First = Screen(Edge.First), Second = Screen(Edge.Second);
		if (!MiningDeveloperMap::ClipLine(First, Second, Graph)) continue;
		FLinearColor Color = Colors[Edge.Region];
		Color.A = Regions[Edge.Region].Kind == EMiningMapRegionKind::RockRegion ? 0.12f : Edge.Region == CurrentRegionIndex ? 1.f : 0.35f;
		Canvas->K2_DrawLine(First, Second, Edge.Region == CurrentRegionIndex ? 2 * UiScale : UiScale, Color);
	}
	const FVector2D Origin = Graph.Min + FVector2D(34, 36) * UiScale;
	const FVector Axes[] = {FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector};
	const FLinearColor AxisColors[] = {FLinearColor(1, 0.35f, 0.3f), FLinearColor(0.35f, 1, 0.45f), FLinearColor(0.3f, 0.65f, 1)};
	const TCHAR* AxisNames[] = {TEXT("X"), TEXT("Y"), TEXT("Z")};
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const FVector2D Endpoint = Origin + ProjectPoint(Axes[Axis]) * 22 * UiScale;
		Canvas->K2_DrawLine(Origin, Endpoint, 2 * UiScale, AxisColors[Axis]);
		FCanvasTextItem Label(Endpoint + FVector2D(2, -5) * UiScale, FText::FromString(AxisNames[Axis]), GEngine->GetSmallFont(), AxisColors[Axis]);
		Label.Scale = FVector2D(UiScale); Canvas->DrawItem(Label);
	}
}
