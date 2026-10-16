#include "MiningExternalRegion.h"
#include "MiningExternalGeometry.h"
#include "MiningChunkGeometry.h"
#include "GeneratedMiningRock.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMeshActor.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "UDynamicMesh.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "Editor.h"
#include "ScopedTransaction.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "StaticMeshCompiler.h"
#endif

using namespace UE::Geometry;

AMiningExternalRegion::AMiningExternalRegion()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RegionRoot")));
	BoundsPreview = CreateDefaultSubobject<UBoxComponent>(TEXT("RockBounds"));
	BoundsPreview->SetupAttachment(RootComponent);
	BoundsPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundsPreview->SetCanEverAffectNavigation(false);
	BoundsPreview->SetHiddenInGame(true);
	BoundsPreview->SetBoxExtent(RockSize * 0.5);
	BoundsPreview->ShapeColor = FColor(220, 160, 70);
}

void AMiningExternalRegion::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BoundsPreview->SetBoxExtent(RockSize.ComponentMax(FVector(100)) * 0.5);
	ApplyHardnessSettings();
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld()) RefreshPreview();
#endif
}

void AMiningExternalRegion::BeginPlay()
{
	Super::BeginPlay();ApplyHardnessSettings();
}

void AMiningExternalRegion::ApplyHardnessSettings()
{
	const auto Apply=[](AGeneratedMiningRock* Rock,int32 Hardness)
	{
		if(!IsValid(Rock) || Rock->HardnessLevel==Hardness)return;
#if WITH_EDITOR
		if(Rock->GetWorld() && !Rock->GetWorld()->IsGameWorld())Rock->Modify();
#endif
		Rock->HardnessLevel=FMath::Clamp(Hardness,0,4);
	};
	Apply(GeneratedRock,RockHardness);
	for(AGeneratedMiningRock* Guide:GeneratedGuideLayers)Apply(Guide,GuideHardness);
}

#if WITH_EDITOR
void AMiningExternalRegion::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	const FName Property = Event.MemberProperty ? Event.MemberProperty->GetFName() : NAME_None;
	if(Property==GET_MEMBER_NAME_CHECKED(AMiningExternalRegion,RockHardness) || Property==GET_MEMBER_NAME_CHECKED(AMiningExternalRegion,GuideHardness))
	{ApplyHardnessSettings();Super::PostEditChangeProperty(Event);return;}
	const bool bGuideProperty = Property == GET_MEMBER_NAME_CHECKED(AMiningExternalRegion, GuideLayers) ||
		Property == GET_MEMBER_NAME_CHECKED(AMiningExternalRegion, bGuideFollowCutterTransform) ||
		Property == GET_MEMBER_NAME_CHECKED(AMiningExternalRegion, GuideCutterIndex) ||
		Property == GET_MEMBER_NAME_CHECKED(AMiningExternalRegion, GuideTransform) ||
		Property == GET_MEMBER_NAME_CHECKED(AMiningExternalRegion, GuideMaterial);
	if (!bGuideProperty && IsValid(GeneratedRock)) BuildStatus = TEXT("参数已修改：请重新生成矿区，当前显示的是上一次成功生成结果。");
	if (!GeneratedGuideLayers.IsEmpty()) GuideBuildStatus = TEXT("配置已修改：请重新生成引导层，当前显示的是上一次生成结果。");
	Super::PostEditChangeProperty(Event);
}

void AMiningExternalRegion::RefreshPreview()
{
	TArray<UStaticMeshComponent*> Components;
	GetComponents(Components);
	for (UStaticMeshComponent* Component : Components)
		if (Component->ComponentHasTag(TEXT("ExternalCutterPreview"))) Component->DestroyComponent();
	if (!bShowCutterPreview) return;
	for (const FMiningExternalCutter& Cutter : Cutters)
	{
		if (!Cutter.bEnabled || !Cutter.Mesh) continue;
		UStaticMeshComponent* Preview = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Preview->ComponentTags.Add(TEXT("ExternalCutterPreview"));
		Preview->bIsEditorOnly = true;
		Preview->SetIsVisualizationComponent(true);
		Preview->SetStaticMesh(Cutter.Mesh);
		Preview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Preview->SetCanEverAffectNavigation(false);
		Preview->SetCastShadow(false);
		Preview->SetHiddenInGame(true);
		Preview->SetupAttachment(RootComponent);
		Preview->SetRelativeTransform(Cutter.LocalTransform);
		Preview->RegisterComponent();
	}
}

