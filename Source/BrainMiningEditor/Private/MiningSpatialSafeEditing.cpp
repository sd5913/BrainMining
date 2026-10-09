#include "MiningSpatialSafeEditing.h"
#include "MiningSpatialEditBridge.h"
#include "MiningSpatialVisualizer.h"
#include "MiningSpatialOverview.h"
#include "MiningSpatialDirectEditing.h"
#include "MiningSpatialBuild.h"
#include "Async/Async.h"
#include "Misc/ScopeExit.h"
#include "MiningTunnelGenerator.h"
#include "MiningLayoutRoom.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "IPIEAuthorizer.h"
#include "Features/IModularFeatures.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "UObject/ObjectSaveContext.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace MiningSpatialSafeEditing
{
	namespace
	{
		struct FSnapshot
		{
			FTransform Transform;
			TMap<FName, FString> Values;
			TArray<FMiningSpatialKnot> Knots;
			FMiningSpatialPortal StartPortal, EndPortal;
			FVector Size = FVector::ZeroVector;
			FVector Up = FVector::UpVector;
			float Tolerance = 8;
			float Margin = 400, Bedrock = 100;

			static FSnapshot Read(AActor* Actor)
			{
				FSnapshot Result; Result.Transform = Actor->GetActorTransform();
				for (TFieldIterator<FProperty> Property(Actor->GetClass()); Property; ++Property)
				{
					const UClass* Owner = Property->GetOwnerClass();
					if (Owner == AActor::StaticClass() || !Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Transient | CPF_InstancedReference)) continue;
					FString Value; const void* Data = Property->ContainerPtrToValuePtr<void>(Actor);
					Property->ExportTextItem_Direct(Value, Data, Data, Actor, PPF_None); Result.Values.Add(Property->GetFName(), MoveTemp(Value));
				}
				if (const AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Actor)) { Result.Knots = Tunnel->Knots; Result.Size = Tunnel->RoomSize; Result.Up = Tunnel->InitialUp; Result.StartPortal = Tunnel->StartPortal; Result.EndPortal = Tunnel->EndPortal; }
				if (const AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(Actor)) Result.Size = Room->RoomSize;
				if (const AMiningSpatialRegion* Region = Cast<AMiningSpatialRegion>(Actor)) { Result.Size = Region->RockSize; Result.Tolerance = Region->CurveTolerance; Result.Margin = Region->ExcavationMargin; Result.Bedrock = Region->AutomaticBedrockThickness; }
				return Result;
			}
			bool Equals(const FSnapshot& Other) const
			{
				if (!Transform.Equals(Other.Transform) || Values.Num() != Other.Values.Num()) return false;
				for (const auto& Value : Values) { const FString* Match = Other.Values.Find(Value.Key); if (!Match || *Match != Value.Value) return false; }
				return true;
			}
			void Restore(AActor* Actor) const
			{
				for (const auto& Value : Values)
					if (FProperty* Property = Actor->GetClass()->FindPropertyByName(Value.Key)) Property->ImportText_Direct(*Value.Value, Property->ContainerPtrToValuePtr<void>(Actor), Actor, PPF_None);
				if (AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Actor)) { Tunnel->Knots = Knots; Tunnel->RoomSize = Size; Tunnel->InitialUp = Up; Tunnel->StartPortal = StartPortal; Tunnel->EndPortal = EndPortal; }
				if (AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(Actor)) Room->RoomSize = Size;
				if (AMiningSpatialRegion* Region = Cast<AMiningSpatialRegion>(Actor)) { Region->RockSize = Size; Region->CurveTolerance = Tolerance; Region->ExcavationMargin = Margin; Region->AutomaticBedrockThickness = Bedrock; }
				Actor->SetActorTransform(Transform);
				if (AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(Actor)) Room->OnConstruction(Room->GetActorTransform());
				if (AMiningSpatialRegion* Region = Cast<AMiningSpatialRegion>(Actor)) Region->RefreshBounds(Region->GetRockBounds());
			}
			bool CanBlend(const FSnapshot& Other) const
			{
				if (Values.Num() != Other.Values.Num() || Knots.Num() != Other.Knots.Num()) return false;
				if (StartPortal.Room != Other.StartPortal.Room || StartPortal.Wall != Other.StartPortal.Wall || EndPortal.Room != Other.EndPortal.Room || EndPortal.Wall != Other.EndPortal.Wall) return false;
				for (const auto& Value : Values)
				{
					if (!Other.Values.Contains(Value.Key)) return false;
					if (Value.Key != TEXT("Knots") && Value.Key != TEXT("RoomSize") && Value.Key != TEXT("RockSize") && Value.Key != TEXT("InitialUp") && Value.Key != TEXT("CurveTolerance") && Value.Key != TEXT("StartPortal") && Value.Key != TEXT("EndPortal") && Value.Value != Other.Values[Value.Key]) return false;
				}
				for (int32 Index = 0; Index < Knots.Num(); ++Index)
				{
					if (Knots[Index].Profile.Num() != Other.Knots[Index].Profile.Num() || Knots[Index].bLocked != Other.Knots[Index].bLocked) return false;
					for (int32 Point = 0; Point < Knots[Index].Profile.Num(); ++Point)
						if (Knots[Index].Profile[Point].Mode != Other.Knots[Index].Profile[Point].Mode || Knots[Index].Profile[Point].bLocked != Other.Knots[Index].Profile[Point].bLocked) return false;
				}
				return true;
			}
			void Blend(AActor* Actor, const FSnapshot& Target, double Alpha) const
			{
				Restore(Actor);
				FTransform Blended; Blended.Blend(Transform, Target.Transform, Alpha); Actor->SetActorTransform(Blended);
				if (AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Actor))
				{
					Tunnel->RoomSize = FMath::Lerp(Size, Target.Size, Alpha); Tunnel->InitialUp = FMath::Lerp(Up, Target.Up, Alpha);
					Tunnel->StartPortal.Offset = FMath::Lerp(StartPortal.Offset, Target.StartPortal.Offset, Alpha); Tunnel->EndPortal.Offset = FMath::Lerp(EndPortal.Offset, Target.EndPortal.Offset, Alpha);
					for (int32 Index = 0; Index < Knots.Num(); ++Index)
					{
						FMiningSpatialKnot& Knot = Tunnel->Knots[Index]; const FMiningSpatialKnot& Other = Target.Knots[Index];
						Knot.Position = FMath::Lerp(Knot.Position, Other.Position, Alpha); Knot.Arrive = FMath::Lerp(Knot.Arrive, Other.Arrive, Alpha); Knot.Leave = FMath::Lerp(Knot.Leave, Other.Leave, Alpha);
						Knot.Roll = FMath::Lerp(Knot.Roll, Other.Roll, Alpha); Knot.SectionScale = FMath::Lerp(Knot.SectionScale, Other.SectionScale, Alpha);
						for (int32 IndexPoint = 0; IndexPoint < Knot.Profile.Num(); ++IndexPoint)
						{
							FMiningPenPoint& Point = Knot.Profile[IndexPoint]; const FMiningPenPoint& OtherPoint = Other.Profile[IndexPoint];
							Point.Position = FMath::Lerp(Point.Position, OtherPoint.Position, Alpha); Point.Arrive = FMath::Lerp(Point.Arrive, OtherPoint.Arrive, Alpha); Point.Leave = FMath::Lerp(Point.Leave, OtherPoint.Leave, Alpha);
						}
					}
				}
				if (AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(Actor)) { Room->RoomSize = FMath::Lerp(Size, Target.Size, Alpha); Room->OnConstruction(Room->GetActorTransform()); }
				if (AMiningSpatialRegion* Region = Cast<AMiningSpatialRegion>(Actor)) { Region->RockSize = FMath::Lerp(Size, Target.Size, Alpha); Region->CurveTolerance = FMath::Lerp(Tolerance, Target.Tolerance, Alpha); Region->RefreshBounds(Region->GetRockBounds()); }
			}
		};
		TWeakObjectPtr<UWorld> CurrentWorld;
		TMap<TWeakObjectPtr<AActor>, FSnapshot> Snapshots;
		TMap<TWeakObjectPtr<AActor>, FSnapshot> RepairOriginals;
		TWeakObjectPtr<AActor> RepairRoot;
		TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<AMiningTunnelGenerator>>> PreviousOwners;
		FIssue CurrentIssue;
		FString CurrentStatus, LastSave;
		bool bBusy = false, bUndoPending = false, bSaving = false, bAdding = false;
		FDelegateHandle SaveHandle, PreSaveHandle;
		bool bInteractive = false, bPreparedCommit = false, bInteractiveChanged = false;
		uint64 DragEpoch = 0;
		FDragStats Stats;
		struct FRegionWork
		{
			MiningSpatial::FRegionInput Input;
			MiningSpatial::FBuildPtr Previous;
		};
		struct FDragRequest
		{
			TWeakObjectPtr<AActor> Actor;
			TWeakObjectPtr<AMiningTunnelGenerator> Generator;
			TWeakObjectPtr<UWorld> World;
			TWeakObjectPtr<UWorld> EditorWorld;
			FSnapshot Target;
			TMap<TWeakObjectPtr<AActor>, FSnapshot> Environment;
			TArray<TWeakObjectPtr<AMiningSpatialRegion>> Regions;
			TArray<FRegionWork> Work;
			uint64 Epoch = 0;
			double Started = 0, ChunkLength = 1200;
			int32 Budget = 30000;
			bool bFree = false;
		};
		TUniquePtr<FDragRequest> PendingDrag, RunningDrag;
		struct FDragResult { TArray<MiningSpatial::FBuildPtr> Builds; double Milliseconds = 0; };
		TFuture<FDragResult> DragTask;
		TMap<TWeakObjectPtr<AMiningSpatialRegion>, MiningSpatial::FBuildPtr> BuildCache;
		void InvalidateDrag()
		{
			++DragEpoch; PendingDrag.Reset(); bInteractive = false;
		}
		void StartPendingDrag()
		{
			if (RunningDrag || !PendingDrag) return;
			RunningDrag = MoveTemp(PendingDrag);
			TArray<FRegionWork> Work = MoveTemp(RunningDrag->Work);
			const double Length = RunningDrag->ChunkLength; const int32 Budget = RunningDrag->Budget;
			DragTask = Async(EAsyncExecution::ThreadPool, [Work = MoveTemp(Work), Length, Budget]() mutable
			{
				FDragResult Result; const double Started = FPlatformTime::Seconds();
				for (FRegionWork& Region : Work)
				{
					Result.Builds.Add(MiningSpatial::Compute(MoveTemp(Region.Input), Length, Budget, MoveTemp(Region.Previous)));
					if (!Result.Builds.Last()->bSuccess) break;
				}
				Result.Milliseconds = (FPlatformTime::Seconds() - Started) * 1000;
				return Result;
			});
		}

		bool Supported(const AActor* Actor) { return Actor && (Actor->IsA<AMiningSpatialTunnel>() || Actor->IsA<AMiningSpatialRegion>() || Actor->IsA<AMiningLayoutRoom>() || Actor->IsA<AMiningTunnelGenerator>()); }
		bool SpatialGenerator(const AMiningTunnelGenerator* Generator)
		{
			return Generator && Generator->bHollowRegionPrototype && Generator->GetHollowRegions().ContainsByPredicate([](const AMiningHollowRegion* Region) { return IsValid(Region) && Region->IsA<AMiningSpatialRegion>(); });
		}
		TArray<AMiningTunnelGenerator*> Owners(AActor* Actor)
		{
			TArray<AMiningTunnelGenerator*> Result;
			for (TActorIterator<AMiningTunnelGenerator> Item(Actor->GetWorld()); Item; ++Item)
			{
				if (!SpatialGenerator(*Item) || Item->GetLevel() != Actor->GetLevel()) continue;
				bool bOwns = Actor == *Item;
				if (AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(Actor)) bOwns |= Room->LayoutId == Item->LayoutId;
				if (AMiningSpatialRegion* Region = Cast<AMiningSpatialRegion>(Actor)) bOwns |= Item->UsesHollowRegion(Region);
				if (AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Actor)) bOwns |= Item->UsesHollowRegion(Tunnel->Region);
				if (bOwns) Result.Add(*Item);
			}
			return Result;
		}
		void Remember(AActor* Actor)
		{
			Snapshots.Add(Actor, FSnapshot::Read(Actor));
			TArray<TWeakObjectPtr<AMiningTunnelGenerator>>& Cached = PreviousOwners.FindOrAdd(Actor); Cached.Reset();
			for (AMiningTunnelGenerator* Generator : Owners(Actor)) Cached.Add(Generator);
		}
		bool PreviouslyOwned(AActor* Actor, AMiningTunnelGenerator* Generator)
		{
			const auto* Cached = PreviousOwners.Find(Actor); return Cached && Cached->Contains(Generator);
		}
		void Capture()
		{
			Snapshots.Reset(); PreviousOwners.Reset();
			if (CurrentWorld.IsValid()) for (TActorIterator<AActor> Item(CurrentWorld.Get()); Item; ++Item) if (Supported(*Item)) Remember(*Item);
		}
		bool FinishRepair(AActor* Root)
		{
			if (!Root || RepairRoot != Root) return false;
			for (const auto& Original : RepairOriginals) if (Original.Key.IsValid()) Remember(Original.Key.Get());
			RepairRoot.Reset(); RepairOriginals.Reset(); return true;
		}
		void Problem(AActor* Actor, const FString& Reason, const MiningSpatial::FDiagnostic& Diagnostic = {})
		{
			CurrentIssue.Actor = Actor; CurrentIssue.Reason = Reason; CurrentIssue.Geometry = Diagnostic;
			if (Reason.Contains(TEXT("内弯折叠")) || Reason.Contains(TEXT("内弯挤穿"))) CurrentIssue.Reason = TEXT("这里的弯相对于通道宽度太急，内侧将互相穿过。把弯拉缓，或单独缩小这一段截面。");
			else if (Reason.Contains(TEXT("零切线")) || Reason.Contains(TEXT("转向过急"))) CurrentIssue.Reason = TEXT("这段路径将形成尖角或反向折返。请沿更平缓的方向拖动黄色手柄。");
			else if (Reason.Contains(TEXT("triangle budget")) || Reason.Contains(TEXT("exceeds 128 chunks"))) CurrentIssue.Reason = TEXT("这次修改超过现有地形预算。请减少局部细节或拆分矿区；工具不会提高预算。");
			if (Diagnostic.Segment != INDEX_NONE) CurrentIssue.Reason += FString::Printf(TEXT("（节点 %d 与 %d 之间）"), Diagnostic.Segment + 1, Diagnostic.Segment + 2);
			if (!CurrentIssue.Geometry.Bounds.IsValid || CurrentIssue.Geometry.Bounds.Min.ContainsNaN() || CurrentIssue.Geometry.Bounds.Max.ContainsNaN())
			{
				FVector Position = Actor ? Actor->GetActorLocation() : FVector::ZeroVector; if (Position.ContainsNaN()) Position = FVector::ZeroVector;
				CurrentIssue.Geometry.Location = Position; CurrentIssue.Geometry.Bounds = FBox(Position - FVector(250), Position + FVector(250));
			}
		}
		bool ValidateSource(AMiningSpatialTunnel* Tunnel)
		{
			if (!Tunnel->bEnabled) return true;
			MiningSpatial::FDiagnostic Diagnostic; TArray<MiningSpatial::FRing> Rings; UE::Geometry::FDynamicMesh3 Mesh; FString Error;
			const AMiningSpatialRegion* Region = Tunnel->Region;
			if (!MiningSpatial::Shape(*Tunnel, IsValid(Region) ? Region->CurveTolerance : 8, Mesh, Rings, Error, &Diagnostic)) { Problem(Tunnel, Error, Diagnostic); return false; }
			if (IsValid(Region) && !Region->bAutomaticBedrock)
				for (int32 Vertex : Mesh.VertexIndicesItr())
					if (!Region->GetRockBounds().ExpandBy(0.1).IsInsideOrOn(Mesh.GetVertex(Vertex)))
					{
						Diagnostic.Location = Mesh.GetVertex(Vertex); Diagnostic.Bounds = FBox(Diagnostic.Location - FVector(150), Diagnostic.Location + FVector(150));
						Problem(Tunnel, TEXT("已经碰到矿区外边界。请向矿区内部移动，或先扩大矿区岩石范围。"), Diagnostic); return false;
					}
			if (Tunnel->Kind != EMiningManualKind::Passage || (Region && Region->bAutomaticBedrock)) return true;
			for (int32 Endpoint = 0; Endpoint < 2; ++Endpoint)
			{
				const FMiningSpatialPortal& Portal = Endpoint ? Tunnel->EndPortal : Tunnel->StartPortal;
				if (!Portal.Room) continue;
				FTransform Transform; FVector HalfSize, Center, Normal;
				if (!Tunnel->ResolvePortal(Portal, Transform, HalfSize, Center, Normal, Error)) { Problem(Tunnel, Error); return false; }
				const MiningSpatial::FRing& Ring = Endpoint ? Rings.Last() : Rings[0]; const int32 Axis = static_cast<int32>(Portal.Wall) / 2;
				for (const FVector2D& Point : Ring.Profile)
				{
					const FVector Local = Transform.InverseTransformPosition(Ring.Center + Ring.Right * Point.X + Ring.Up * Point.Y);
					if (FMath::Abs(Local[(Axis + 1) % 3]) > HalfSize[(Axis + 1) % 3] - 1 || FMath::Abs(Local[(Axis + 2) % 3]) > HalfSize[(Axis + 2) % 3] - 1)
					{
						Diagnostic.Knot = Endpoint ? Tunnel->Knots.Num() - 1 : 0; Diagnostic.Segment = Endpoint ? Tunnel->Knots.Num() - 2 : 0;
						Diagnostic.Location = Center; Diagnostic.Bounds = FBox(Center - HalfSize, Center + HalfSize);
						Problem(Tunnel, TEXT("入口已经超出房间墙面。请扩大房间，或减小入口截面/扭转。"), Diagnostic); return false;
					}
				}
			}
			return true;
		}
		bool Preflight(AActor* Edited, AMiningTunnelGenerator* Generator)
		{
			if (!Generator) { if (AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Edited)) return ValidateSource(Tunnel); return true; }
			if (!SpatialGenerator(Generator)) { Problem(Generator, TEXT("不能在安全编辑中直接移除生成器的全部三维矿区。请保留当前矿区引用。")); return false; }
			for (AMiningHollowRegion* Item : Generator->GetHollowRegions())
				if (AMiningSpatialRegion* Region = Cast<AMiningSpatialRegion>(Item))
				{
					if (Region->bAutomaticBedrock)
					{
						MiningSpatial::FRegionInput Input; FString Error;
						if (!MiningSpatial::Capture(*Region, Generator->GetActorLocation(), Generator->BedrockThickness, Input, Error)) { Problem(Region, Error); return false; }
						continue;
					}
					if (Region->RockSize.ContainsNaN() || Region->RockSize.GetMin() < 500 || !Region->GetActorRotation().IsNearlyZero() || !Region->GetActorScale3D().Equals(FVector::OneVector)) { Problem(Region, TEXT("矿区尺寸至少500cm，旋转保持0、缩放保持1。请用岩石范围尺寸调整边界。")); return false; }
					for (AMiningSpatialTunnel* Tunnel : Region->GetTunnels()) if (!ValidateSource(Tunnel)) return false;
				}
			return true;
		}
		bool BuildCurrent(AActor* Actor, AMiningTunnelGenerator* Generator)
		{
			if (!bPreparedCommit && !Preflight(Actor, Generator)) return false;
			if (!Generator) return true;
			for (TActorIterator<AActor> Item(Generator->GetWorld()); Item; ++Item)
				if (Supported(*Item) && Owners(*Item).Contains(Generator) && Owners(*Item).Num() > 1)
				{
					Problem(*Item, TEXT("同一空间被多个生成器引用。请先明确唯一所属生成器。")); return false;
				}
			TGuardValue<ITransaction*> DerivedTransaction(GUndo, nullptr);
			TMap<TWeakObjectPtr<AActor>, FSnapshot> Inputs;
			for (TActorIterator<AActor> Item(Generator->GetWorld()); Item; ++Item)
				if (Supported(*Item) && (*Item == Generator || Owners(*Item).Contains(Generator))) Inputs.Add(*Item, FSnapshot::Read(*Item));
			if (Generator->RebuildTunnels()) return true;
			const FString Error = Generator->LastBuildStatus;
			for (const auto& Input : Inputs) if (Input.Key.IsValid()) Input.Value.Restore(Input.Key.Get());
			Problem(Actor, Error); return false;
		}
		FString UnappliedReason()
		{
			if (!CurrentWorld.IsValid()) return FString();
			for (TActorIterator<AMiningTunnelGenerator> Item(CurrentWorld.Get()); Item; ++Item)
				if (SpatialGenerator(*Item) && !Item->bLayoutApplied)
				{
					if (CurrentIssue.Reason.IsEmpty() && Item->LastBuildStatus.StartsWith(TEXT("Ready:"))) return TEXT("等待同步地形：当前设计需要重新检查并应用。编辑、保存或开玩时会同步，不会改动原设计。");
					return !CurrentIssue.bLimited && !CurrentIssue.Reason.IsEmpty() ? CurrentIssue.Reason : Item->LastBuildStatus;
				}
			for (TActorIterator<AMiningSpatialTunnel> Item(CurrentWorld.Get()); Item; ++Item)
				if (Item->bEnabled && Owners(*Item).IsEmpty()) return TEXT("存在未关联生成器的空间，仅有设计轮廓，没有可运行地形。");
			return FString();
		}
		void Notify(const FString& Text, bool bSuccess, bool bWarning = false)
		{
			if (IsRunningCommandlet()) return;
			FNotificationInfo Info(FText::FromString(Text)); Info.ExpireDuration = 5;
			if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info)) Item->SetCompletionState(bWarning ? SNotificationItem::CS_None : bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
		void BeforeSave(UWorld* World, FObjectPreSaveContext Context)
		{
			if (bSaving || bBusy || !World || World->IsGameWorld() || Context.IsProceduralSave() || Context.IsFromAutoSave() || (GEditor && GEditor->PlayWorld)) return;
			PrepareWorld(World);
		}
		void Saved(const FString& Filename, UPackage* Package, FObjectPostSaveContext Context)
		{
			if (!CurrentWorld.IsValid() || Package != CurrentWorld->GetOutermost() || Context.IsProceduralSave() || Context.IsFromAutoSave() || Filename.Contains(TEXT("Autosaves"))) return;
			const FString Reason = UnappliedReason();
			LastSave = Context.SaveSucceeded() ? FString::Printf(TEXT("%s %s · %s"), Reason.IsEmpty() ? TEXT("关卡保存成功") : TEXT("仅草稿保存成功 · 地形未生效"), *FDateTime::Now().ToString(TEXT("%H:%M:%S")), *FPaths::GetBaseFilename(Filename)) : TEXT("保存失败；改动仍在编辑器中，请检查输出日志。 ");
			if (Context.SaveSucceeded() && !Reason.IsEmpty()) LastSave += TEXT("\n") + Reason + TEXT(" 请点击“定位限制 / 问题”；修复前不会进入旧地形。");
			if (!bSaving) Notify(LastSave, Context.SaveSucceeded(), Context.SaveSucceeded() && !Reason.IsEmpty());
		}
		class FSpatialPlayAuthorizer : public IPIEAuthorizer
		{
			virtual TValueOrError<bool, FText> IsPIEAuthorizedInternal(bool bSimulate) const override { return MakeValue(true); }
			virtual TValueOrError<bool, FText> RequestPIEPermissionInternal(bool bSimulate) const override
			{
				if (!GEditor || PrepareWorld(GEditor->GetEditorWorldContext().World())) return MakeValue(true);
				CurrentStatus = TEXT("已阻止运行旧地形：") + CurrentIssue.Reason + TEXT(" 请修复后再开玩；保存的草稿没有丢失。");
				FocusIssue();
				return MakeError(FText::FromString(CurrentStatus));
			}
		};
		FSpatialPlayAuthorizer PlayAuthorizer;
	}

	void Register()
	{
		MiningSpatialEditing::Handler.BindStatic(&Handle);
		SaveHandle = UPackage::PackageSavedWithContextEvent.AddStatic(&Saved);
		PreSaveHandle = FEditorDelegates::PreSaveWorldWithContext.AddStatic(&BeforeSave);
		IModularFeatures::Get().RegisterModularFeature(IPIEAuthorizer::GetModularFeatureName(), &PlayAuthorizer);
	}
	void Unregister()
	{
		FinishInteractive(true);
		if (DragTask.IsValid()) DragTask.Wait();
		RunningDrag.Reset(); DragTask = {};
		IModularFeatures::Get().UnregisterModularFeature(IPIEAuthorizer::GetModularFeatureName(), &PlayAuthorizer);
		FEditorDelegates::PreSaveWorldWithContext.Remove(PreSaveHandle);
		MiningSpatialEditing::Handler.Unbind(); UPackage::PackageSavedWithContextEvent.Remove(SaveHandle); Reset();
	}
	void Reset() { InvalidateDrag(); BuildCache.Reset(); CurrentWorld.Reset(); Snapshots.Reset(); PreviousOwners.Reset(); RepairRoot.Reset(); RepairOriginals.Reset(); CurrentIssue = {}; CurrentStatus.Empty(); LastSave.Empty(); bUndoPending = false; }
	const FIssue& Issue() { return CurrentIssue; }
	FString Status() { return CurrentStatus; }
	FString TerrainStatus()
	{
		if (HasPendingInteractive()) return TEXT("正在后台生成 · 设计轮廓与已生成实体可能不同；只有生成完成的当前设计才能进入游戏。");
		if (!CurrentWorld.IsValid()) return TEXT("未打开关卡");
		const FString Reason = UnappliedReason();
		if (!Reason.IsEmpty()) return Reason.StartsWith(TEXT("等待同步")) ? Reason : TEXT("地形未生效 · 设计轮廓不是游戏中的实体。") + Reason;
		for (TActorIterator<AMiningTunnelGenerator> Item(CurrentWorld.Get()); Item; ++Item)
			if (SpatialGenerator(*Item)) return Item->GeneratedFreeAccess.Contains(true) ? TEXT("地形已应用 · 交汇矿区可直接挖掘，不按旧单入口规则回收；每块预算不变。保存后可开玩。") : TEXT("地形已应用 · 保存后可开玩，开玩前会再次检查并同步。");
		return TEXT("当前关卡没有三维矿区生成器。");
	}
	FString SaveStatus()
	{
		if (!CurrentWorld.IsValid()) return TEXT("未打开关卡");
		return CurrentWorld->GetOutermost()->IsDirty() ? TEXT("有未保存修改 · ") + (LastSave.IsEmpty() ? TEXT("请保存当前关卡") : LastSave) : LastSave.IsEmpty() ? TEXT("当前关卡无未保存修改") : LastSave;
	}
	void Observe(UWorld* World)
	{
		if (!World || World->IsGameWorld() || bBusy || CurrentWorld == World) return;
		Reset(); CurrentWorld = World; Capture(); CurrentStatus = TEXT("编辑就绪：自由编辑矿区保留未完成造型；生成结果与草稿分别显示，保存不会丢弃控制点。");
		for (TActorIterator<AMiningTunnelGenerator> Item(World); Item; ++Item)
			if (SpatialGenerator(*Item) && (!Preflight(*Item, *Item) || !Item->bLayoutApplied))
			{
				if (CurrentIssue.Reason.IsEmpty() && Item->LastBuildStatus.StartsWith(TEXT("Ready:")))
				{
					CurrentStatus = TEXT("等待同步地形：编辑、保存或开玩时会检查并应用。原设计没有损坏，也没有判定为预算超限。");
					continue;
				}
				if (CurrentIssue.Reason.IsEmpty()) Problem(*Item, Item->LastBuildStatus);
				CurrentStatus = TEXT("已有草稿尚未应用。原设计已保留；请先定位问题，修复成功后进入连续安全编辑。"); break;
			}
	}
	bool Handle(AActor* Actor)
	{
		if (bBusy || !Supported(Actor) || !CurrentWorld.IsValid() || Actor->GetWorld() != CurrentWorld || (GEditor && GEditor->PlayWorld)) return false;
		if (GIsTransacting) { InvalidateDrag(); bUndoPending = true; return true; }
		if (AMiningTunnelGenerator* Generator = Cast<AMiningTunnelGenerator>(Actor); Generator && !SpatialGenerator(Generator) && !PreviouslyOwned(Generator, Generator)) return false;
		if (!Actor->IsA<AMiningSpatialTunnel>() && !Actor->IsA<AMiningSpatialRegion>() && Owners(Actor).IsEmpty() && (!PreviousOwners.Contains(Actor) || PreviousOwners[Actor].IsEmpty())) return false;
		Apply(Actor); return true;
	}
	bool IsFreeEditing(AActor* Actor)
	{
		if (!IsValid(Actor)) return false;
		if (const AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Actor)) return Tunnel->Region && Tunnel->Region->bFreeEditing;
		if (const AMiningSpatialRegion* Region = Cast<AMiningSpatialRegion>(Actor)) return Region->bFreeEditing;
		for (const AMiningTunnelGenerator* Generator : Owners(Actor))
			for (const AMiningHollowRegion* Item : Generator->GetHollowRegions())
				if (const AMiningSpatialRegion* Region = Cast<AMiningSpatialRegion>(Item); Region && Region->bFreeEditing) return true;
		return false;
	}
	bool Apply(AActor* Actor, bool bForce)
	{
		if (!IsValid(Actor) || bBusy || (GEditor && GEditor->PlayWorld)) return false;
		if (bInteractive || PendingDrag) InvalidateDrag();
		Observe(Actor->GetWorld());
		TGuardValue<bool> Busy(bBusy, true);
		TArray<AMiningTunnelGenerator*> Generators = Owners(Actor);
		if (const auto* Cached = PreviousOwners.Find(Actor)) for (auto Generator : *Cached) if (Generator.IsValid()) Generators.AddUnique(Generator.Get());
		AMiningTunnelGenerator* Generator = Generators.IsEmpty() ? nullptr : Generators[0];
		bool bMissingSource = false;
		for (const auto& Entry : Snapshots) if (!Entry.Key.IsValid())
			if (const auto* Cached = PreviousOwners.Find(Entry.Key); Cached && Cached->Contains(Generator)) bMissingSource = true;
		bool bUntrackedSource = false;
		if (Generator) for (TActorIterator<AActor> Item(CurrentWorld.Get()); Item; ++Item)
			if (Supported(*Item) && !Snapshots.Contains(*Item) && Owners(*Item).Contains(Generator)) { bForce = true; bUntrackedSource |= !bAdding || *Item != Actor; }
		bForce |= bMissingSource || !Snapshots.Contains(Actor);
		TArray<TWeakObjectPtr<AActor>> Changed; TArray<FSnapshot> Targets, Originals;
		for (auto& Entry : Snapshots)
		{
			if (!Entry.Key.IsValid()) continue;
			if (Generator ? Entry.Key.Get() != Generator && !Owners(Entry.Key.Get()).Contains(Generator) && !PreviouslyOwned(Entry.Key.Get(), Generator) : Entry.Key.Get() != Actor) continue;
			const FSnapshot Target = FSnapshot::Read(Entry.Key.Get());
			if (!Entry.Value.Equals(Target)) { Changed.Add(Entry.Key); Targets.Add(Target); Originals.Add(Entry.Value); }
		}
		if (Changed.IsEmpty() && !bForce && !bUndoPending) return CurrentIssue.Reason.IsEmpty();
		bUndoPending = false;
		const auto Restore = [&] { for (int32 Index = 0; Index < Changed.Num(); ++Index) if (Changed[Index].IsValid()) Originals[Index].Restore(Changed[Index].Get()); };
		if (Generators.Num() > 1) { Restore(); Problem(Actor, TEXT("同一空间被多个生成器引用。请先明确唯一所属生成器，避免只更新部分地形。")); CurrentStatus = TEXT("本次修改未接受：生成器归属冲突。"); return false; }
		const bool bWasApplied = Generator && Generator->bLayoutApplied;
		const FString OldBuildStatus = Generator ? Generator->LastBuildStatus : FString();
		const auto Build = [&]() { return BuildCurrent(Actor, Generator); };
		bool bAccepted = Build();
		if (!bAccepted && IsFreeEditing(Actor))
		{
			if (Generator) Generator->AcknowledgeSpatialEdit(false, TEXT("NOT APPLIED: ") + CurrentIssue.Reason);
			for (TActorIterator<AActor> Item(CurrentWorld.Get()); Item; ++Item)
				if (Supported(*Item) && (*Item == Actor || (Generator && (*Item == Generator || Owners(*Item).Contains(Generator))))) Remember(*Item);
			CurrentIssue.bLimited = false; CurrentStatus = TEXT("草稿已保留 · 未回退控制点。地形尚未生成，可以继续修改或保存草稿。") + CurrentIssue.Reason;
			Actor->MarkPackageDirty(); if (GEditor) GEditor->RedrawLevelEditingViewports(); return true;
		}
		FIssue Rejected = CurrentIssue;
		bool bLimited = false;
		bool bHadValidBaseline = bWasApplied;
		if (!bAccepted && IsRepairing() && RepairRoot.Get() == (Generator ? static_cast<AActor*>(Generator) : Actor) && !bAdding && !bMissingSource && !bUntrackedSource)
		{
			CurrentIssue.bLimited = false;
			if (Generator) Generator->AcknowledgeSpatialEdit(false, TEXT("NOT APPLIED: ") + CurrentIssue.Reason);
			for (const auto& Original : RepairOriginals) if (Original.Key.IsValid()) Remember(Original.Key.Get());
			CurrentStatus = TEXT("修复旧草稿中：轮廓改动已保留，但地形尚未生效。可继续分步修改；完整合法后自动恢复安全编辑。");
			Actor->MarkPackageDirty();
			if (GEditor) GEditor->RedrawLevelEditingViewports();
			return false;
		}
		if (!bAccepted && !Changed.IsEmpty())
		{
			Restore();
			bHadValidBaseline = Preflight(Actor, Generator) && (!Generator || bWasApplied);
			bool bBlend = bHadValidBaseline;
			for (int32 Index = 0; Index < Changed.Num(); ++Index) bBlend &= Originals[Index].CanBlend(Targets[Index]);
			if (bBlend)
			{
				double Low = 0, High = 1;
				for (int32 Step = 0; Step < 9 || (Low == 0 && Step < 20); ++Step)
				{
					const double Alpha = (Low + High) * 0.5;
					for (int32 Index = 0; Index < Changed.Num(); ++Index) Originals[Index].Blend(Changed[Index].Get(), Targets[Index], Alpha);
					if (Preflight(Actor, Generator)) Low = Alpha; else High = Alpha;
				}
				for (int32 Attempt = 0; Attempt < 4 && Low > UE_SMALL_NUMBER; ++Attempt, Low *= 0.5)
				{
					for (int32 Index = 0; Index < Changed.Num(); ++Index) Originals[Index].Blend(Changed[Index].Get(), Targets[Index], Low * 0.995);
					if (Build()) { bAccepted = true; bLimited = true; break; }
				}
			}
		}
		if (!bAccepted)
		{
			Restore();
			if (Generator)
			{
				Generator->AcknowledgeSpatialEdit(bWasApplied && !bMissingSource && !bUntrackedSource, bMissingSource ? TEXT("NOT APPLIED: source actor deleted; undo deletion or reconnect layout") : bUntrackedSource ? TEXT("NOT APPLIED: new source has not passed validation") : OldBuildStatus);
				for (AMiningHollowRegion* Region : Generator->GetHollowRegions()) if (Region) Region->RegionStatus = OldBuildStatus;
				if (Generator->bLayoutApplied) for (AMiningHollowRegion* Region : Generator->GetHollowRegions()) if (AMiningSpatialRegion* Spatial = Cast<AMiningSpatialRegion>(Region))
					for (AMiningSpatialTunnel* Tunnel : Spatial->GetTunnels()) Tunnel->ShapeStatus = Tunnel->bEnabled ? TEXT("合法，已应用") : TEXT("未启用");
			}
			CurrentIssue = Rejected; CurrentIssue.bLimited = bHadValidBaseline;
			CurrentStatus = bHadValidBaseline ? TEXT("已限制：保持上一次合法位置，地形没有被改坏。点击“定位限制”查看原因。") : TEXT("旧草稿仍不合法，本次修改已退回。请先点“修复旧草稿”，再分步调整节点；正常安全编辑保持开启。");
			if (bMissingSource) CurrentStatus = TEXT("删除的空间仍被布局引用：新布局未应用，请撤销删除或重新连接。旧地形不会冒充新结果。");
			else if (bUntrackedSource) CurrentStatus = TEXT("外部新增的空间尚未通过检查：新布局未应用。请撤销新增，或修正此对象；旧地形仍保留。");
		}
		else
		{
			const bool bFinishedRepair = FinishRepair(Generator ? static_cast<AActor*>(Generator) : Actor);
			for (auto Entry = Snapshots.CreateIterator(); Entry; ++Entry)
				if (!Entry.Key().IsValid())
				{
					if (auto* Cached = PreviousOwners.Find(Entry.Key()))
					{
						Cached->Remove(Generator); Cached->RemoveAll([](auto Owner) { return !Owner.IsValid(); });
						if (!Cached->IsEmpty()) continue;
					}
					PreviousOwners.Remove(Entry.Key()); Entry.RemoveCurrent();
				}
			for (TActorIterator<AActor> Item(CurrentWorld.Get()); Item; ++Item)
				if (Supported(*Item) && (Generator ? *Item == Generator || Owners(*Item).Contains(Generator) || PreviouslyOwned(*Item, Generator) : *Item == Actor)) Remember(*Item);
			CurrentIssue = bLimited ? Rejected : FIssue(); CurrentIssue.bLimited = bLimited;
			CurrentStatus = Generator ? bLimited ? TEXT("已限制到安全位置 · 控制点、地形和碰撞已同步更新。") : TEXT("合法 · 控制点、地形和碰撞已同步更新。") : TEXT("未关联生成器 · 仅保存编辑数据，尚未生成可运行地形。");
			if (bFinishedRepair) CurrentStatus = TEXT("旧草稿修复完成，已恢复每步合法的安全编辑。") + CurrentStatus;
			if (Generator)
				for (AMiningHollowRegion* Region : Generator->GetHollowRegions()) if (AMiningSpatialRegion* Spatial = Cast<AMiningSpatialRegion>(Region))
					for (AMiningSpatialTunnel* Tunnel : Spatial->GetTunnels()) Tunnel->ShapeStatus = Tunnel->bEnabled ? TEXT("合法，已应用") : TEXT("未启用");
			Actor->MarkPackageDirty();
		}
		if (GEditor) GEditor->RedrawLevelEditingViewports();
		return bAccepted;
	}
	const FDragStats& DragStats() { return Stats; }
	bool IsInteractive() { return bInteractive; }
	bool HasPendingInteractive() { return PendingDrag || (RunningDrag && RunningDrag->Epoch == DragEpoch); }
	void BeginInteractive()
	{
		if (bInteractive) return;
		++DragEpoch; Stats = {}; bInteractive = true; bInteractiveChanged = false;
	}
	bool QueueInteractive(AActor* Actor)
	{
		if (!bInteractive || !IsValid(Actor) || bBusy || IsRepairing() || bUndoPending || !Snapshots.Contains(Actor)) return false;
		const double Started = FPlatformTime::Seconds();
		ON_SCOPE_EXIT { Stats.LastInputMilliseconds = (FPlatformTime::Seconds() - Started) * 1000; Stats.MaxInputMilliseconds = FMath::Max(Stats.MaxInputMilliseconds, Stats.LastInputMilliseconds); };
		const TArray<AMiningTunnelGenerator*> Generators = Owners(Actor);
		const bool bFree = IsFreeEditing(Actor);
		if (Generators.Num() != 1 || (!bFree && !Generators[0]->bLayoutApplied) || !Generators[0]->bAutomaticChunks) return false;
		AMiningTunnelGenerator* Generator = Generators[0];
		for (AMiningHollowRegion* Region : Generator->GetHollowRegions()) if (!IsValid(Region) || !Region->IsA<AMiningSpatialRegion>()) return false;
		for (TActorIterator<AActor> Item(Actor->GetWorld()); Item; ++Item)
			if (*Item != Actor && Supported(*Item) && (*Item == Generator || Owners(*Item).Contains(Generator)))
			{
				const FSnapshot* Accepted = Snapshots.Find(*Item);
				if (!Accepted || !Accepted->Equals(FSnapshot::Read(*Item))) return false;
			}
		TGuardValue<bool> Busy(bBusy, true);
		for (auto Entry = BuildCache.CreateIterator(); Entry; ++Entry) if (!Entry.Key().IsValid()) Entry.RemoveCurrent();
		auto Request = MakeUnique<FDragRequest>();
		Request->Actor = Actor; Request->Generator = Generator; Request->World = Actor->GetWorld(); Request->Target = FSnapshot::Read(Actor);
		Request->bFree = bFree;
		Request->EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		Request->Epoch = DragEpoch; Request->Started = Started; Request->ChunkLength = Generator->ChunkLength; Request->Budget = FMath::Clamp(Generator->SegmentTriangleBudget, 1000, 30000);
		FString Error;
		for (AMiningHollowRegion* Item : Generator->GetHollowRegions())
		{
			AMiningSpatialRegion* Region = CastChecked<AMiningSpatialRegion>(Item);
			FRegionWork& Work = Request->Work.AddDefaulted_GetRef();
			Request->Regions.Add(Region); Work.Previous = BuildCache.FindRef(Region);
			if (!MiningSpatial::Capture(*Region, Generator->GetActorLocation(), Generator->BedrockThickness, Work.Input, Error)) break;
		}
		if (!bFree) Snapshots[Actor].Restore(Actor);
		else
		{
			Remember(Actor); Actor->MarkPackageDirty(); bInteractiveChanged = true;
			Generator->AcknowledgeSpatialEdit(false, TEXT("NOT APPLIED: 正在生成当前草稿"));
		}
		if (!Error.IsEmpty())
		{
			PendingDrag.Reset(); ++DragEpoch;
			Problem(Actor, Error); CurrentIssue.bLimited = !bFree; CurrentStatus = (bFree ? TEXT("草稿已保留 · 可继续修改；尚未生成地形。") : TEXT("目标未接受 · 保留上一份合法地形。")) + CurrentIssue.Reason; return true;
		}
		for (TActorIterator<AActor> Item(Actor->GetWorld()); Item; ++Item)
			if (Supported(*Item) && (*Item == Generator || Owners(*Item).Contains(Generator))) Request->Environment.Add(*Item, FSnapshot::Read(*Item));
		++Stats.Requests;
		if (PendingDrag) ++Stats.Coalesced;
		PendingDrag = MoveTemp(Request);
		CurrentIssue = {}; CurrentStatus = bFree ? TEXT("自由编辑 · 轮廓已更新，正在后台生成；旧地形不代表当前草稿。") : TEXT("正在后台检查目标 · 旧地形仍合法；检查通过后同步控制点、地形和碰撞。");
		StartPendingDrag();
		return true;
	}
	void PumpInteractive()
	{
		if (bBusy) return;
		if (!RunningDrag) { StartPendingDrag(); return; }
		if (!DragTask.IsReady()) return;
		FDragResult Completed = DragTask.Get(); DragTask = {};
		TArray<MiningSpatial::FBuildPtr>& Results = Completed.Builds;
		TUniquePtr<FDragRequest> Request = MoveTemp(RunningDrag);
		AActor* Actor = Request->Actor.Get(); AMiningTunnelGenerator* Generator = Request->Generator.Get();
		bool bCurrent = Request->Epoch == DragEpoch && bInteractive && Actor && Generator && Request->World == CurrentWorld && CurrentWorld.IsValid() && !bUndoPending && !GIsTransacting && (!GEditor || (!GEditor->PlayWorld && GEditor->GetEditorWorldContext().World() == Request->EditorWorld));
		if (bCurrent)
		{
			const FSnapshot* Accepted = Snapshots.Find(Actor);
			bCurrent = Accepted && (Request->bFree ? Request->Target.Equals(FSnapshot::Read(Actor)) : Accepted->Equals(FSnapshot::Read(Actor)));
			for (const auto& Entry : Request->Environment)
				if (!Entry.Key.IsValid() || (Entry.Key != Actor && !Entry.Value.Equals(FSnapshot::Read(Entry.Key.Get())))) { bCurrent = false; break; }
			if (bCurrent) for (TActorIterator<AActor> Item(CurrentWorld.Get()); Item; ++Item)
				if (Supported(*Item) && (*Item == Generator || Owners(*Item).Contains(Generator)) && !Request->Environment.Contains(*Item)) { bCurrent = false; break; }
		}
		if (!bCurrent)
		{
			++Stats.Discarded;
			if (Request->Epoch == DragEpoch && !(Request->bFree && PendingDrag)) { InvalidateDrag(); CurrentStatus = TEXT("设计或关卡已改变，旧后台结果已丢弃；未覆盖你的修改。"); }
			StartPendingDrag(); return;
		}
		TGuardValue<bool> Busy(bBusy, true);
		TMap<const AMiningSpatialRegion*, MiningSpatial::FBuildPtr> Prepared;
		bool bSuccess = Results.Num() == Request->Regions.Num();
		Stats.LastComputeMilliseconds = Completed.Milliseconds;
		for (int32 Index = 0; Index < Results.Num(); ++Index)
		{
			const MiningSpatial::FRegionBuild& Result = *Results[Index];
			if (!Result.bSuccess)
			{
				Problem(Actor, Result.Error, Result.Diagnostic); bSuccess = false; break;
			}
			if (!Request->Regions[Index].IsValid()) { bSuccess = false; break; }
			Prepared.Add(Request->Regions[Index].Get(), Results[Index]);
		}
		const FSnapshot Original = FSnapshot::Read(Actor);
		const FString OldStatus = Generator->LastBuildStatus;
		if (bSuccess)
		{
			const double Started = FPlatformTime::Seconds();
			Request->Target.Restore(Actor);
			MiningSpatial::FPreparedScope Scope(Prepared);
			TGuardValue<bool> PreparedCommit(bPreparedCommit, true);
			bSuccess = BuildCurrent(Actor, Generator);
			Stats.LastCommitMilliseconds = (FPlatformTime::Seconds() - Started) * 1000;
			Stats.MaxCommitMilliseconds = FMath::Max(Stats.MaxCommitMilliseconds, Stats.LastCommitMilliseconds);
		}
		if (bSuccess)
		{
			++Stats.Committed; bInteractiveChanged = true; Stats.LastLatencyMilliseconds = (FPlatformTime::Seconds() - Request->Started) * 1000;
			for (const auto& Entry : Request->Environment) if (Entry.Key.IsValid()) Remember(Entry.Key.Get());
			for (int32 Index = 0; Index < Results.Num(); ++Index) BuildCache.Add(Request->Regions[Index], Results[Index]);
			CurrentIssue = {}; CurrentStatus = TEXT("合法 · 控制点、地形和碰撞已同步更新（后台计算 / 局部岩块更新）。");
			for (const auto& Region : Request->Regions) if (Region.IsValid())
				for (AMiningSpatialTunnel* Tunnel : Region->GetTunnels()) Tunnel->ShapeStatus = Tunnel->bEnabled ? TEXT("合法，已应用") : TEXT("未启用");
			Actor->MarkPackageDirty();
		}
		else
		{
			if (Request->bFree)
			{
				Generator->AcknowledgeSpatialEdit(false, TEXT("NOT APPLIED: ") + CurrentIssue.Reason); Remember(Actor);
				CurrentIssue.bLimited = false; CurrentStatus = TEXT("草稿已保留 · 没有回退造型。地形未生成，可继续编辑或保存。") + CurrentIssue.Reason;
			}
			else
			{
				Original.Restore(Actor); Generator->AcknowledgeSpatialEdit(true, OldStatus);
				CurrentIssue.bLimited = true; CurrentStatus = TEXT("目标未接受 · 保持上一次合法位置，地形和碰撞未被改坏。") + CurrentIssue.Reason;
			}
		}
		if (GEditor) GEditor->RedrawLevelEditingViewports();
		if (PendingDrag)
		{
			for (int32 Index = 0; Index < PendingDrag->Regions.Num(); ++Index) PendingDrag->Work[Index].Previous = BuildCache.FindRef(PendingDrag->Regions[Index]);
			CurrentStatus = TEXT("正在后台检查最新目标 · 当前显示的是上一份已应用的合法地形。");
		}
		StartPendingDrag();
	}
	bool FinishInteractive(bool bCancel)
	{
		if (!bInteractive && !HasPendingInteractive()) { const bool bChanged = bInteractiveChanged; bInteractiveChanged = false; return bChanged; }
		if (!bCancel)
			while (HasPendingInteractive())
			{
				StartPendingDrag();
				if (DragTask.IsValid()) DragTask.Wait();
				PumpInteractive();
			}
		const bool bChanged = bInteractiveChanged; bInteractiveChanged = false;
		InvalidateDrag(); return bChanged;
	}
	bool Add(AActor* Actor)
	{
		TGuardValue<bool> Adding(bAdding, true);
		return Apply(Actor, true);
	}
	bool PrepareWorld(UWorld* World)
	{
		if (!World || World->IsGameWorld() || bBusy || (GEditor && GEditor->PlayWorld)) return false;
		MiningSpatialDirectEditing::End();
		FinishInteractive();
		Observe(World);
		TGuardValue<bool> Busy(bBusy, true);
		bool bReady = true, bHasSpatial = false;
		FIssue FirstIssue;
		for (TActorIterator<AMiningTunnelGenerator> Item(World); Item; ++Item)
		{
			if (!SpatialGenerator(*Item)) continue;
			bHasSpatial = true;
			if (BuildCurrent(*Item, *Item)) { FinishRepair(*Item); continue; }
			CurrentIssue.bLimited = false;
			Item->AcknowledgeSpatialEdit(false, TEXT("NOT APPLIED: ") + CurrentIssue.Reason);
			if (bReady) FirstIssue = CurrentIssue;
			bReady = false;
		}
		for (TActorIterator<AMiningSpatialTunnel> Item(World); Item; ++Item)
		{
			if (!Item->bEnabled || !Owners(*Item).IsEmpty()) continue;
			bHasSpatial = true;
			if (ValidateSource(*Item)) Problem(*Item, TEXT("此空间未关联生成器，仅有设计轮廓，没有可运行地形。请检查所属矿区和生成器引用。"));
			CurrentIssue.bLimited = false;
			if (bReady) FirstIssue = CurrentIssue;
			bReady = false;
		}
		if (bHasSpatial)
		{
			if (bReady) { Capture(); CurrentIssue = {}; CurrentStatus = TEXT("当前设计已完整检查，地形和碰撞已同步，可以开玩。"); }
			else { CurrentIssue = FirstIssue; CurrentStatus = TEXT("地形未生效：") + FirstIssue.Reason + TEXT(" 草稿可以保存，但不会继续运行旧地形。"); }
		}
		return bReady;
	}
	bool Save()
	{
		if (!GEditor || GEditor->PlayWorld) return false;
		const bool bReady = PrepareWorld(GEditor->GetEditorWorldContext().World());
		TGuardValue<bool> Saving(bSaving, true);
		LastSave.Empty();
		const bool bResult = FEditorFileUtils::SaveCurrentLevel();
		if (bResult)
		{
			if (LastSave.IsEmpty()) LastSave = bReady ? TEXT("保存检查完成 · 当前关卡无未保存修改") : TEXT("草稿保存检查完成 · 当前关卡无未保存修改，但地形未生效。") + CurrentIssue.Reason;
			Notify(LastSave, true, !bReady);
		}
		else { LastSave = TEXT("保存未完成（取消或失败），编辑内容仍保留。 "); Notify(LastSave, false); }
		return bResult;
	}
	bool IsRepairing() { return RepairRoot.IsValid() && !RepairOriginals.IsEmpty(); }
	bool BeginRepair()
	{
		if (!GEditor || GEditor->PlayWorld || bBusy || IsRepairing() || !CurrentWorld.IsValid()) return false;
		AActor* Actor = CurrentIssue.Actor.Get();
		if (!Actor || Actor->GetWorld() != CurrentWorld) return false;
		const TArray<AMiningTunnelGenerator*> Generators = Owners(Actor);
		if (Generators.Num() > 1) { Notify(TEXT("此空间被多个生成器引用，请先明确唯一归属。"), false); return false; }
		AMiningTunnelGenerator* Generator = Generators.IsEmpty() ? nullptr : Generators[0];
		if (Generator ? Generator->bLayoutApplied : Preflight(Actor, nullptr))
		{
			Notify(TEXT("当前已有合法地形，继续使用安全编辑即可；不需要进入旧草稿修复。"), false); return false;
		}
		RepairRoot = Generator ? static_cast<AActor*>(Generator) : Actor;
		RepairOriginals.Reset();
		for (TActorIterator<AActor> Item(CurrentWorld.Get()); Item; ++Item)
			if (Supported(*Item) && (Generator ? *Item == Generator || Owners(*Item).Contains(Generator) : *Item == Actor)) RepairOriginals.Add(*Item, FSnapshot::Read(*Item));
		CurrentStatus = TEXT("修复旧草稿中：可分步拖动节点和切线柄，暂时只更新轮廓；地形未合法前不能开玩。修好后自动恢复安全编辑，也可取消修复。");
		FocusIssue();
		return true;
	}
	bool CancelRepair()
	{
		if (!IsRepairing() || bBusy || !GEditor || GEditor->PlayWorld) return false;
		TGuardValue<bool> Busy(bBusy, true);
		FScopedTransaction Transaction(FText::FromString(TEXT("取消旧草稿修复")));
		AActor* Root = RepairRoot.Get();
		bool bMissingActor = false;
		for (const auto& Original : RepairOriginals)
		{
			if (!Original.Key.IsValid()) { bMissingActor = true; continue; }
			Original.Key->Modify(); Original.Value.Restore(Original.Key.Get());
		}
		RepairRoot.Reset(); RepairOriginals.Reset(); Capture();
		CurrentIssue = {};
		AMiningTunnelGenerator* Generator = Cast<AMiningTunnelGenerator>(Root);
		if (Preflight(Root, Generator)) Problem(Root, TEXT("已恢复修复前的草稿，请重新检查并生成。"));
		if (Generator) Generator->AcknowledgeSpatialEdit(false, TEXT("NOT APPLIED: ") + CurrentIssue.Reason);
		CurrentStatus = bMissingActor ? TEXT("已恢复仍存在对象的原始设计；被删除的对象无法由取消修复找回，请撤销删除。") : TEXT("已恢复进入修复前的原始草稿，退出修复模式；原有问题仍需处理。");
		GEditor->RedrawLevelEditingViewports(); return !bMissingActor;
	}
	void FocusIssue()
	{
		if (!GEditor || GEditor->PlayWorld || !CurrentIssue.Actor.IsValid() || !CurrentIssue.Geometry.Bounds.IsValid) return;
		MiningSpatialOverview::SetEnabled(true);
		GEditor->SelectNone(false, true); GEditor->SelectActor(CurrentIssue.Actor.Get(), true, true);
		if (AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(CurrentIssue.Actor.Get())) FMiningSpatialSelection::Select(Tunnel, FMath::Max(0, CurrentIssue.Geometry.Knot));
		if (GCurrentLevelEditingViewportClient) { GCurrentLevelEditingViewportClient->SetGameView(false); GCurrentLevelEditingViewportClient->FocusViewportOnBox(CurrentIssue.Geometry.Bounds.ExpandBy(200), true); }
	}
	bool SmoothIssue()
	{
		AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(CurrentIssue.Actor.Get());
		if (!GEditor || GEditor->PlayWorld) return false;
		if (!Tunnel || Tunnel->Kind == EMiningManualKind::SideRoom || CurrentIssue.Geometry.Segment == INDEX_NONE)
		{
			Notify(TEXT("当前问题无法仅靠平滑手柄修复。请点击“定位限制 / 问题”，按提示调整；原设计未改变。"), false); return false;
		}
		const bool bWasDirty = Tunnel->GetOutermost()->IsDirty();
		FScopedTransaction Transaction(FText::FromString(TEXT("平滑问题弯道（仅合法才应用）"))); Tunnel->Modify();
		const FSnapshot Original = FSnapshot::Read(Tunnel);
		const FIssue OriginalIssue = CurrentIssue;
		const int32 First = FMath::Max(1, CurrentIssue.Geometry.Segment), Last = FMath::Min(Tunnel->Knots.Num() - 2, CurrentIssue.Geometry.Segment + 1);
		for (int32 Neighbors : {0, 1})
		for (bool bChordDirection : {false, true})
		for (double Strength : {0.2, 0.3, 0.4, 0.5, 0.6, 0.75})
		{
			Original.Restore(Tunnel);
			for (int32 Index = FMath::Max(1, First - Neighbors); Index <= FMath::Min(Tunnel->Knots.Num() - 2, Last + Neighbors); ++Index)
			{
				FMiningSpatialKnot& Knot = Tunnel->Knots[Index]; if (Knot.bLocked) continue;
				const FVector Before = Knot.Position - Tunnel->Knots[Index - 1].Position, After = Tunnel->Knots[Index + 1].Position - Knot.Position;
				const FVector Direction = (bChordDirection ? Before + After : Before.GetSafeNormal() + After.GetSafeNormal()).GetSafeNormal();
				Knot.Leave = Direction * FMath::Min(Before.Length(), After.Length()) * Strength; Knot.Arrive = -Knot.Leave;
			}
			if (ValidateSource(Tunnel) && Apply(Tunnel)) { Notify(TEXT("问题弯道已平滑并通过检查。仅调整相邻手柄，节点位置、宽度、扭转和房间保持不变；可撤销。"), true); return true; }
		}
		Original.Restore(Tunnel); CurrentIssue = OriginalIssue; Transaction.Cancel(); Tunnel->GetOutermost()->SetDirtyFlag(bWasDirty);
		CurrentStatus = TEXT("仅调整手柄仍无法修复，原设计未改变。请定位问题后，拉缓路径或手动减小局部截面。");
		Notify(CurrentStatus, false); return false;
	}
}
