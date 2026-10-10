#include "MiningSpatialDirectEditing.h"
#include "MiningSpatialOverview.h"
#include "MiningSpatialSafeEditing.h"
#include "MiningSpatialVisualizer.h"
#include "MiningLayoutRoom.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Engine/Selection.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "SceneManagement.h"
#include "ScopedTransaction.h"

namespace MiningSpatialDirectEditing
{
	namespace
	{
		ETool CurrentTool = ETool::Path;
		FHandle Current;
		bool bAutoSmooth = true;
		bool bDragging = false;
		bool bChanged = false;
		FVector DragStart, PlaneNormal, PointerStart, RequestedPosition;
		TWeakObjectPtr<AActor> DragActor;
		TUniquePtr<FScopedTransaction> Transaction;
		FString Feedback;
		double FeedbackUntil = 0;
		TWeakObjectPtr<AMiningSpatialTunnel> FrameSource;
		uint32 FrameHash = 0;
		TArray<MiningSpatial::FRing> FrameRings;
		TWeakObjectPtr<AActor> ControlSource;
		uint32 ControlHash = 0;
		TArray<FControl> CachedControls;
		FVector WidgetTarget = FVector::ZeroVector;

		void Explain(const FString& Text)
		{
			Feedback = Text;
			FeedbackUntil = FPlatformTime::Seconds() + 5;
			MiningSpatialOverview::Redraw();
		}
		AActor* SelectedActor()
		{
			return GEditor && !GEditor->PlayWorld ? GEditor->GetSelectedActors()->GetTop<AActor>() : nullptr;
		}
		bool Room(AActor* Actor, FTransform& Transform, FVector& Size)
		{
			if (const AMiningLayoutRoom* Main = Cast<AMiningLayoutRoom>(Actor))
			{
				if (!Main->GetActorRotation().IsNearlyZero() || !Main->GetActorScale3D().Equals(FVector::OneVector)) return false;
				Transform = FTransform(Main->GetRoomBounds().GetCenter()); Size = Main->RoomSize; return true;
			}
			if (const AMiningSpatialTunnel* Side = Cast<AMiningSpatialTunnel>(Actor); Side && Side->Kind == EMiningManualKind::SideRoom && Side->bEnabled)
			{
				Transform = Side->GetActorTransform(); Size = Side->RoomSize; return Transform.GetScale3D().Equals(FVector::OneVector);
			}
			return false;
		}
		FVector ProfilePosition(const FMiningSpatialKnot& Knot, const MiningSpatial::FRing& Ring, const FVector2D& Point)
		{
			return Ring.Center + Ring.Right * Point.X * Knot.SectionScale.X + Ring.Up * Point.Y * Knot.SectionScale.Y;
		}
		bool SectionBounds(const FMiningSpatialKnot& Knot, FBox2D& Bounds)
		{
			TArray<FVector2D> Polygon; FString Error;
			if (!MiningManual::Sample(Knot.Profile, true, 2, Polygon, Error)) return false;
			Bounds = FBox2D(Polygon); return Bounds.bIsValid && Bounds.GetSize().GetMin() > UE_SMALL_NUMBER;
		}
		void Smooth(AMiningSpatialTunnel& Tunnel, int32 Moved)
		{
			TArray<FMiningSpatialKnot> Knots; FString Error;
			if (!Tunnel.ResolveKnots(Knots, Error)) return;
			for (int32 Index = FMath::Max(1, Moved - 1); Index <= FMath::Min(Knots.Num() - 2, Moved + 1); ++Index)
			{
				if (Tunnel.Knots[Index].bLocked) continue;
				const FVector Before = Knots[Index].Position - Knots[Index - 1].Position;
				const FVector After = Knots[Index + 1].Position - Knots[Index].Position;
				const FVector Direction = (Before.GetSafeNormal() + After.GetSafeNormal()).GetSafeNormal();
				if (Direction.IsNearlyZero()) continue;
				const double Length = FMath::Min(Before.Length(), After.Length()) / 3;
				Tunnel.Knots[Index].Leave = Tunnel.GetActorTransform().InverseTransformVectorNoScale(Direction * Length);
				Tunnel.Knots[Index].Arrive = -Tunnel.Knots[Index].Leave;
			}
		}
		bool ProjectPointer(FEditorViewportClient* Client, FViewport* Viewport, int32 MouseX, int32 MouseY, FVector& Position)
		{
			FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(Viewport, Client->GetScene(), Client->EngineShowFlags));
			const FSceneView* View = Client->CalcSceneView(&Family);
			const FViewportCursorLocation Cursor(View, Client, MouseX, MouseY);
			const double Denominator = FVector::DotProduct(Cursor.GetDirection(), PlaneNormal);
			if (FMath::Abs(Denominator) < 0.05) return false;
			Position = Cursor.GetOrigin() + Cursor.GetDirection() * (FVector::DotProduct(DragStart - Cursor.GetOrigin(), PlaneNormal) / Denominator);
			return !Position.ContainsNaN();
		}
		void UpdatePointer(FEditorViewportClient* Client, FViewport* Viewport, int32 MouseX, int32 MouseY)
		{
			if (!bDragging || DragActor != SelectedActor() || !DragActor.IsValid()) return;
			FVector Pointer, Actual;
			if (ProjectPointer(Client, Viewport, MouseX, MouseY, Pointer) && Location(Current, Actual))
			{
				RequestedPosition = DragStart + Pointer - PointerStart;
				Translate(Current, RequestedPosition - Actual);
			}
		}
	}

	struct HDirectHandle : HHitProxy
	{
		DECLARE_HIT_PROXY();
		FHandle Handle;
		explicit HDirectHandle(const FHandle& Value) : HHitProxy(HPP_UI), Handle(Value) {}
		virtual EMouseCursor::Type GetMouseCursor() override { return EMouseCursor::CardinalCross; }
	};
	IMPLEMENT_HIT_PROXY(HDirectHandle, HHitProxy);

	bool IsEnabled() { return MiningSpatialOverview::IsEnabled() && MiningSpatialOverview::Options().bDirectManipulation; }
	ETool Tool() { return CurrentTool; }
	bool& AutoSmooth() { return bAutoSmooth; }
	void SetTool(ETool Value) { End(); Current = {}; CurrentTool = Value; Feedback.Empty(); FMiningSpatialSelection::Activate(); MiningSpatialOverview::Redraw(); }
	void Reset() { End(); Current = {}; DragActor.Reset(); Feedback.Empty(); FrameSource.Reset(); FrameRings.Reset(); ControlSource.Reset(); CachedControls.Reset(); }
	FString Hint()
	{
		if (MiningSpatialSafeEditing::HasPendingInteractive()) return MiningSpatialSafeEditing::IsFreeEditing(SelectedActor()) ? TEXT("轮廓是当前草稿；后台正在生成岩体。可以继续拖动，生成失败不会回退造型，Esc 取消本次拖动。") : TEXT("正在后台检查目标；实心控制点和地形仍是合法结果，橙点是待检查目标。松手后完成最后一次同步，Esc 取消本次拖动。");
		if (!Feedback.IsEmpty() && FPlatformTime::Seconds() < FeedbackUntil) return Feedback;
		if (const auto& Issue = MiningSpatialSafeEditing::Issue(); !Issue.Reason.IsEmpty()) return (MiningSpatialSafeEditing::IsFreeEditing(SelectedActor()) ? TEXT("草稿可继续编辑：") : TEXT("限制原因：")) + Issue.Reason;
		FTransform Transform; FVector Size;
		if (Room(SelectedActor(), Transform, Size)) return TEXT("拖六个面的控制点改变房间大小，对面保持不动；点“移动空间”后用 W 搬动整个房间，入口随房间更新。");
		if (CurrentTool == ETool::Move) return TEXT("点击空间后用 W 移动整体；空洞可整体移动，房间可直接拖六个面的控制点。");
		if (CurrentTool == ETool::Boundary) return TEXT("选节点，再拖白色轮廓点；青色点只拉一侧边界，绿色点插入轮廓点。入口沿墙面移动。");
		return TEXT("直接拖白色节点拉弯路径；绿色中点按下即可加点并拖动。入口只能沿房间墙面移动。");
	}
	bool Frame(const AMiningSpatialTunnel& Tunnel, int32 Knot, MiningSpatial::FRing& Result)
	{
		const double Tolerance = IsValid(Tunnel.Region) ? Tunnel.Region->CurveTolerance : 8;
		const uint32 Hash = HashCombineFast(Tunnel.GetGeometryHash(), GetTypeHash(Tolerance));
		if (FrameSource.Get() != &Tunnel || FrameHash != Hash)
		{
			FrameSource = const_cast<AMiningSpatialTunnel*>(&Tunnel); FrameHash = Hash; FrameRings.Reset();
			TArray<FMiningSpatialKnot> Knots; FString Error;
			if (!Tunnel.ResolveKnots(Knots, Error) || !MiningSpatial::Sample(Knots, Tunnel.GetActorTransform().TransformVectorNoScale(Tunnel.InitialUp), Tolerance, FrameRings, Error, nullptr, Tunnel.Region && Tunnel.Region->bAutomaticBedrock)) FrameRings.Reset();
		}
		for (const MiningSpatial::FRing& Ring : FrameRings) if (Ring.Knot == Knot) { Result = Ring; return true; }
		if (Tunnel.Region && Tunnel.Region->bFreeEditing && Tunnel.Knots.IsValidIndex(Knot))
		{
			TArray<FMiningSpatialKnot> Knots; FString Error;
			if (!Tunnel.ResolveKnots(Knots, Error)) return false;
			Result.Center = Knots[Knot].Position; Result.Knot = Knot;
			Result.Tangent = (Knots[Knot].Leave - Knots[Knot].Arrive).GetSafeNormal();
			if (Result.Tangent.IsNearlyZero()) Result.Tangent = FVector::ForwardVector;
			FVector Up = Tunnel.GetActorTransform().TransformVectorNoScale(Tunnel.InitialUp).GetSafeNormal();
			if (Up.IsNearlyZero() || FMath::Abs(FVector::DotProduct(Up, Result.Tangent)) > 0.99) Up = FMath::Abs(Result.Tangent.Z) < 0.9 ? FVector::UpVector : FVector::RightVector;
			Up = (Up - Result.Tangent * FVector::DotProduct(Up, Result.Tangent)).GetSafeNormal();
			Result.Up = FQuat(Result.Tangent, FMath::DegreesToRadians(Knots[Knot].Roll)).RotateVector(Up);
			Result.Right = FVector::CrossProduct(Result.Up, Result.Tangent).GetSafeNormal(); Result.Profile.Reset();
			for (const FMiningPenPoint& Point : Knots[Knot].Profile) Result.Profile.Add(Point.Position * Knots[Knot].SectionScale);
			return true;
		}
		return false;
	}
	bool Location(const FHandle& Handle, FVector& Position)
	{
		AActor* Actor = Handle.Actor.Get();
		if (!IsValid(Actor)) return false;
		if (Handle.Kind == EHandle::RoomFace)
		{
			FTransform Transform; FVector Size;
			if (Handle.Index < 0 || Handle.Index > 5 || !Room(Actor, Transform, Size)) return false;
			FVector Local = FVector::ZeroVector; Local[Handle.Index / 2] = Size[Handle.Index / 2] * (Handle.Index % 2 ? -0.5 : 0.5);
			Position = Transform.TransformPosition(Local); return true;
		}
		AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Actor);
		if (!Tunnel || !Tunnel->Knots.IsValidIndex(Handle.Knot)) return false;
		TArray<FMiningSpatialKnot> Knots; FString Error;
		if (!Tunnel->ResolveKnots(Knots, Error)) return false;
		if (Handle.Kind == EHandle::Knot) { Position = Knots[Handle.Knot].Position; return true; }
		if (Handle.Kind == EHandle::InsertKnot && Knots.IsValidIndex(Handle.Knot + 1)) { Position = MiningSpatial::Evaluate(Knots[Handle.Knot], Knots[Handle.Knot + 1], 0.5); return true; }
		MiningSpatial::FRing Ring;
		if (!Frame(*Tunnel, Handle.Knot, Ring)) return false;
		const FMiningSpatialKnot& Knot = Tunnel->Knots[Handle.Knot];
		if ((Handle.Kind == EHandle::Profile || Handle.Kind == EHandle::InsertProfile) && Knot.Profile.IsValidIndex(Handle.Index))
		{
			const FVector2D Point = Handle.Kind == EHandle::Profile ? Knot.Profile[Handle.Index].Position : MiningManual::Evaluate(Knot.Profile[Handle.Index], Knot.Profile[(Handle.Index + 1) % Knot.Profile.Num()], 0.5);
			Position = ProfilePosition(Knot, Ring, Point); return true;
		}
		if (Handle.Kind == EHandle::SectionSide && Handle.Index >= 0 && Handle.Index < 4)
		{
			FBox2D Bounds; if (!SectionBounds(Knot, Bounds)) return false;
			FVector2D Point = Bounds.GetCenter(); const int32 Axis = Handle.Index / 2;
			Point[Axis] = Handle.Index % 2 ? Bounds.Min[Axis] : Bounds.Max[Axis];
			Point[Axis] += (Handle.Index % 2 ? -70 : 70) / Knot.SectionScale[Axis];
			Position = ProfilePosition(Knot, Ring, Point); return true;
		}
		return false;
	}
	static void BuildControls(AActor* Actor, TArray<FControl>& Result)
	{
		Result.Reset();
		if (!IsValid(Actor)) return;
		const auto Add = [&](EHandle Kind, int32 Knot, int32 Index, const FString& Label, bool bLocked = false)
		{
			FControl Control; Control.Handle = {Actor, Kind, Knot, Index}; Control.Label = Label; Control.bLocked = bLocked;
			if (Location(Control.Handle, Control.Position)) Result.Add(MoveTemp(Control));
		};
		FTransform Transform; FVector Size;
		if (Room(Actor, Transform, Size))
		{
			const TCHAR* Names[] = {TEXT("右面 +X"), TEXT("左面 -X"), TEXT("前面 +Y"), TEXT("后面 -Y"), TEXT("顶部"), TEXT("底部")};
			for (int32 Face = 0; Face < 6; ++Face) Add(EHandle::RoomFace, 0, Face, Names[Face]);
			return;
		}
		AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Actor);
		if (!Tunnel || !Tunnel->bEnabled || CurrentTool == ETool::Move) return;
		for (int32 Knot = 0; Knot < Tunnel->Knots.Num(); ++Knot)
		{
			Add(EHandle::Knot, Knot, 0, FString::Printf(TEXT("%d%s"), Knot + 1, Tunnel->Knots[Knot].bLocked ? TEXT(" 锁定") : TEXT("")), Tunnel->Knots[Knot].bLocked);
			if (CurrentTool == ETool::Path && Knot + 1 < Tunnel->Knots.Num() && Tunnel->Knots.Num() < 64 && !Tunnel->Knots[Knot].bLocked && !Tunnel->Knots[Knot + 1].bLocked) Add(EHandle::InsertKnot, Knot, 0, TEXT("+"));
		}
		const int32 Knot = FMiningSpatialSelection::Tunnel == Tunnel ? FMiningSpatialSelection::Knot : 0;
		if (CurrentTool != ETool::Boundary || !Tunnel->Knots.IsValidIndex(Knot)) return;
		const FMiningSpatialKnot& Selected = Tunnel->Knots[Knot];
		for (int32 Point = 0; Point < Selected.Profile.Num(); ++Point)
		{
			Add(EHandle::Profile, Knot, Point, FString(), Selected.bLocked || Selected.Profile[Point].bLocked);
			if (!Selected.bLocked && !Selected.Profile[Point].bLocked && !Selected.Profile[(Point + 1) % Selected.Profile.Num()].bLocked) Add(EHandle::InsertProfile, Knot, Point, TEXT("+"));
		}
		const TCHAR* Names[] = {TEXT("右边界"), TEXT("左边界"), TEXT("顶部"), TEXT("底部")};
		for (int32 Side = 0; Side < 4; ++Side) Add(EHandle::SectionSide, Knot, Side, Names[Side], Selected.bLocked);
	}
	void Controls(AActor* Actor, TArray<FControl>& Result)
	{
		if (!IsValid(Actor)) { Result.Reset(); return; }
		uint32 Hash = HashCombineFast(GetTypeHash(static_cast<int32>(CurrentTool)), GetTypeHash(FMiningSpatialSelection::Knot));
		if (AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Actor)) Hash = HashCombineFast(Hash, HashCombineFast(Tunnel->GetGeometryHash(), GetTypeHash(IsValid(Tunnel->Region) ? Tunnel->Region->CurveTolerance : 8.f)));
		else if (AMiningLayoutRoom* Main = Cast<AMiningLayoutRoom>(Actor)) Hash = HashCombineFast(Hash, HashCombineFast(GetTypeHash(Main->GetRoomBounds().ToString()), GetTypeHash(Main->GetActorTransform().ToString())));
		if (ControlSource != Actor || ControlHash != Hash)
		{
			ControlSource = Actor; ControlHash = Hash; BuildControls(Actor, CachedControls);
		}
		Result = CachedControls;
	}
	bool Translate(const FHandle& Handle, const FVector& Delta)
	{
		AActor* Actor = Handle.Actor.Get();
		if (!IsValid(Actor) || Delta.ContainsNaN() || Delta.IsNearlyZero() || (GEditor && GEditor->PlayWorld)) return false;
		MiningSpatialSafeEditing::Observe(Actor->GetWorld());
		if (Handle.Kind == EHandle::RoomFace)
		{
			FTransform Transform; FVector Size;
			if (!Room(Actor, Transform, Size) || Handle.Index < 0 || Handle.Index > 5) return false;
			const int32 Axis = Handle.Index / 2; const double Sign = Handle.Index % 2 ? -1 : 1;
			const double Requested = Transform.InverseTransformVectorNoScale(Delta)[Axis] * Sign;
			const double Change = FMath::Max(250.0 - Size[Axis], Requested);
			if (FMath::IsNearlyZero(Change)) { Explain(TEXT("房间尺寸至少250cm；请向外拉，或改动其他面。")); return false; }
			Actor->Modify(); Size[Axis] += Change;
			FVector Shift = FVector::ZeroVector; Shift[Axis] = Change * Sign * 0.5;
			FVector Center = Transform.GetLocation() + Transform.TransformVectorNoScale(Shift);
			if (AMiningLayoutRoom* Main = Cast<AMiningLayoutRoom>(Actor))
			{
				Main->RoomSize = Size; Main->SetActorLocation(Center - (Main->bFloorAnchored ? FVector(0, 0, Size.Z * 0.5) : FVector::ZeroVector)); Main->OnConstruction(Main->GetActorTransform());
			}
			else { CastChecked<AMiningSpatialTunnel>(Actor)->RoomSize = Size; Actor->SetActorLocation(Center); }
		}
		else
		{
			AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Actor);
			if (!Tunnel || !Tunnel->Knots.IsValidIndex(Handle.Knot) || !Tunnel->bEnabled) return false;
			if (Tunnel->Knots[Handle.Knot].bLocked) { Explain(TEXT("当前节点已锁定。请先在高级编辑中解锁，或选择其他节点。")); return false; }
			FMiningSpatialKnot& Knot = Tunnel->Knots[Handle.Knot];
			if (Handle.Kind == EHandle::Knot)
			{
				Tunnel->Modify();
				FMiningSpatialPortal* Portal = Handle.Knot == 0 ? &Tunnel->StartPortal : Handle.Knot == Tunnel->Knots.Num() - 1 ? &Tunnel->EndPortal : nullptr;
				if (Portal && Portal->Room)
				{
					FTransform Transform; FVector HalfSize, Center, Normal; FString Error;
					if (!Tunnel->ResolvePortal(*Portal, Transform, HalfSize, Center, Normal, Error)) { Explain(Error); return false; }
					const int32 Axis = static_cast<int32>(Portal->Wall) / 2; const FVector Local = Transform.InverseTransformVectorNoScale(Delta);
					const FVector2D Offset(Local[(Axis + 1) % 3], Local[(Axis + 2) % 3]);
					if (Offset.IsNearlyZero()) { Explain(TEXT("这是房间入口，只能沿墙面移动。要前后移动入口，请移动房间或拖动房间的面。")); return false; }
					Portal->Offset += Offset;
				}
				else Knot.Position += Tunnel->GetActorTransform().InverseTransformVectorNoScale(Delta);
				if (bAutoSmooth) Smooth(*Tunnel, Handle.Knot);
			}
			else
			{
				MiningSpatial::FRing Ring; if (!Frame(*Tunnel, Handle.Knot, Ring)) return false;
				const FVector2D Local(FVector::DotProduct(Delta, Ring.Right) / Knot.SectionScale.X, FVector::DotProduct(Delta, Ring.Up) / Knot.SectionScale.Y);
				if (Local.IsNearlyZero()) { Explain(TEXT("轮廓点在当前截面内移动；点击“正对截面”能更清楚地勾画边界。")); return false; }
				if (Handle.Kind == EHandle::Profile && Knot.Profile.IsValidIndex(Handle.Index))
				{
					if (Knot.Profile[Handle.Index].bLocked) { Explain(TEXT("这个轮廓点已锁定，请先解锁。")); return false; }
					Tunnel->Modify(); Knot.Profile[Handle.Index].Position += Local;
				}
				else if (Handle.Kind == EHandle::SectionSide && Handle.Index >= 0 && Handle.Index < 4)
				{
					if (Knot.Profile.ContainsByPredicate([](const FMiningPenPoint& Point) { return Point.bLocked; })) { Explain(TEXT("截面里有锁定点，不能整体拉伸这一侧。请拖未锁定的白点，或先解锁。")); return false; }
					FBox2D Bounds; if (!SectionBounds(Knot, Bounds)) return false;
					const int32 Axis = Handle.Index / 2; const bool bNegative = Handle.Index % 2 != 0;
					const double Fixed = bNegative ? Bounds.Max[Axis] : Bounds.Min[Axis];
					const double Width = Bounds.GetSize()[Axis];
					const double Factor = FMath::Max(100.0 / Knot.SectionScale[Axis], Width + Local[Axis] * (bNegative ? -1 : 1)) / Width;
					Tunnel->Modify();
					for (FMiningPenPoint& Point : Knot.Profile)
					{
						Point.Position[Axis] = Fixed + (Point.Position[Axis] - Fixed) * Factor;
						Point.Arrive[Axis] *= Factor; Point.Leave[Axis] *= Factor;
					}
				}
				else return false;
			}
		}
		if (MiningSpatialSafeEditing::IsInteractive() && MiningSpatialSafeEditing::QueueInteractive(Actor)) { Feedback.Empty(); return true; }
		const bool bAccepted = MiningSpatialSafeEditing::Apply(Actor);
		if (!bAccepted || MiningSpatialSafeEditing::Issue().bLimited) Explain(MiningSpatialSafeEditing::Issue().Reason);
		else { Feedback.Empty(); bChanged = true; }
		bChanged |= bAccepted;
		return bAccepted;
	}
	bool Insert(FHandle& Handle)
	{
		AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Handle.Actor.Get());
		if (!Tunnel || !Tunnel->Knots.IsValidIndex(Handle.Knot) || (GEditor && GEditor->PlayWorld)) return false;
		MiningSpatialSafeEditing::Observe(Tunnel->GetWorld());
		Tunnel->Modify();
		if (Handle.Kind == EHandle::InsertKnot)
		{
			TArray<FMiningSpatialKnot> Knots; FString Error;
			if (!Tunnel->ResolveKnots(Knots, Error)) return false;
			for (FMiningSpatialKnot& Knot : Knots)
			{
				Knot.Position = Tunnel->GetActorTransform().InverseTransformPosition(Knot.Position);
				Knot.Arrive = Tunnel->GetActorTransform().InverseTransformVectorNoScale(Knot.Arrive);
				Knot.Leave = Tunnel->GetActorTransform().InverseTransformVectorNoScale(Knot.Leave);
			}
			if (!MiningSpatial::Insert(Knots, Handle.Knot, 0.5)) return false;
			Tunnel->Knots = MoveTemp(Knots);
		}
		else if (Handle.Kind == EHandle::InsertProfile)
		{
			if (Tunnel->Knots[Handle.Knot].bLocked || !MiningManual::Insert(Tunnel->Knots[Handle.Knot].Profile, Handle.Index, 0.5, true)) return false;
		}
		else return false;
		if (!MiningSpatialSafeEditing::Apply(Tunnel)) { Explain(TEXT("这里不能插点：") + MiningSpatialSafeEditing::Issue().Reason); return false; }
		if (Handle.Kind == EHandle::InsertKnot) { Handle.Kind = EHandle::Knot; ++Handle.Knot; }
		else { Handle.Kind = EHandle::Profile; ++Handle.Index; }
		bChanged = true; return true;
	}
	bool Delete(const FHandle& Handle)
	{
		AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Handle.Actor.Get());
		if (!Tunnel || !Tunnel->Knots.IsValidIndex(Handle.Knot) || Tunnel->Knots[Handle.Knot].bLocked || (GEditor && GEditor->PlayWorld)) return false;
		MiningSpatialSafeEditing::Observe(Tunnel->GetWorld());
		FScopedTransaction DeleteTransaction(FText::FromString(TEXT("删除空间控制点"))); Tunnel->Modify();
		if (Handle.Kind == EHandle::Knot && Tunnel->Knots.Num() > 2 && !(Handle.Knot == 0 && Tunnel->StartPortal.Room) && !(Handle.Knot == Tunnel->Knots.Num() - 1 && Tunnel->EndPortal.Room)) Tunnel->Knots.RemoveAt(Handle.Knot);
		else if (Handle.Kind == EHandle::Profile && Tunnel->Knots[Handle.Knot].Profile.Num() > 3 && Tunnel->Knots[Handle.Knot].Profile.IsValidIndex(Handle.Index) && !Tunnel->Knots[Handle.Knot].Profile[Handle.Index].bLocked) Tunnel->Knots[Handle.Knot].Profile.RemoveAt(Handle.Index);
		else { DeleteTransaction.Cancel(); Explain(TEXT("连接入口、锁定点和轮廓的必要点不能删除；不会删除整个空间。")); return false; }
		if (!MiningSpatialSafeEditing::Apply(Tunnel)) { DeleteTransaction.Cancel(); Explain(MiningSpatialSafeEditing::Issue().Reason); return false; }
		Current = {}; FMiningSpatialSelection::Select(Tunnel, FMath::Min(Handle.Knot, Tunnel->Knots.Num() - 1)); return true;
	}
	void Select(const FHandle& Handle)
	{
		Current = Handle; Feedback.Empty();
		if (AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Handle.Actor.Get())) FMiningSpatialSelection::Select(Tunnel, Handle.Knot);
		MiningSpatialOverview::Redraw();
	}
	void Begin()
	{
		if (Transaction || !Current.Actor.IsValid()) return;
		MiningSpatialSafeEditing::Observe(Current.Actor->GetWorld());
		Transaction = MakeUnique<FScopedTransaction>(FText::FromString(TEXT("直接塑形空间"))); Current.Actor->Modify(); bChanged = false;
	}
	void End(bool bCancel)
	{
		bDragging = false; DragActor.Reset();
		bChanged |= MiningSpatialSafeEditing::FinishInteractive(bCancel);
		if (!Transaction) return;
		if (!bChanged) Transaction->Cancel();
		Transaction.Reset();
		if (bCancel && bChanged && GEditor)
		{
			GEditor->UndoTransaction();
			if (Current.Actor.IsValid()) MiningSpatialSafeEditing::Apply(Current.Actor.Get());
			Explain(TEXT("已取消这次拖动，恢复拖动前的空间。"));
		}
		bChanged = false;
	}
	bool HasSelection() { FVector Position; return IsEnabled() && Current.Actor == SelectedActor() && Location(Current, Position); }
	FVector WidgetLocation() { FVector Position = FVector::ZeroVector; Location(Current, Position); return Position; }
	bool InputDelta(const FVector& Delta, const FRotator& Rotation, const FVector& Scale)
	{
		if (!HasSelection()) return false;
		if (bDragging) return true;
		Begin();
		if (!MiningSpatialSafeEditing::IsInteractive()) WidgetTarget = WidgetLocation();
		MiningSpatialSafeEditing::BeginInteractive();
		if (!Delta.IsNearlyZero()) { WidgetTarget += Delta; Translate(Current, WidgetTarget - WidgetLocation()); }
		else if (!Rotation.IsNearlyZero() || !Scale.IsNearlyZero()) Explain(TEXT("当前选中的是局部控制点。请用 W 移动；扭转可在高级编辑中调整。移动整个空间请切换“移动空间”。"));
		return true;
	}
	bool InputKey(FEditorViewportClient* Client, FViewport* Viewport, FKey Key, EInputEvent Event)
	{
		if (!IsEnabled() || !Client || !Viewport || (GEditor && GEditor->PlayWorld)) return false;
		if (Key == EKeys::Escape && Event == IE_Pressed && bDragging) { End(true); return true; }
		if (Key == EKeys::Delete && Event == IE_Pressed && HasSelection()) { End(); Delete(Current); return true; }
		if (Key != EKeys::LeftMouseButton) return false;
		if (Event == IE_Released && bDragging) { UpdatePointer(Client, Viewport, Viewport->GetMouseX(), Viewport->GetMouseY()); End(); return true; }
		if (Event != IE_Pressed || Client->IsAltPressed() || Client->IsCtrlPressed()) return false;
		HHitProxy* Proxy = Viewport->GetHitProxy(Viewport->GetMouseX(), Viewport->GetMouseY());
		if (!Proxy || !Proxy->IsA(HDirectHandle::StaticGetType()))
		{
			if (!Proxy || Proxy->IsA(HActor::StaticGetType())) { End(); Current = {}; }
			return false;
		}
		Select(static_cast<HDirectHandle*>(Proxy)->Handle); Begin();
		if (Current.Kind == EHandle::InsertKnot || Current.Kind == EHandle::InsertProfile)
		{
			if (!Insert(Current)) { End(); return true; }
			Select(Current);
		}
		if (!Location(Current, DragStart)) { End(); return true; }
		FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(Viewport, Client->GetScene(), Client->EngineShowFlags));
		const FSceneView* View = Client->CalcSceneView(&Family);
		PlaneNormal = View->GetViewDirection();
		if (Current.Kind == EHandle::Profile || Current.Kind == EHandle::SectionSide)
		{
			MiningSpatial::FRing Ring;
			if (Frame(*CastChecked<AMiningSpatialTunnel>(Current.Actor.Get()), Current.Knot, Ring)) PlaneNormal = Ring.Tangent;
		}
		if (!ProjectPointer(Client, Viewport, Viewport->GetMouseX(), Viewport->GetMouseY(), PointerStart)) { Explain(TEXT("当前视角几乎侧对截面；请点击“正对截面”，或用 W 操作轴调整。")); End(); return true; }
		RequestedPosition = DragStart; bDragging = true; DragActor = Current.Actor;
		MiningSpatialSafeEditing::BeginInteractive(); return true;
	}
	bool MouseMove(FEditorViewportClient* Client, FViewport* Viewport, int32 MouseX, int32 MouseY)
	{
		if (!bDragging) return false;
		if (!IsEnabled() || DragActor != SelectedActor() || !DragActor.IsValid() || !Viewport->KeyState(EKeys::LeftMouseButton)) { End(); return false; }
		UpdatePointer(Client, Viewport, MouseX, MouseY);
		return true;
	}
	void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
	{
		if (!IsEnabled() || (bDragging && DragActor != SelectedActor())) return;
		if (AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(SelectedActor()); Tunnel && CurrentTool != ETool::Move && Tunnel->Kind != EMiningManualKind::SideRoom)
		{
			MiningSpatial::FRing SelectedRing;
			if (Frame(*Tunnel, FMiningSpatialSelection::Knot, SelectedRing))
			{
				for (int32 Ring = 1; Ring < FrameRings.Num(); ++Ring) PDI->DrawLine(FrameRings[Ring - 1].Center, FrameRings[Ring].Center, FLinearColor(0.25f, 1, 0.65f), SDPG_Foreground, 2.f);
				if (CurrentTool == ETool::Boundary)
					for (int32 Point = 0; Point < SelectedRing.Profile.Num(); ++Point)
					{
						const FVector2D First = SelectedRing.Profile[Point], Last = SelectedRing.Profile[(Point + 1) % SelectedRing.Profile.Num()];
						PDI->DrawLine(SelectedRing.Center + SelectedRing.Right * First.X + SelectedRing.Up * First.Y, SelectedRing.Center + SelectedRing.Right * Last.X + SelectedRing.Up * Last.Y, FLinearColor(1, 0.7f, 0.1f), SDPG_Foreground, 2.5f);
					}
			}
		}
		TArray<FControl> Items; Controls(SelectedActor(), Items);
		for (const FControl& Control : Items)
		{
			const bool bInsert = Control.Handle.Kind == EHandle::InsertKnot || Control.Handle.Kind == EHandle::InsertProfile;
			const bool bFace = Control.Handle.Kind == EHandle::RoomFace || Control.Handle.Kind == EHandle::SectionSide;
			const FLinearColor Color = Control.bLocked ? FLinearColor(0.45f, 0.45f, 0.45f) : Control.Handle == Current ? FLinearColor(1, 0.7f, 0.15f) : bInsert ? FLinearColor(0.25f, 1, 0.5f) : bFace ? FLinearColor(0.15f, 0.85f, 1) : FLinearColor::White;
			PDI->SetHitProxy(new HDirectHandle(Control.Handle));
			PDI->DrawPoint(Control.Position, FLinearColor(0.015f, 0.02f, 0.03f), bInsert ? 17 : 23, SDPG_Foreground);
			PDI->DrawPoint(Control.Position, Color, bInsert ? 11 : 16, SDPG_Foreground);
			PDI->SetHitProxy(nullptr);
		}
		FVector Actual;
		if (bDragging && Location(Current, Actual) && FVector::DistSquared(Actual, RequestedPosition) > 4 && !MiningSpatialSafeEditing::Issue().Reason.IsEmpty())
		{
			PDI->SetHitProxy(nullptr);
			PDI->DrawLine(Actual, RequestedPosition, FLinearColor(1, 0.4f, 0.1f), SDPG_Foreground, 1.5f);
			PDI->DrawPoint(RequestedPosition, FLinearColor(1, 0.25f, 0.1f), 9, SDPG_Foreground);
		}
	}
	void DrawHUD(const FSceneView* View, FCanvas* Canvas)
	{
		if (!IsEnabled() || !GEngine) return;
		TArray<FControl> Items; Controls(SelectedActor(), Items);
		for (const FControl& Control : Items)
		{
			FVector2D Screen;
			if (Control.Label.IsEmpty() || !View->WorldToPixel(Control.Position, Screen) || !View->UnscaledViewRect.Contains(FIntPoint(Screen.X, Screen.Y))) continue;
			FCanvasTextItem Label(Screen / Canvas->GetDPIScale() + FVector2D(11, -17), FText::FromString(Control.Label), GEngine->GetSmallFont(), FLinearColor::White);
			Label.EnableShadow(FLinearColor::Black); Canvas->DrawItem(Label);
		}
		if (!SelectedActor()) return;
		FString Text = Hint(); if (Text.Len() > 95) Text = Text.Left(94) + TEXT("…");
		const FVector2D Origin = FVector2D(View->UnscaledViewRect.Min) / Canvas->GetDPIScale() + FVector2D(16, 45);
		const float Width = FMath::Min(1050.f, static_cast<float>(View->UnscaledViewRect.Width() / Canvas->GetDPIScale() - 32));
		FCanvasTileItem Background(Origin - FVector2D(8, 5), FVector2D(Width, 28), FLinearColor(0.015f, 0.022f, 0.035f, 0.92f)); Background.BlendMode = SE_BLEND_Translucent; Canvas->DrawItem(Background);
		FCanvasTextItem Help(Origin, FText::FromString(Text), GEngine->GetSmallFont(), MiningSpatialSafeEditing::Issue().Reason.IsEmpty() ? FLinearColor::White : FLinearColor(1, 0.75f, 0.2f)); Canvas->DrawItem(Help);
	}
	bool FaceSection()
	{
		AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(SelectedActor());
		FLevelEditorViewportClient* Client = GCurrentLevelEditingViewportClient;
		MiningSpatial::FRing Ring;
		if (!Tunnel || !Client || !Frame(*Tunnel, FMiningSpatialSelection::Knot, Ring)) return false;
		SetTool(ETool::Boundary); Client->SetViewportType(LVT_Perspective);
		const double Radius = FBox2D(Ring.Profile).GetSize().GetMax();
		Client->SetViewLocation(Ring.Center - Ring.Tangent * FMath::Max(600.0, Radius * 1.8));
		Client->SetViewRotation(FRotationMatrix::MakeFromXZ(Ring.Tangent, Ring.Up).Rotator()); Client->Invalidate(); return true;
	}
}