void AMiningExternalRegion::RemoveOwnedActor(AActor* Actor)
{
	if (!IsValid(Actor) || Actor->GetOwner() != this) return;
	Actor->Modify();
	GetWorld()->EditorDestroyActor(Actor, true);
}
#endif

bool AMiningExternalRegion::GenerateRegion()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld() || (GEditor && GEditor->PlayWorld)) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("ExternalMining", "Generate", "生成外部通路矿区"));
	Modify();
	const auto Fail = [this](const FString& Error) { BuildStatus = TEXT("未应用：") + Error; return false; };
	if (!GetActorScale3D().Equals(FVector::OneVector)) return Fail(TEXT("矿区 Actor 缩放须为 1，请使用岩层尺寸调整大小。"));
	if (!FMath::IsFinite(ChunkLength) || ChunkLength < 400 || ChunkLength > 3000 || RockSize.ContainsNaN())
		return Fail(TEXT("分块边长须为 400–3000 cm，岩层尺寸须为有效数值。"));
	FIntVector Counts;
	if (!MiningChunks::SpatialChunkCounts(RockSize, ChunkLength, Counts))
		return Fail(FString::Printf(TEXT("岩层尺寸无效或超过单矿区 %d 块预算，请增加分块边长或缩小矿区。"), MiningChunks::MaxSpatialChunks));
	if (Cutters.Num() > 32) return Fail(TEXT("单矿区最多 32 个切割体。"));
	TArray<FDynamicMesh3> Tools;
	int32 TotalToolTriangles = 0;
	for (int32 Index = 0; Index < Cutters.Num(); ++Index)
	{
		const FMiningExternalCutter& Cutter = Cutters[Index];
		if (!Cutter.bEnabled) continue;
		if (!Cutter.Mesh) return Fail(FString::Printf(TEXT("切割体 %d 未指定模型。"), Index + 1));
		if (!Cutter.LocalTransform.IsValid() || Cutter.LocalTransform.GetScale3D().GetMin() <= 0)
			return Fail(FString::Printf(TEXT("切割体 %d 变换无效；请使用正缩放。"), Index + 1));
		FStaticMeshCompilingManager::Get().FinishCompilation({Cutter.Mesh.Get()});
		const FMeshDescription* Description = Cutter.Mesh->GetMeshDescription(0);
		if (!Description) return Fail(TEXT("模型没有可读取的 LOD0 源网格，请重新导入 Static Mesh。"));
		FDynamicMesh3 Tool;
		FMeshDescriptionToDynamicMesh Converter;
		Converter.bDisableAttributes = true;
		Converter.Convert(Description, Tool);
		TotalToolTriangles += Tool.TriangleCount();
		if (TotalToolTriangles > 500000) return Fail(TEXT("切割体合计超过 500000 个三角形，请简化通路模型。"));
		for (int32 Vertex : Tool.VertexIndicesItr())
			Tool.SetVertex(Vertex, FVector3d(Cutter.LocalTransform.TransformPosition(FVector(Tool.GetVertex(Vertex)))));
		Tools.Add(MoveTemp(Tool));
	}
	FDynamicMesh3 Rock, Shell;
	FString Error;
	if (!MiningExternal::Build(RockSize, ShellThickness, Tools, Rock, Shell, Error)) return Fail(Error);
	FActorSpawnParameters Spawn;
	Spawn.Owner = this;
	Spawn.OverrideLevel = GetLevel();
	Spawn.ObjectFlags |= RF_Transactional;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGeneratedMiningRock* NewRock = GetWorld()->SpawnActor<AGeneratedMiningRock>(GetActorLocation(), GetActorRotation(), Spawn);
	if (!NewRock) return Fail(TEXT("创建可挖岩层失败。"));
	NewRock->RockMaterial = RockMaterial;
	NewRock->HardnessLevel = FMath::Clamp(RockHardness,0,4);
	NewRock->MaxTriangles = FMath::Clamp(ChunkTriangleBudget, 1000, MiningChunks::MaxChunkTriangles);
	NewRock->bSimplifyChunks = bSimplifyChunks;
	if (!NewRock->SetChunkedMesh(Rock, ChunkLength, true, true))
	{
		RemoveOwnedActor(NewRock);
		return Fail(TEXT("空间分块布尔运算失败，请调整分块边长或通路位置，避免相切和共面。原矿区保留。"));
	}
	if (NewRock->LargestChunkTriangles > NewRock->MaxTriangles)
	{
		const int32 Required = NewRock->LargestChunkTriangles;
		const int32 Budget = NewRock->MaxTriangles;
		RemoveOwnedActor(NewRock);
		return Fail(FString::Printf(TEXT("初始最大单块 %d 三角形，超过每块预算 %d；请提高预算（最高 %d）或减小分块边长。原矿区保留。"), Required, Budget, MiningChunks::MaxChunkTriangles));
	}
	ADynamicMeshActor* NewShell = GetWorld()->SpawnActor<ADynamicMeshActor>(GetActorLocation(), GetActorRotation(), Spawn);
	if (!NewShell) { RemoveOwnedActor(NewRock); return Fail(TEXT("创建不可挖外壳失败，原矿区保留。")); }
	UDynamicMeshComponent* ShellComponent = NewShell->GetDynamicMeshComponent();
	ShellComponent->SetCollisionProfileName(TEXT("BlockAll"));
	ShellComponent->SetComplexAsSimpleCollisionEnabled(true, false);
	ShellComponent->SetDeferredCollisionUpdatesEnabled(true, false);
	ShellComponent->bUseAsyncCooking = false;
	ShellComponent->SetGenerateOverlapEvents(false);
	ShellComponent->SetCanEverAffectNavigation(false);
	ShellComponent->SetMesh(MoveTemp(Shell));
	ShellComponent->GetBodySetup()->SetFlags(RF_Transient);
	ShellComponent->UpdateCollision(false);
	ShellComponent->SetMaterial(0, ShellMaterial);
	NewRock->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	NewShell->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	NewRock->SetActorLabel(GetActorLabel() + TEXT("_可挖岩层"));
	NewShell->SetActorLabel(GetActorLabel() + TEXT("_不可挖外壳"));
	NewRock->SetFolderPath(FName(TEXT("ExternalMining/Generated")));
	NewShell->SetFolderPath(FName(TEXT("ExternalMining/Generated")));
	RemoveOwnedActor(GeneratedRock);
	RemoveOwnedActor(GeneratedShell);
	GeneratedRock = NewRock;
	GeneratedShell = NewShell;
	MarkPackageDirty();
	BuildStatus = FString::Printf(TEXT("生成成功：%d 个切割体，%d 块岩层，最大单块 %d 三角形。基岩不可挖；切割体已统一作用于岩层和外壳。"),
		Tools.Num(), NewRock->Chunks.Num(), NewRock->LargestChunkTriangles);
	return true;
