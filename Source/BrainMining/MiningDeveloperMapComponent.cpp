#include "MiningDeveloperMapComponent.h"
#include "MiningTunnelGenerator.h"
#include "MiningTunnelRoute.h"
#include "MiningRoutePlanner.h"
#include "GeneratedMiningRock.h"
#include "Components/InputComponent.h"
#include "CanvasItem.h"
#include "GlobalRenderResources.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

UMiningDeveloperMapComponent::UMiningDeveloperMapComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	bVisible = false;
#else
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
#endif
}

void UMiningDeveloperMapComponent::BindInput(UInputComponent* Input)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (!Input) return;
	const auto Bind = [this, Input](FKey Key, void (UMiningDeveloperMapComponent::*Callback)())
	{
		FInputKeyBinding& Binding = Input->BindKey(Key, IE_Pressed, this, Callback);
		Binding.bConsumeInput = false;
		Binding.bExecuteWhenPaused = true;
	};
	Bind(EKeys::M, &UMiningDeveloperMapComponent::ToggleMap);
	Bind(EKeys::Tab, &UMiningDeveloperMapComponent::ToggleExpanded);
	Bind(EKeys::V, &UMiningDeveloperMapComponent::CycleView);
	Bind(EKeys::Home, &UMiningDeveloperMapComponent::ToggleFollow);
	Bind(EKeys::L, &UMiningDeveloperMapComponent::ToggleLabels);
	Bind(EKeys::Equals, &UMiningDeveloperMapComponent::ZoomIn);
	Bind(EKeys::Add, &UMiningDeveloperMapComponent::ZoomIn);
	Bind(EKeys::Hyphen, &UMiningDeveloperMapComponent::ZoomOut);
	Bind(EKeys::Subtract, &UMiningDeveloperMapComponent::ZoomOut);
	Bind(EKeys::BackSpace, &UMiningDeveloperMapComponent::ResetView);
	Bind(EKeys::LeftAlt, &UMiningDeveloperMapComponent::BeginMapControl);
	Bind(EKeys::RightAlt, &UMiningDeveloperMapComponent::BeginMapControl);
	Input->BindKey(EKeys::LeftAlt, IE_Released, this, &UMiningDeveloperMapComponent::EndMapControl).bExecuteWhenPaused = true;
	Input->BindKey(EKeys::RightAlt, IE_Released, this, &UMiningDeveloperMapComponent::EndMapControl).bExecuteWhenPaused = true;
	SetComponentTickEnabled(true);
#endif
}

void UMiningDeveloperMapComponent::ToggleMap()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (!IsValid(Layout) || Layout->DeveloperMapSettings.bEnabled) bVisible = !bVisible;
	if (!bVisible) EndMapControl();
#endif
}

void UMiningDeveloperMapComponent::ToggleExpanded() { if (bVisible) bExpanded = !bExpanded; }
void UMiningDeveloperMapComponent::CycleView() { if (bVisible) { EndMapControl(); View = static_cast<EMiningMapView>((static_cast<uint8>(View) + 1) % 4); Zoom = 1; PanOffset = FVector2D::ZeroVector; } }
void UMiningDeveloperMapComponent::ToggleFollow() { if (bVisible) { bFollowPlayer = !bFollowPlayer; Zoom = 1; PanOffset = FVector2D::ZeroVector; } }
void UMiningDeveloperMapComponent::ToggleLabels() { if (bVisible) bLabels = !bLabels; }
void UMiningDeveloperMapComponent::ZoomIn() { if (bVisible) Zoom = FMath::Clamp(Zoom * 1.5f, 0.25f, 16.f); }
void UMiningDeveloperMapComponent::ZoomOut() { if (bVisible) Zoom = FMath::Clamp(Zoom / 1.5f, 0.25f, 16.f); }

