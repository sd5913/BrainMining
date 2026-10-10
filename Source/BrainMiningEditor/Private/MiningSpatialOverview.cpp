#include "MiningSpatialOverview.h"
#include "MiningSpatialSafeEditing.h"
#include "MiningSpatialGeometry.h"
#include "MiningSpatialDirectEditing.h"
#include "MiningLayoutRoom.h"
#include "MiningTunnelGenerator.h"
#include "GeneratedMiningRock.h"
#include "DynamicMeshActor.h"
#include "DynamicMeshBuilder.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "UObject/StrongObjectPtr.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorModeRegistry.h"
#include "EdMode.h"
#include "LevelEditorViewport.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "EngineUtils.h"
#include "Engine/Level.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "SceneManagement.h"
#include "SceneViewExtension.h"
#include "SceneInterface.h"
#include "Components/PrimitiveComponent.h"

namespace MiningSpatialOverview
{
	static const FEditorModeID ModeId(TEXT("EM_MiningSpatialOverview"));
	static FScene Scene;
	static FOptions Settings;
	static TStrongObjectPtr<UMaterial> PreviewMaterial;

	static UMaterial* CreatePreviewMaterial()
	{
		UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
		Material->SetShadingModel(MSM_Unlit);
		Material->TwoSided = true;
		Material->SetUsageByFlag(MATUSAGE_EditorCompositing, true);
		UMaterialExpressionVertexColor* VertexColor = NewObject<UMaterialExpressionVertexColor>(Material);
		Material->GetExpressionCollection().AddExpression(VertexColor);
		Material->GetEditorOnlyData()->EmissiveColor.Connect(0, VertexColor);
		Material->PostEditChange();
		return Material;
	}

	void GetPreviewTerrain(UWorld* World, TArray<AActor*>& Actors)
	{
		Actors.Reset();
		if (!World) return;
		for (TActorIterator<AMiningTunnelGenerator> Generator(World); Generator; ++Generator)
		{
			if (!Generator->GetHollowRegions().ContainsByPredicate([](AMiningHollowRegion* Region) { return IsValid(Region) && Region->IsA<AMiningSpatialRegion>(); })) continue;
			for (AActor* Actor : {static_cast<AActor*>(Generator->GeneratedBedrock.Get()), static_cast<AActor*>(Generator->GeneratedFill.Get()), static_cast<AActor*>(Generator->PreviewBedrock.Get()), static_cast<AActor*>(Generator->PreviewFill.Get())}) if (IsValid(Actor)) Actors.AddUnique(Actor);
			for (AGeneratedMiningRock* Segment : Generator->GeneratedSegments) if (IsValid(Segment)) Actors.AddUnique(Segment);
		}
	}