#else
	return false; // Shipping uses saved geometry; imported mesh source data is editor-only.
#endif
}

void AMiningExternalRegion::ClearGeneratedRegion()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld() || (GEditor && GEditor->PlayWorld)) return;
	const FScopedTransaction Transaction(NSLOCTEXT("ExternalMining", "Clear", "清除外部矿区生成结果"));
	Modify();
	RemoveOwnedActor(GeneratedRock);
	RemoveOwnedActor(GeneratedShell);
	GeneratedRock = nullptr;
	GeneratedShell = nullptr;
	for (AGeneratedMiningRock* Guide : GeneratedGuideLayers) RemoveOwnedActor(Guide);
	GeneratedGuideLayers.Reset();
	GuideBuildStatus = TEXT("已清除引导可挖层，阻塞块配置保留。");
	BuildStatus = TEXT("已清除本矿区生成结果，通路和引导层配置保留。");
	MarkPackageDirty();
#endif
}

void AMiningExternalRegion::ResetExcavation()
{
	if (IsValid(GeneratedRock) && GeneratedRock->GetOwner() == this) GeneratedRock->ResetRock();
	ResetGuideExcavation();
}

bool AMiningExternalRegion::GenerateGuideLayers()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld() || (GEditor && GEditor->PlayWorld)) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("ExternalMining", "GenerateGuides", "生成引导可挖层"));
	Modify();
	const auto Fail = [this](const FString& Error) { GuideBuildStatus = TEXT("引导层未应用：") + Error; return false; };
	if (!GetActorScale3D().Equals(FVector::OneVector)) return Fail(TEXT("矿区 Actor 缩放须为 1。"));
	if (GuideLayers.IsEmpty() || GuideLayers.Num() > 128) return Fail(TEXT("请添加 1–128 个阻塞块模型。"));
	FTransform Shared = GuideTransform;
	if (bGuideFollowCutterTransform)
	{
		if (!Cutters.IsValidIndex(GuideCutterIndex)) return Fail(TEXT("对应切割体索引无效；请指定通路，或关闭跟随并设置共用变换。"));
		Shared = Cutters[GuideCutterIndex].LocalTransform;
	}
	if (!Shared.IsValid() || Shared.GetScale3D().GetMin() <= 0) return Fail(TEXT("共用引导层变换须为有效数值和正缩放。"));
	TArray<FDynamicMesh3> Geometry;
	TArray<int32> Indices;
	int32 TotalTriangles = 0;
	for (int32 Index = 0; Index < GuideLayers.Num(); ++Index)
	{
		const FMiningExternalGuideLayer& Layer = GuideLayers[Index];
		if (!Layer.bEnabled) continue;
		if (!Layer.Mesh || !Layer.LocalTransform.IsValid() || Layer.LocalTransform.GetScale3D().GetMin() <= 0)
			return Fail(FString::Printf(TEXT("阻塞块 %d 缺少模型或附加变换无效。原引导层保留。"), Index + 1));
		FStaticMeshCompilingManager::Get().FinishCompilation({Layer.Mesh.Get()});
		const FMeshDescription* Description = Layer.Mesh->GetMeshDescription(0);
		if (!Description) return Fail(FString::Printf(TEXT("阻塞块 %d 没有 LOD0 源网格。"), Index + 1));
		FDynamicMesh3 Mesh;
		FMeshDescriptionToDynamicMesh Converter;
		Converter.bDisableAttributes = true;
		Converter.Convert(Description, Mesh);
		TotalTriangles += Mesh.TriangleCount();
		if (TotalTriangles > 500000) return Fail(TEXT("阻塞模型合计超过 500000 三角形。原引导层保留。"));
		for (int32 Vertex : Mesh.VertexIndicesItr())
			Mesh.SetVertex(Vertex, FVector3d(Shared.TransformPosition(Layer.LocalTransform.TransformPosition(FVector(Mesh.GetVertex(Vertex))))));
		FString Error;
		if (!MiningExternal::PrepareCutter(Mesh, Error)) return Fail(FString::Printf(TEXT("阻塞块 %d：%s 原引导层保留。"), Index + 1, *Error));
		MiningChunks::Normals(Mesh);
		Geometry.Add(MoveTemp(Mesh));
		Indices.Add(Index);
	}
	if (Geometry.IsEmpty()) return Fail(TEXT("没有启用的阻塞块；如需移除，请使用清除引导层。"));
	TArray<TObjectPtr<AGeneratedMiningRock>> NewGuides;
	const auto Rollback = [this, &NewGuides, &Fail](const FString& Error)
	{
		for (AGeneratedMiningRock* Guide : NewGuides) RemoveOwnedActor(Guide);
		return Fail(Error + TEXT(" 原引导层保留。"));
	};
	for (int32 Index = 0; Index < Geometry.Num(); ++Index)
	{
		FActorSpawnParameters Spawn;
		Spawn.Owner = this; Spawn.OverrideLevel = GetLevel(); Spawn.ObjectFlags |= RF_Transactional;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGeneratedMiningRock* Guide = GetWorld()->SpawnActor<AGeneratedMiningRock>(GetActorLocation(), GetActorRotation(), Spawn);
		if (!Guide) return Rollback(TEXT("创建引导层 Actor 失败。"));
		NewGuides.Add(Guide);
		Guide->MaxTriangles = FMath::Clamp(ChunkTriangleBudget, 1000, MiningChunks::MaxChunkTriangles);
		Guide->bSimplifyChunks = bSimplifyChunks;
		UStaticMesh* Model = GuideLayers[Indices[Index]].Mesh;
		Guide->RockMaterial = GuideMaterial ? GuideMaterial.Get() : (Model->GetMaterial(0) ? Model->GetMaterial(0) : RockMaterial.Get());
		Guide->HardnessLevel = FMath::Clamp(GuideHardness,0,4);
		if (!Guide->SetChunkedMesh(Geometry[Index], ChunkLength, true, true))
			return Rollback(FString::Printf(TEXT("阻塞块 %d 分块失败，请检查分块边长或模型。"), Indices[Index] + 1));
		if (Guide->LargestChunkTriangles > Guide->MaxTriangles)
			return Rollback(FString::Printf(TEXT("阻塞块 %d 最大单块 %d 三角形超过预算 %d。"), Indices[Index] + 1, Guide->LargestChunkTriangles, Guide->MaxTriangles));
		Guide->Tags.Add(FName(TEXT("ExternalMiningGuideLayer")));
		Guide->SetActorLabel(FString::Printf(TEXT("%s_引导可挖层_%02d"), *GetActorLabel(), Indices[Index] + 1));
		Guide->SetFolderPath(FName(TEXT("ExternalMining/Generated/Guides")));
		Guide->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	}
	for (AGeneratedMiningRock* Guide : GeneratedGuideLayers) RemoveOwnedActor(Guide);
	GeneratedGuideLayers = MoveTemp(NewGuides);
	int32 Chunks = 0;
	for (AGeneratedMiningRock* Guide : GeneratedGuideLayers) Chunks += Guide->Chunks.Num();
	GuideBuildStatus = FString::Printf(TEXT("引导层生成成功：%d 个独立可挖阻塞块，共 %d 个分块；R 可恢复初始阻塞层。"), GeneratedGuideLayers.Num(), Chunks);
	MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

void AMiningExternalRegion::ClearGeneratedGuideLayers()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld() || (GEditor && GEditor->PlayWorld)) return;
	const FScopedTransaction Transaction(NSLOCTEXT("ExternalMining", "ClearGuides", "清除引导可挖层"));
	Modify();
	for (AGeneratedMiningRock* Guide : GeneratedGuideLayers) RemoveOwnedActor(Guide);
	GeneratedGuideLayers.Reset();
	GuideBuildStatus = TEXT("已清除引导可挖层，阻塞块配置保留。");
	MarkPackageDirty();
#endif
}

void AMiningExternalRegion::ResetGuideExcavation()
{
	for (AGeneratedMiningRock* Guide : GeneratedGuideLayers)
		if (IsValid(Guide) && Guide->GetOwner() == this) Guide->ResetRock();
}

void AMiningExternalRegion::Destroyed()
{
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
#if WITH_EDITOR
		RemoveOwnedActor(GeneratedRock);
		RemoveOwnedActor(GeneratedShell);
		for (AGeneratedMiningRock* Guide : GeneratedGuideLayers) RemoveOwnedActor(Guide);
#endif
	}
	Super::Destroyed();
}