void UMiningDeveloperMapComponent::RefreshLayout(const FVector& PlayerLocation)
{
	AMiningTunnelGenerator* Selected = IsValid(Layout) ? Layout.Get() : nullptr;
	if (!Selected || GetWorld()->GetRealTimeSeconds() >= NextLayoutSearch)
	{
		NextLayoutSearch = GetWorld()->GetRealTimeSeconds() + 1;
		double Nearest = TNumericLimits<double>::Max();
		Selected = nullptr;
		for (TActorIterator<AMiningTunnelGenerator> Candidate(GetWorld()); Candidate; ++Candidate)
		{
			if (Candidate->GeneratedMapRegions.IsEmpty() && Candidate->GeneratedRoomBounds.IsEmpty()) continue;
			double Distance = TNumericLimits<double>::Max();
			for (const FMiningMapRegion& Region : Candidate->GeneratedMapRegions)
			{
				if (Region.Contains(PlayerLocation)) { Distance = 0; break; }
				Distance = FMath::Min(Distance, Region.Bounds.ComputeSquaredDistanceToPoint(PlayerLocation));
			}
			if (Candidate->GeneratedMapRegions.IsEmpty())
			{
				for (const FBox& Bounds : Candidate->GeneratedRoomBounds)
					Distance = FMath::Min(Distance, Bounds.ShiftBy(Candidate->GetActorLocation()).ComputeSquaredDistanceToPoint(PlayerLocation));
			}
			if (Distance < Nearest || (Distance == Nearest && *Candidate == Layout.Get())) { Selected = *Candidate; Nearest = Distance; }
		}
	}
	if (Selected == Layout && Selected && SnapshotRevision == Selected->BuildRevision) return;
	Layout = Selected;
	Regions.Reset();
	SurfaceRegions.Reset();
	SnapshotRevision = Selected ? Selected->BuildRevision : INDEX_NONE;
	bLegacySnapshot = Selected && Selected->GeneratedMapRegions.IsEmpty();
	if (Selected)
	{
		Regions = Selected->GeneratedMapRegions;
		if (bLegacySnapshot)
		{
			for (int32 Index = 0; Index < Selected->GeneratedRoomBounds.Num(); ++Index)
				Regions.Add(MiningDeveloperMap::Box(FString::Printf(TEXT("R%d%s"), Index, Index == 0 ? TEXT(" START") : TEXT("")), EMiningMapRegionKind::Room, Index, Selected->GeneratedRoomBounds[Index].ShiftBy(Selected->GetActorLocation())));
			for (int32 Index = 0; Index + 1 < Selected->GeneratedRoomBounds.Num() && Index < Selected->Routes.Num(); ++Index)
			{
				const AMiningTunnelRoute* Route = Selected->Routes[Index];
				if (!IsValid(Route) || !Route->bDirectConnection) continue;
				MiningRoutes::FPlan Plan;
				FString Error;
				if (!MiningRoutes::Direct(Selected->GeneratedRoomBounds[Index], Selected->GeneratedRoomBounds[Index + 1], Route->ResolvedStartWall, Route->ResolvedEndWall, Plan, Error)) continue;
				for (FVector& Vertex : Plan.FaceCorners) Vertex += Selected->GetActorLocation();
				Regions.Add(MiningDeveloperMap::Convex(FString::Printf(TEXT("Tunnel R%d > R%d"), Index, Index + 1), EMiningMapRegionKind::Tunnel, Index, Plan.FaceCorners));
			}
		}
		if (!bInitialized)
		{
			bInitialized = true;
			bVisible = Selected->DeveloperMapSettings.bShowOnStart;
			View = EMiningMapView::Orbit3D;
		}
	}
	RegionCount = Regions.Num();
	for (const FMiningMapRegion& Region : Regions) SurfaceRegions.Add(MiningDeveloperMap::BuildSurface(Region));
	RefreshProjection();
}

void UMiningDeveloperMapComponent::RefreshProjection()
{
	ProjectedRegions.Reset();
	ProjectedBounds = FBox2D(ForceInit);
	CachedView = View;
	CachedRotation = OrbitRotation;
	ProjectedFaces.Reset(); ProjectedEdges.Reset(); DisplayTriangleCount = 0;
	if (View == EMiningMapView::Orbit3D) { Refresh3DProjection(); return; }
	for (const FMiningMapRegion& Region : Regions)
	{
		TArray<FVector2D> Outline = MiningDeveloperMap::Outline(Region, View);
		for (const FVector2D& Point : Outline) ProjectedBounds += Point;
		ProjectedRegions.Add(MoveTemp(Outline));
	}
}