	class FPreviewViewExtension : public FSceneViewExtensionBase
	{
	public:
		explicit FPreviewViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister) {}
		virtual void SetupView(FSceneViewFamily& Family, FSceneView& View) override
		{
			if (!GEditor || GEditor->PlayWorld || !Settings.bSolid || !IsEnabled() || Family.EngineShowFlags.Game || !Family.Scene) return;
			UWorld* World = Family.Scene->GetWorld();
			if (!World || World != GEditor->GetEditorWorldContext().World()) return;
			TArray<AActor*> Actors; GetPreviewTerrain(World, Actors);
			for (AActor* Actor : Actors)
				for (UPrimitiveComponent* Component : TInlineComponentArray<UPrimitiveComponent*>(Actor))
					if (Component->IsRegistered()) View.HiddenPrimitives.Add(Component->GetPrimitiveSceneId());
		}
	};
	static TSharedPtr<FPreviewViewExtension, ESPMode::ThreadSafe> PreviewView;

	void FEntry::AddLine(const FVector& Start, const FVector& End)
	{
		if (Start.ContainsNaN() || End.ContainsNaN()) return;
		Lines.Add({Start, End});
		Bounds += Start;
		Bounds += End;
	}

	static void AddBox(FEntry& Entry, const FBox& Box, const FTransform& Transform = FTransform::Identity)
	{
		if (!Box.IsValid || Box.Min.ContainsNaN() || Box.Max.ContainsNaN()) return;
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Start(Corner & 1 ? Box.Max.X : Box.Min.X, Corner & 2 ? Box.Max.Y : Box.Min.Y, Corner & 4 ? Box.Max.Z : Box.Min.Z);
			Entry.Vertices.Add(Transform.TransformPosition(Start));
			for (int32 Axis = 0; Axis < 3; ++Axis)
				if (!(Corner & (1 << Axis)))
				{
					FVector End = Start;
					End[Axis] = Box.Max[Axis];
					Entry.AddLine(Transform.TransformPosition(Start), Transform.TransformPosition(End));
				}
		}
		Entry.Triangles = {{0, 2, 3}, {0, 3, 1}, {4, 5, 7}, {4, 7, 6}, {0, 1, 5}, {0, 5, 4}, {2, 6, 7}, {2, 7, 3}, {0, 4, 6}, {0, 6, 2}, {1, 3, 7}, {1, 7, 5}};
	}

	static void BuildTunnel(AMiningSpatialTunnel& Tunnel, FEntry& Entry)
	{
		if (Tunnel.Kind == EMiningManualKind::SideRoom)
		{
			Entry.bDraft = Tunnel.RoomSize.ContainsNaN() || Tunnel.RoomSize.GetMin() < 250 || !Tunnel.GetActorScale3D().Equals(FVector::OneVector);
			AddBox(Entry, FBox(-Tunnel.RoomSize * 0.5, Tunnel.RoomSize * 0.5), FTransform(Tunnel.GetActorQuat(), Tunnel.GetActorLocation()));
			return;
		}
		TArray<FMiningSpatialKnot> Knots;
		TArray<MiningSpatial::FRing> Rings;
		FString Error;
		const bool bResolved = Tunnel.ResolveKnots(Knots, Error);
		const double Tolerance = IsValid(Tunnel.Region) ? Tunnel.Region->CurveTolerance : 8;
		if (bResolved && MiningSpatial::Sample(Knots, Tunnel.GetActorTransform().TransformVectorNoScale(Tunnel.InitialUp), Tolerance, Rings, Error, nullptr, Tunnel.Region && Tunnel.Region->bAutomaticBedrock))
		{
			UE::Geometry::FDynamicMesh3 Surface;
			if (MiningSpatial::Sweep(Rings, Surface, Error, false, nullptr, Tunnel.Region && Tunnel.Region->bAutomaticBedrock))
			{
				TMap<int32, int32> Indices;
				for (int32 Vertex : Surface.VertexIndicesItr()) Indices.Add(Vertex, Entry.Vertices.Add(Surface.GetVertex(Vertex)));
				for (int32 Triangle : Surface.TriangleIndicesItr())
				{
					const auto Face = Surface.GetTriangle(Triangle); Entry.Triangles.Add(FIntVector(Indices[Face.A], Indices[Face.B], Indices[Face.C]));
				}
			}
			else Entry.bDraft = true;
			for (int32 Index = 0; Index < Rings.Num(); ++Index)
			{
				const MiningSpatial::FRing& Ring = Rings[Index];
				if (Ring.Knot != INDEX_NONE || Index % 8 == 0 || Index == Rings.Num() - 1)
					for (int32 Vertex = 0; Vertex < Ring.Profile.Num(); ++Vertex)
					{
						const FVector2D Start = Ring.Profile[Vertex], End = Ring.Profile[(Vertex + 1) % Ring.Profile.Num()];
						Entry.AddLine(Ring.Center + Ring.Right * Start.X + Ring.Up * Start.Y, Ring.Center + Ring.Right * End.X + Ring.Up * End.Y);
					}
				for (const FVector2D& Point : Ring.Profile)
				{
					const FVector Position = Ring.Center + Ring.Right * Point.X + Ring.Up * Point.Y;
					if (!Position.ContainsNaN()) Entry.Bounds += Position;
				}
				if (Index == 0) continue;
				const MiningSpatial::FRing& Previous = Rings[Index - 1];
				Entry.AddLine(Previous.Center, Ring.Center);
				for (int32 Vertex = 0; Vertex < Ring.Profile.Num(); Vertex += FMath::Max(1, Ring.Profile.Num() / 4))
					if (Previous.Profile.IsValidIndex(Vertex))
						Entry.AddLine(Previous.Center + Previous.Right * Previous.Profile[Vertex].X + Previous.Up * Previous.Profile[Vertex].Y,
							Ring.Center + Ring.Right * Ring.Profile[Vertex].X + Ring.Up * Ring.Profile[Vertex].Y);
			}
			return;
		}
		Entry.bDraft = true;
		if (!bResolved)
		{
			Knots = Tunnel.Knots;
			for (FMiningSpatialKnot& Knot : Knots)
			{
				Knot.Position = Tunnel.GetActorTransform().TransformPosition(Knot.Position);
				Knot.Arrive = Tunnel.GetActorTransform().TransformVectorNoScale(Knot.Arrive);
				Knot.Leave = Tunnel.GetActorTransform().TransformVectorNoScale(Knot.Leave);
			}
		}
		for (int32 Index = 1; Index < FMath::Min(Knots.Num(), 64); ++Index)
			for (int32 Step = 0; Step < 24; ++Step)
				Entry.AddLine(MiningSpatial::Evaluate(Knots[Index - 1], Knots[Index], Step / 24.0), MiningSpatial::Evaluate(Knots[Index - 1], Knots[Index], (Step + 1) / 24.0));
	}

	void FScene::Reset()
	{
		Entries.Reset();
		CachedWorld.Reset();
	}

	void FScene::Refresh(UWorld* World)
	{
		if (!IsValid(World)) { Reset(); return; }
		if (CachedWorld != World) { Reset(); CachedWorld = World; }
		TMap<TWeakObjectPtr<AActor>, int32> Previous;
		for (int32 Index = 0; Index < Entries.Num(); ++Index) Previous.Add(Entries[Index].Actor, Index);
		TArray<FEntry> Updated;
		for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
		{
			if (Actor->IsHiddenEd() || !Actor->GetLevel()->bIsVisible) continue;
			AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(*Actor);
			AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(*Actor);
			AMiningSpatialRegion* Region = Cast<AMiningSpatialRegion>(*Actor);
			if (!Tunnel && !Room && !Region) continue;
			const FBox Box = Room ? Room->GetRoomBounds() : Region ? Region->GetRockBounds() : FBox(ForceInit);
			const uint32 Hash = Tunnel ? HashCombineFast(Tunnel->GetGeometryHash(), GetTypeHash(IsValid(Tunnel->Region) ? Tunnel->Region->CurveTolerance : 8.f)) : HashCombineFast(GetTypeHash(Box.Min), GetTypeHash(Box.Max));
			const int32* OldIndex = Previous.Find(*Actor);
			if (OldIndex && Entries[*OldIndex].Hash == Hash)
			{
				Updated.Add(MoveTemp(Entries[*OldIndex]));
				continue;
			}
			FEntry& Entry = Updated.AddDefaulted_GetRef();
			Entry.Actor = *Actor;
			Entry.Hash = Hash;
			if (Tunnel) BuildTunnel(*Tunnel, Entry);
			else AddBox(Entry, Box);
			if (Entry.Lines.IsEmpty())
			{
				Entry.bDraft = true;
				AddBox(Entry, FBox(FVector(-50), FVector(50)), FTransform(Actor->GetActorLocation()));
			}
		}
		Entries = MoveTemp(Updated);
	}

	static bool BelongsTo(const AActor* Actor, const AMiningSpatialRegion* Region)
	{
		if (!IsValid(Actor) || !IsValid(Region) || Actor->GetLevel() != Region->GetLevel()) return false;
		if (Actor == Region || Actor == Region->FromRoom || Actor == Region->ToRoom) return true;
		const AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Actor);
		return Tunnel && Tunnel->Region == Region;
	}

	FBox FScene::GetBounds(EScope Scope, AActor* Selected, AMiningSpatialRegion* Region, bool bIncludeRockBounds) const
	{
		if (Scope == EScope::Selection)
			if (AMiningSpatialRegion* SelectedRegion = Cast<AMiningSpatialRegion>(Selected)) { Scope = EScope::Region; Region = SelectedRegion; }
		FBox Result(ForceInit), RegionBounds(ForceInit);
		for (const FEntry& Entry : Entries)
		{
			AActor* Actor = Entry.Actor.Get();
			if (!IsValid(Actor) || !Entry.Bounds.IsValid) continue;
			if (Scope == EScope::Selection && Actor != Selected) continue;
			if (Scope == EScope::Region && !BelongsTo(Actor, Region)) continue;
			if (Actor->IsA<AMiningSpatialRegion>())
			{
				RegionBounds += Entry.Bounds;
				if (!bIncludeRockBounds) continue;
			}
			Result += Entry.Bounds;
		}
		return Result.IsValid ? Result : RegionBounds;
	}

	static FLinearColor Color(const FEntry& Entry)
	{
		if (Entry.bDraft) return FLinearColor(1, 0.12f, 0.12f);
		if (const AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Entry.Actor.Get()))
		{
			if (!Tunnel->bEnabled) return FLinearColor(0.4f, 0.4f, 0.4f);
			switch (Tunnel->Kind)
			{
			case EMiningManualKind::Passage: return IsValid(Tunnel->Region) && Tunnel->Region->MainTunnel == Tunnel ? FLinearColor(0.1f, 0.8f, 1) : FLinearColor(0.1f, 1, 0.6f);
			case EMiningManualKind::Cavity: return FLinearColor(0.85f, 0.35f, 1);
			case EMiningManualKind::SideRoom: return FLinearColor(0.3f, 1, 0.3f);
			case EMiningManualKind::RockBlock: return FLinearColor(1, 0.4f, 0.08f);
			}
		}
		return Entry.Actor->IsA<AMiningLayoutRoom>() ? FLinearColor(0.35f, 0.6f, 1) : FLinearColor(0.45f, 0.45f, 0.5f);
	}

	class FOverviewMode : public FEdMode
	{
	public:
		virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override { return true; }
		virtual bool UsesTransformWidget() const override { return MiningSpatialDirectEditing::HasSelection(); }
		virtual bool UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const override { return UsesTransformWidget(); }
		virtual bool ShouldDrawWidget() const override { return UsesTransformWidget(); }
		virtual FVector GetWidgetLocation() const override { return MiningSpatialDirectEditing::WidgetLocation(); }
		virtual void Exit() override { MiningSpatialDirectEditing::Reset(); Scene.Reset(); FEdMode::Exit(); }
		virtual bool InputKey(FEditorViewportClient* Client, FViewport* Viewport, FKey Key, EInputEvent Event) override { return MiningSpatialDirectEditing::InputKey(Client, Viewport, Key, Event); }
		virtual bool CapturedMouseMove(FEditorViewportClient* Client, FViewport* Viewport, int32 MouseX, int32 MouseY) override { return MiningSpatialDirectEditing::MouseMove(Client, Viewport, MouseX, MouseY); }
		virtual bool MouseMove(FEditorViewportClient* Client, FViewport* Viewport, int32 MouseX, int32 MouseY) override { return MiningSpatialDirectEditing::MouseMove(Client, Viewport, MouseX, MouseY); }
		virtual bool InputDelta(FEditorViewportClient* Client, FViewport* Viewport, FVector& Drag, FRotator& Rotate, FVector& Scale) override { return MiningSpatialDirectEditing::InputDelta(Drag, Rotate, Scale); }
		virtual bool StartTracking(FEditorViewportClient* Client, FViewport* Viewport) override { if (!UsesTransformWidget()) return false; MiningSpatialDirectEditing::Begin(); return true; }
		virtual bool EndTracking(FEditorViewportClient* Client, FViewport* Viewport) override { const bool bHandled = UsesTransformWidget(); MiningSpatialDirectEditing::End(); return bHandled; }
		virtual bool LostFocus(FEditorViewportClient* Client, FViewport* Viewport) override { MiningSpatialDirectEditing::End(); return false; }
		virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override
		{
			if (!GEditor || GEditor->PlayWorld || View->Family->EngineShowFlags.Game) return;
			Scene.Refresh(GetWorld());
			if (Settings.bSolid && !PreviewMaterial.IsValid()) PreviewMaterial.Reset(CreatePreviewMaterial());
			const auto& Issue = MiningSpatialSafeEditing::Issue();
			if (Issue.Actor.IsValid() && Issue.Actor->GetWorld() == GetWorld() && Issue.Geometry.Bounds.IsValid && !Issue.Reason.IsEmpty())
			{
				PDI->SetHitProxy(nullptr);
				const FBox Bounds = Issue.Geometry.Bounds.ExpandBy(20);
				const double Length = FMath::Clamp(Bounds.GetExtent().GetMin() * 0.25, 15.0, 100.0);
				for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
				{
					FVector Corner;
					for (int32 Axis = 0; Axis < 3; ++Axis) Corner[Axis] = CornerIndex & (1 << Axis) ? Bounds.Max[Axis] : Bounds.Min[Axis];
					for (int32 Axis = 0; Axis < 3; ++Axis)
					{
						FVector End = Corner; End[Axis] += CornerIndex & (1 << Axis) ? -Length : Length;
						PDI->DrawLine(Corner, End, FLinearColor(1, 0.65f, 0.05f), SDPG_Foreground, 2.f);
					}
				}
			}
			for (const FEntry& Entry : Scene.GetEntries())
			{
				AActor* Actor = Entry.Actor.Get();
				if (!IsValid(Actor) || (Actor->IsA<AMiningSpatialRegion>() && !Settings.bRockBounds)) continue;
				const bool bSurface = Settings.bSolid && !Actor->IsA<AMiningSpatialRegion>() && !Entry.Triangles.IsEmpty() && PreviewMaterial.IsValid();
				if (bSurface)
				{
					HActor* Proxy = new HActor(Actor, nullptr, HPP_Wireframe); PDI->SetHitProxy(Proxy);
					FDynamicMeshBuilder Builder(View->GetFeatureLevel());
					const FLinearColor Tint = FMath::Lerp(FLinearColor(0.55f, 0.58f, 0.62f), Color(Entry), Actor->IsSelected() ? 0.5f : 0.18f);
					for (const FIntVector& Triangle : Entry.Triangles)
					{
						const FVector Normal = FVector::CrossProduct(Entry.Vertices[Triangle.Y] - Entry.Vertices[Triangle.X], Entry.Vertices[Triangle.Z] - Entry.Vertices[Triangle.X]).GetSafeNormal();
						FColor Shade = (Tint * (0.38f + 0.62f * FMath::Abs(FVector::DotProduct(Normal, FVector(0.35, -0.45, 0.82).GetSafeNormal())))).ToFColor(true); Shade.A = 255;
						const int32 First = Builder.AddVertex(FVector3f(Entry.Vertices[Triangle.X]), FVector2f::ZeroVector, FVector3f::XAxisVector, FVector3f::YAxisVector, FVector3f(Normal), Shade);
						Builder.AddVertex(FVector3f(Entry.Vertices[Triangle.Y]), FVector2f::ZeroVector, FVector3f::XAxisVector, FVector3f::YAxisVector, FVector3f(Normal), Shade);
						Builder.AddVertex(FVector3f(Entry.Vertices[Triangle.Z]), FVector2f::ZeroVector, FVector3f::XAxisVector, FVector3f::YAxisVector, FVector3f(Normal), Shade);
						Builder.AddTriangle(First, First + 1, First + 2);
					}
					Builder.Draw(PDI, FMatrix::Identity, PreviewMaterial->GetRenderProxy(), SDPG_World, true, false, Proxy->Id);
					PDI->SetHitProxy(nullptr);
					if (Settings.bDirectManipulation && !Entry.bDraft) continue;
				}
				if (Actor->IsSelected() && Actor->IsA<AMiningSpatialTunnel>() && !Entry.bDraft && !Settings.bDirectManipulation) continue;
				PDI->SetHitProxy(new HActor(Actor, nullptr, HPP_Wireframe));
				const FLinearColor Tint = Actor->IsSelected() && !Entry.bDraft ? FLinearColor::White : Color(Entry);
				for (const FLine& Line : Entry.Lines) PDI->DrawLine(Line.Start, Line.End, Tint, SDPG_Foreground, Actor->IsSelected() ? 2.f : 1.2f, 0, true);
				PDI->SetHitProxy(nullptr);
			}
			MiningSpatialDirectEditing::Draw(View, PDI);
		}
		virtual void DrawHUD(FEditorViewportClient* Client, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override
		{
			if (!GEditor || GEditor->PlayWorld || Client->IsInGameView() || !GEngine) return;
			MiningSpatialDirectEditing::DrawHUD(View, Canvas);
			if (!Settings.bLabels) return;
			const float Dpi = Canvas->GetDPIScale();
			TArray<FBox2D> Labels;
			const FVector2D ViewMin = FVector2D(View->UnscaledViewRect.Min) / Dpi + FVector2D(4, 4);
			const FVector2D ViewMax = FVector2D(View->UnscaledViewRect.Max) / Dpi - FVector2D(4, 4);
			for (const FEntry& Entry : Scene.GetEntries())
			{
				AActor* Actor = Entry.Actor.Get();
				if (!IsValid(Actor) || !Entry.Bounds.IsValid || (Actor->IsA<AMiningSpatialRegion>() && !Settings.bRockBounds)) continue;
				FVector2D Screen;
				if (!View->WorldToPixel(Entry.Bounds.GetCenter(), Screen) || !View->UnscaledViewRect.Contains(FIntPoint(Screen.X, Screen.Y))) continue;
				FString Label = Actor->GetActorNameOrLabel();
				if (Label.Len() > 28) Label = Label.Left(27) + TEXT("…");
				if (const AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Actor))
				{
					if (!Tunnel->bEnabled) Label += TEXT(" [未启用]");
					else if (Tunnel->bHiddenCavity) Label += TEXT(" [隐藏空洞]");
				}
				if (Entry.bDraft) Label += TEXT(" [无效草稿]");
				FVector2D Size = FVector2D::ZeroVector;
				UCanvas::StrLen(GEngine->GetSmallFont(), Label, Size.X, Size.Y, false, Canvas);
				Size += FVector2D(8, 6);
				if (Size.X > ViewMax.X - ViewMin.X || Size.Y > ViewMax.Y - ViewMin.Y) continue;
				FBox2D LabelBox(ForceInit);
				for (int32 Attempt = 0; Attempt < 16; ++Attempt)
				{
					const double Offset = (Attempt % 2 ? 1 : -1) * (Attempt / 2 + 1) * (Size.Y + 3);
					FVector2D Position = Screen / Dpi + FVector2D(10, Offset);
					Position.X = FMath::Clamp(Position.X, ViewMin.X, ViewMax.X - Size.X);
					Position.Y = FMath::Clamp(Position.Y, ViewMin.Y, ViewMax.Y - Size.Y);
					const FBox2D Candidate(Position, Position + Size);
					if (!Labels.ContainsByPredicate([&Candidate](const FBox2D& Existing) { return Existing.Intersect(Candidate); })) { LabelBox = Candidate; break; }
				}
				if (!LabelBox.bIsValid) continue;
				Labels.Add(LabelBox);
				FCanvasLineItem Leader(Screen / Dpi, LabelBox.Min + FVector2D(0, Size.Y * 0.5));
				Leader.SetColor(Color(Entry));
				Canvas->DrawItem(Leader);
				FCanvasTileItem Backdrop(LabelBox.Min, Size, FLinearColor(0.015f, 0.02f, 0.03f, 0.8f));
				Backdrop.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Backdrop);
				FCanvasTextItem Text(LabelBox.Min + FVector2D(4, 3), FText::FromString(Label), GEngine->GetSmallFont(), Color(Entry));
				Text.EnableShadow(FLinearColor::Black);
				Canvas->DrawItem(Text);
			}
		}
	};

	FOptions& Options() { return Settings; }
	void Register() { FEditorModeRegistry::Get().RegisterMode<FOverviewMode>(ModeId, FText::FromString(TEXT("矿区总览"))); }
	void Unregister() { MiningSpatialDirectEditing::Reset(); FEditorModeRegistry::Get().UnregisterMode(ModeId); Scene.Reset(); PreviewMaterial.Reset(); PreviewView.Reset(); }
	bool IsEnabled() { return GEditor && !IsRunningCommandlet() && GLevelEditorModeTools().IsModeActive(ModeId); }
	void Redraw() { if (GEditor) GEditor->RedrawLevelEditingViewports(); }
	void SetEnabled(bool bEnabled)
	{
		if (!GEditor || GEditor->PlayWorld || IsRunningCommandlet()) return;
		if (bEnabled)
		{
			if (!PreviewView.IsValid()) PreviewView = FSceneViewExtensions::NewExtension<FPreviewViewExtension>();
			bool bHasVisibleMode = false;
			for (const FEditorModeInfo& Mode : GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->GetEditorModeInfoOrderedByPriority())
				bHasVisibleMode |= Mode.IsVisible() && GLevelEditorModeTools().IsModeActive(Mode.ID);
			if (!bHasVisibleMode) GLevelEditorModeTools().ActivateDefaultMode();
			GLevelEditorModeTools().ActivateMode(ModeId);
			if (GCurrentLevelEditingViewportClient) GCurrentLevelEditingViewportClient->SetGameView(false);
		}
		else GLevelEditorModeTools().DeactivateMode(ModeId);
		Redraw();
	}

	bool Focus(EScope Scope, AActor* Selected, AMiningSpatialRegion* Region, FString& Status, EView View)
	{
		if (!GEditor || GEditor->PlayWorld || IsRunningCommandlet()) { Status = TEXT("请先停止运行，再在关卡视口中使用总览。"); return false; }
		UWorld* World = GEditor->GetEditorWorldContext().World();
		FLevelEditorViewportClient* Client = GCurrentLevelEditingViewportClient;
		if (!Client || !GEditor->GetLevelViewportClients().Contains(Client) || Client->GetWorld() != World || !Client->Viewport)
		{
			Status = TEXT("请先点击要操作的关卡视口，再使用聚焦按钮。"); return false;
		}
		Scene.Refresh(World);
		const FBox Bounds = Scene.GetBounds(Scope, Selected, Region, Settings.bRockBounds);
		if (!Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN())
		{
			Status = Scope == EScope::Map ? TEXT("当前可见关卡中没有三维空间或主房间。") : TEXT("请先选择空间、主房间或三维矿区。"); return false;
		}
		SetEnabled(true);
		switch (View)
		{
		case EView::Perspective: Client->SetViewportType(LVT_Perspective); Client->SetViewRotation(FRotator(-30, -50, 0)); break;
		case EView::Top: Client->SetViewportType(LVT_OrthoXY); break;
		case EView::Front: Client->SetViewportType(LVT_OrthoXZ); break;
		case EView::Side: Client->SetViewportType(LVT_OrthoYZ); break;
		default: break;
		}
		Client->FocusViewportOnBox(Bounds.ExpandBy(FMath::Max(100.0, Bounds.GetExtent().GetMax() * 0.08)), true);
		const FIntPoint Pixels = Client->Viewport->GetSizeXY();
		if (Client->IsOrtho() && Pixels.X > 0 && Pixels.Y > 0)
		{
			FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(Client->Viewport, Client->GetScene(), Client->EngineShowFlags));
			const FSceneView* CameraView = Client->CalcSceneView(&Family);
			const FVector Size = Bounds.GetSize();
			const double Width = FVector::DotProduct(Size, CameraView->GetViewRight().GetAbs());
			const double Height = FVector::DotProduct(Size, CameraView->GetViewUp().GetAbs());
			const double UnitsPerPixel = FMath::Max((Width * 1.2 + 200) / Pixels.X, (Height * 1.2 + 200) / Pixels.Y);
			const double CurrentUnits = Client->GetOrthoUnitsPerPixel(Client->Viewport);
			if (FMath::IsFinite(CurrentUnits) && CurrentUnits > UE_SMALL_NUMBER)
				Client->SetOrthoZoom(FMath::Clamp(static_cast<float>(Client->GetOrthoZoom() * UnitsPerPixel / CurrentUnits), FMath::Max(GetDefault<ULevelEditorViewportSettings>()->MinimumOrthographicZoom, MIN_ORTHOZOOM), MAX_ORTHOZOOM));
		}
		Client->Invalidate();
		Status = TEXT("设计轮廓穿透岩石显示，点击可选中；实际地形是否应用请看下方检查结果。切换视角沿用上次聚焦范围。");
		return true;
	}
}