void UMiningDeveloperMapComponent::Draw(AHUD* HUD, UCanvas* Canvas)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (!HUD || !Canvas || !HUD->PlayerOwner || !HUD->PlayerOwner->IsLocalController() || !HUD->PlayerOwner->GetPawn() || !GEngine) return;
	if (Canvas->ClipX < 240 || Canvas->ClipY < 180) return;
	const FVector PlayerLocation = HUD->PlayerOwner->GetPawn()->GetActorLocation();
	RefreshLayout(PlayerLocation);
	if (IsValid(Layout) && !Layout->DeveloperMapSettings.bEnabled) { EndMapControl(); return; }
	const FMiningDeveloperMapSettings Settings = IsValid(Layout) ? Layout->DeveloperMapSettings : FMiningDeveloperMapSettings();
	const float UiScale = FMath::Clamp(FMath::Min(Canvas->ClipX / 960.f, Canvas->ClipY / 640.f), 0.5f, 1.4f);
	const FLinearColor Accent(0.2f, 0.95f, 0.8f);
	if (!bVisible)
	{
		HUD->DrawText(TEXT("M  Developer map"), Accent, FMath::Max(8.f, Canvas->ClipX - 160 * UiScale), Canvas->ClipY - 62 * UiScale, GEngine->GetSmallFont(), UiScale);
		return;
	}
	if (CachedView != View || (View == EMiningMapView::Orbit3D && CachedRotation != OrbitRotation)) RefreshProjection();
	const FVector2D Size(FMath::Max(100.f, FMath::Min(bExpanded ? Canvas->ClipX * 0.92f : Settings.PanelWidth * UiScale, Canvas->ClipX - 24 * UiScale)),
		FMath::Max(140.f, FMath::Min(bExpanded ? Canvas->ClipY * 0.86f : Settings.PanelHeight * UiScale, Canvas->ClipY - 64 * UiScale)));
	const FVector2D Minimum = bExpanded ? FVector2D((Canvas->ClipX - Size.X) * 0.5, (Canvas->ClipY - Size.Y) * 0.5 - 10 * UiScale)
		: FVector2D(Canvas->ClipX - Size.X - 12 * UiScale, Canvas->ClipY - Size.Y - 54 * UiScale);
	const FVector2D Maximum = Minimum + Size;
	HUD->DrawRect(FLinearColor(0.015f, 0.027f, 0.045f, FMath::Clamp(Settings.BackgroundOpacity, 0.3f, 1.f)), Minimum.X, Minimum.Y, Size.X, Size.Y);
	const auto Text = [HUD, Canvas, UiScale](FString Value, const FVector2D& Position, FLinearColor Color, double Width)
	{
		Value.ReplaceInline(TEXT("\n"), TEXT(" "));
		Value = Value.Left(100);
		const FString Original = Value;
		while (!Value.IsEmpty() && Canvas->K2_TextSize(GEngine->GetSmallFont(), Value, FVector2D(UiScale)).X > Width) Value.LeftChopInline(1);
		if (Value.Len() < Original.Len() && Value.Len() > 3) Value = Value.LeftChop(3) + TEXT("...");
		HUD->DrawText(Value, Color, Position.X, Position.Y, GEngine->GetSmallFont(), UiScale);
	};
	const auto Line = [Canvas](FVector2D Start, FVector2D Finish, const FLinearColor& Color, float Width)
	{
		Canvas->K2_DrawLine(Start, Finish, Width, Color);
	};
	const auto Fill = [Canvas](const TArray<FVector2D>& Points, const FLinearColor& Color)
	{
		TArray<FCanvasUVTri> Triangles;
		for (int32 Index = 1; Index + 1 < Points.Num(); ++Index)
		{
			FCanvasUVTri Triangle;
			Triangle.V0_Pos = Points[0]; Triangle.V1_Pos = Points[Index]; Triangle.V2_Pos = Points[Index + 1];
			Triangle.V0_Color = Color; Triangle.V1_Color = Color; Triangle.V2_Color = Color;
			Triangles.Add(Triangle);
		}
		if (!Triangles.IsEmpty())
		{
			FCanvasTriangleItem Item(Triangles, GWhiteTexture);
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
	};
	const bool bThreeD = View == EMiningMapView::Orbit3D;
	const TCHAR* ViewName = bThreeD ? TEXT("3D ORBIT") : View == EMiningMapView::SideXZ ? TEXT("SIDE X/Z") : View == EMiningMapView::TopXY ? TEXT("TOP X/Y") : TEXT("SIDE Y/Z");
	const TCHAR* HiddenAxis = View == EMiningMapView::SideXZ ? TEXT("Y") : View == EMiningMapView::TopXY ? TEXT("Z") : TEXT("X");
	const double TextWidth = Size.X - 24 * UiScale;
	Text(FString::Printf(TEXT("DEV MAP / %s / %s x%.1f"), ViewName, bFollowPlayer ? TEXT("FOLLOW") : TEXT("FIT"), Zoom), Minimum + FVector2D(12, 8) * UiScale, Accent, TextWidth);
	CurrentRegionIndex = MiningDeveloperMap::FindRegion(Regions, PlayerLocation);
	LocationLabel = Regions.IsValidIndex(CurrentRegionIndex) ? Regions[CurrentRegionIndex].Name : TEXT("Outside mapped rooms / tunnels");
	Text(TEXT("YOU: ") + LocationLabel, Minimum + FVector2D(12, 28) * UiScale, FLinearColor::White, TextWidth);
	Text(FString::Printf(TEXT("X %.1f  Y %.1f  Z %.1fm | %s"), PlayerLocation.X / 100, PlayerLocation.Y / 100, PlayerLocation.Z / 100, bThreeD ? bControllingMap ? TEXT("MAP CONTROL / release Alt to look") : TEXT("3D / hold Alt to orbit") : *FString::Printf(TEXT("all %s layers"), HiddenAxis)), Minimum + FVector2D(12, 47) * UiScale, FLinearColor(0.66f, 0.74f, 0.8f), TextWidth);
	MapMinimum = Minimum + FVector2D(12, 70) * UiScale;
	MapMaximum = Maximum - FVector2D(12, bThreeD ? 104 : 86) * UiScale;
	if (MapMaximum.X - MapMinimum.X < 20 || MapMaximum.Y - MapMinimum.Y < 20) return;
	const FBox2D Graph(MapMinimum, MapMaximum);
	HUD->DrawRect(FLinearColor(0.035f, 0.055f, 0.075f, 0.9f), Graph.Min.X, Graph.Min.Y, Graph.GetSize().X, Graph.GetSize().Y);
	const FVector2D ProjectedPlayer = ProjectPoint(PlayerLocation);
	const FVector2D Center = (bFollowPlayer || !ProjectedBounds.bIsValid ? ProjectedPlayer : ProjectedBounds.GetCenter()) + (bThreeD ? PanOffset : FVector2D::ZeroVector);
	const double Scale = (bFollowPlayer ? Graph.GetSize().X / FMath::Max(1000.f, Settings.FollowWidth)
		: MiningDeveloperMap::FitScale(ProjectedBounds, Graph.GetSize() - FVector2D(30 * UiScale))) * Zoom;
	LastScale = Scale;
	const auto Screen = [Center, Scale, &Graph](const FVector2D& Point) { return Graph.GetCenter() + (Point - Center) * Scale; };
	const double GridStep = FMath::Pow(10.0, FMath::CeilToDouble(FMath::LogX(10.0, 35 * UiScale / Scale)));
	const FVector2D VisibleMinimum = Center - Graph.GetSize() / (2 * Scale);
	const FVector2D VisibleMaximum = Center + Graph.GetSize() / (2 * Scale);
	for (int32 Axis = 0; !bThreeD && Axis < 2; ++Axis)
	{
		int32 Count = 0;
		for (double Position = FMath::CeilToDouble(VisibleMinimum[Axis] / GridStep) * GridStep; Position <= VisibleMaximum[Axis] && Count++ < 80; Position += GridStep)
		{
			FVector2D Start = VisibleMinimum;
			FVector2D Finish = VisibleMaximum;
			Start[Axis] = Position; Finish[Axis] = Position;
			Line(Screen(Start), Screen(Finish), FLinearColor(0.1f, 0.15f, 0.19f, 0.7f), 1);
		}
	}
	if (bThreeD) Draw3D(Canvas, Graph, Center, Scale, UiScale);
	const EMiningMapRegionKind DrawOrder[] = {EMiningMapRegionKind::RockRegion, EMiningMapRegionKind::Tunnel, EMiningMapRegionKind::Branch, EMiningMapRegionKind::Space, EMiningMapRegionKind::Room, EMiningMapRegionKind::HiddenEntrance, EMiningMapRegionKind::GuidePlug};
	for (EMiningMapRegionKind Kind : DrawOrder)
	{
		if (bThreeD) break;
		for (int32 Index = 0; Index < Regions.Num(); ++Index)
		{
			const FMiningMapRegion& Region = Regions[Index];
			if (Region.Kind != Kind) continue;
			TArray<FVector2D> Polygon;
			for (const FVector2D& Point : ProjectedRegions[Index]) Polygon.Add(Screen(Point));
			FLinearColor Color = RegionColor(Region);
			FLinearColor Interior = Color;
			Interior.A = Index == CurrentRegionIndex ? 0.55f : 0.22f;
			if (!Region.SurfaceTriangles.IsEmpty())
			{
				TArray<FVector2D> Surface;
				for (const FVector& Vertex : Region.Vertices) Surface.Add(Screen(MiningDeveloperMap::Project(Vertex, View)));
				Interior.A = 1; Interior *= Index == CurrentRegionIndex ? 0.85f : 0.55f; Interior.A = 1;
				for (const FIntVector& Triangle : Region.SurfaceTriangles)
					if (Surface.IsValidIndex(Triangle.X) && Surface.IsValidIndex(Triangle.Y) && Surface.IsValidIndex(Triangle.Z))
						Fill(MiningDeveloperMap::ClipPolygon({Surface[Triangle.X], Surface[Triangle.Y], Surface[Triangle.Z]}, Graph), Interior);
				continue;
			}
			if (View == EMiningMapView::SideXZ && !Region.PolygonTriangles.IsEmpty())
			{
				for (const FIntVector& Triangle : Region.PolygonTriangles)
					if (Polygon.IsValidIndex(Triangle.X) && Polygon.IsValidIndex(Triangle.Y) && Polygon.IsValidIndex(Triangle.Z))
						Fill(MiningDeveloperMap::ClipPolygon({Polygon[Triangle.X], Polygon[Triangle.Y], Polygon[Triangle.Z]}, Graph), Interior);
			}
			else Fill(MiningDeveloperMap::ClipPolygon(Polygon, Graph), Interior);
			for (int32 Corner = 0; Corner < Polygon.Num(); ++Corner)
			{
				FVector2D Start = Polygon[Corner];
				FVector2D Finish = Polygon[(Corner + 1) % Polygon.Num()];
				if (MiningDeveloperMap::ClipLine(Start, Finish, Graph)) Line(Start, Finish, Color, Index == CurrentRegionIndex ? 2.5f * UiScale : UiScale);
			}
		}
	}
	const FVector2D UnclampedPlayer = Screen(ProjectedPlayer);
	const FBox2D PlayerClip(Graph.Min + FVector2D(12 * UiScale), Graph.Max - FVector2D(12 * UiScale));
	PlayerMapPosition = FVector2D(FMath::Clamp(UnclampedPlayer.X, PlayerClip.Min.X, PlayerClip.Max.X), FMath::Clamp(UnclampedPlayer.Y, PlayerClip.Min.Y, PlayerClip.Max.Y));
	bPlayerOffscreen = !PlayerClip.IsInsideOrOn(UnclampedPlayer);
	if (bLabels)
	{
		TArray<FBox2D> Labels;
		Labels.Add(FBox2D(PlayerMapPosition - FVector2D(14 * UiScale), PlayerMapPosition + FVector2D(14 * UiScale)));
		for (int32 Index = 0; Index < Regions.Num(); ++Index)
		{
			if (Regions[Index].Kind != EMiningMapRegionKind::Room && Regions[Index].Kind != EMiningMapRegionKind::Space) continue;
			const FString Name = Regions[Index].Name.Left(22);
			const FVector2D LabelSize = Canvas->K2_TextSize(GEngine->GetSmallFont(), Name, FVector2D(UiScale));
			const FVector2D Position = Screen(ProjectPoint(Regions[Index].Bounds.GetCenter())) - FVector2D(LabelSize.X * 0.5, 18 * UiScale);
			const FBox2D LabelRect(Position, Position + LabelSize);
			if (!Graph.IsInsideOrOn(LabelRect.Min) || !Graph.IsInsideOrOn(LabelRect.Max) || Labels.ContainsByPredicate([&LabelRect](const FBox2D& Other) { return Other.Intersect(LabelRect); })) continue;
			Labels.Add(LabelRect);
			Text(Name, Position, FLinearColor::White, LabelSize.X + 1);
		}
	}
	FVector2D Direction = bPlayerOffscreen ? (UnclampedPlayer - PlayerMapPosition).GetSafeNormal() : ProjectPoint(HUD->PlayerOwner->GetControlRotation().Vector());
	const bool bOutOfPlane = Direction.SizeSquared() < 0.025;
	Direction.Normalize();
	for (int32 Step = 0; Step < 20; ++Step)
	{
		const double First = Step * 2 * PI / 20;
		const double Second = (Step + 1) * 2 * PI / 20;
		Line(PlayerMapPosition + FVector2D(FMath::Cos(First), FMath::Sin(First)) * 10 * UiScale, PlayerMapPosition + FVector2D(FMath::Cos(Second), FMath::Sin(Second)) * 10 * UiScale, Accent, 1.5f * UiScale);
	}
	if (bOutOfPlane)
	{
		Line(PlayerMapPosition - FVector2D(4, 0) * UiScale, PlayerMapPosition + FVector2D(4, 0) * UiScale, FLinearColor::White, 2 * UiScale);
		Line(PlayerMapPosition - FVector2D(0, 4) * UiScale, PlayerMapPosition + FVector2D(0, 4) * UiScale, FLinearColor::White, 2 * UiScale);
	}
	else
	{
		const FVector2D Side(-Direction.Y, Direction.X);
		Fill({PlayerMapPosition + Direction * 9 * UiScale, PlayerMapPosition - Direction * 5 * UiScale + Side * 5 * UiScale, PlayerMapPosition - Direction * 5 * UiScale - Side * 5 * UiScale}, FLinearColor::White);
	}
	const double ScaleWorld = FMath::Pow(10.0, FMath::FloorToDouble(FMath::LogX(10.0, 80 * UiScale / Scale)));
	const FVector2D ScaleStart = Graph.Min + FVector2D(8 * UiScale, Graph.GetSize().Y - 8 * UiScale);
	Line(ScaleStart, ScaleStart + FVector2D(ScaleWorld * Scale, 0), FLinearColor::White, 2 * UiScale);
	Text(FString::Printf(TEXT("%gm"), ScaleWorld / 100), ScaleStart - FVector2D(0, 18 * UiScale), FLinearColor::White, Graph.GetSize().X - 16 * UiScale);
	const TCHAR* LegendNames[] = {TEXT("Room"), TEXT("Rock"), TEXT("Cave"), TEXT("Hidden"), TEXT("Old")};
	const FLinearColor LegendColors[] = {FLinearColor(0.2f, 0.85f, 0.58f), FLinearColor(0.9f, 0.58f, 0.2f), FLinearColor(0.22f, 0.63f, 1), FLinearColor(1, 0.28f, 0.7f), FLinearColor(0.5f, 0.53f, 0.58f)};
	if (IsValid(Layout) && Layout->bHollowRegionPrototype)
	{
		const TCHAR* HollowNames[] = {TEXT("Main"), TEXT("Branch"), TEXT("Cave"), TEXT("Seal"), TEXT("Plug"), TEXT("Old")};
		const FLinearColor HollowColors[] = {FLinearColor(0.3f, 0.7f, 0.9f), FLinearColor(0.1f, 0.9f, 0.88f), LegendColors[2], LegendColors[3], FLinearColor(0.76f, 0.3f, 1), LegendColors[4]};
		for (int32 Index = 0; Index < 6; ++Index)
			Text(HollowNames[Index], Minimum + FVector2D(12 * UiScale + Index * TextWidth / 6, Size.Y - (bThreeD ? 97 : 79) * UiScale), HollowColors[Index], TextWidth / 6 - 2);
	}
	else for (int32 Index = 0; Index < 5; ++Index)
		Text(LegendNames[Index], Minimum + FVector2D(12 * UiScale + Index * TextWidth / 5, Size.Y - (bThreeD ? 97 : 79) * UiScale), LegendColors[Index], TextWidth / 5 - 2);
	if (bThreeD) Text(TEXT("Alt+mouse orbit / Shift pan / wheel zoom"), Minimum + FVector2D(12 * UiScale, Size.Y - 79 * UiScale), bControllingMap ? FLinearColor::White : Accent, TextWidth);
	Text(TEXT("M show/hide  Tab expand  V change view"), Minimum + FVector2D(12 * UiScale, Size.Y - 60 * UiScale), Accent, TextWidth);
	Text(bThreeD ? TEXT("Home follow  +/- zoom  Backspace reset") : TEXT("Home fit/follow  +/- zoom  L labels"), Minimum + FVector2D(12 * UiScale, Size.Y - 42 * UiScale), Accent, TextWidth);
	const FString Notice = Regions.IsEmpty() ? TEXT("No generated layout found") : bLegacySnapshot ? TEXT("Rebuild layout once to include spaces")
		: bPlayerOffscreen ? TEXT("Player offscreen: Home to follow, or zoom out") : bOutOfPlane ? TEXT("Design only / + marker = facing out of plane") : TEXT("Design only / NOT the excavated surface");
	Text(Notice, Minimum + FVector2D(12 * UiScale, Size.Y - 22 * UiScale), FLinearColor(0.7f, 0.75f, 0.8f), TextWidth);
#endif
}
