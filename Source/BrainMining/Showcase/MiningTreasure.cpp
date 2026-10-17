#include "Showcase/MiningTreasure.h"
#include "Components/DecalComponent.h"
#include "MiningInteractionTrace.h"
#include "MiningAudioComponent.h"
#include "MiningWalletSubsystem.h"
#include "BrainMiningPlayerController.h"
#include "MiningInteractionHUDWidget.h"
#include "Engine/GameInstance.h"
#include "DiggableRock.h"
#include "GeneratedMiningRock.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "StaticMeshResources.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Spatial/FastWinding.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "UDynamicMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

// Each dynamic mesh broadcasts replacements, edits, resets and background simplification
// commits. Invalidate from that event rather than from cut counts (which can reset).
class FMiningTerrainQuery
{
public:
    struct FEntry : TSharedFromThis<FEntry>
    {
        TWeakObjectPtr<UDynamicMesh> Source;
        FDelegateHandle Changed;
        uint32 Revision = 1;
        TUniquePtr<UE::Geometry::FDynamicMeshAABBTree3> Tree;
        TUniquePtr<UE::Geometry::TFastWindingTree<UE::Geometry::FDynamicMesh3>> Winding;
        ~FEntry() { if(Source.IsValid()) Source->OnMeshChanged().Remove(Changed); }
        void Invalidate(UDynamicMesh*, FDynamicMeshChangeInfo)
        { Winding.Reset(); Tree.Reset(); ++Revision; }
        bool Contains(const FVector& P)
        {
            const auto& Mesh=Source->GetMeshRef();
            if(!Mesh.TriangleCount()) return false;
            if(!Tree)
            {
                Tree=MakeUnique<UE::Geometry::FDynamicMeshAABBTree3>(&Mesh,true);
                Winding=MakeUnique<UE::Geometry::TFastWindingTree<UE::Geometry::FDynamicMesh3>>(Tree.Get());
            }
            return FMath::Abs(Winding->FastWindingNumber(P))>=.5;
        }
    };
    TMap<TWeakObjectPtr<UDynamicMesh>, TSharedPtr<FEntry>> Entries;
    FEntry& Get(UDynamicMesh* Mesh)
    {
        if (const auto* Entry = Entries.Find(Mesh)) return **Entry;
        for(auto It=Entries.CreateIterator();It;++It) if(!It.Key().IsValid()) It.RemoveCurrent();
        auto& Entry=Entries.FindOrAdd(Mesh);
        if(!Entry)
        {
            // Editor rebuilds can replace components; do not retain their old spatial trees.
            Entry=MakeShared<FEntry>();Entry->Source=Mesh;
            Entry->Changed=Mesh->OnMeshChanged().AddSP(Entry.ToSharedRef(),&FEntry::Invalidate);
        }
        return *Entry;
    }
};

namespace
{
FMiningTerrainQuery& TerrainQuery(ADiggableRock* Rock)
{
    if(!Rock->TerrainQuery) Rock->TerrainQuery=MakeShared<FMiningTerrainQuery>();
    return *Rock->TerrainQuery;
}
}

uint32 AMiningTreasure::GetTerrainVersion() const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Mining_TerrainVersion);
    FBox WorldBounds = CrystalSupportBounds;
    // Keep monitoring the buried site after the reward falls or is collected.
    WorldBounds+=BuriedTransform.GetLocation()-Model->Bounds.BoxExtent*2;
    WorldBounds+=BuriedTransform.GetLocation()+Model->Bounds.BoxExtent*2;
    uint32 Version=1;
    for(ADiggableRock* Rock:CoveringRocks)
    {
        if(!IsValid(Rock)) {Version=HashCombineFast(Version,0);continue;}
        const FBox Bounds=WorldBounds.TransformBy(Rock->GetActorTransform().ToInverseMatrixWithScale());
        Version=HashCombineFast(Version,GetTypeHash(Rock->GetActorLocation()));
        Version=HashCombineFast(Version,GetTypeHash(Rock->GetActorRotation().Euler()));
        Version=HashCombineFast(Version,GetTypeHash(Rock->GetActorScale3D()));
        auto Add=[&](UDynamicMesh* Mesh)
        { if(Mesh) Version=HashCombineFast(Version,HashCombineFast(GetTypeHash(Mesh),TerrainQuery(Rock).Get(Mesh).Revision)); };
        if(auto* Generated=Cast<AGeneratedMiningRock>(Rock);Generated && Generated->bChunked)
        { for(const auto& Chunk:Generated->Chunks) if(Chunk.Bounds.Intersect(Bounds)) Add(Generated->GetChunkQueryMesh(Chunk)); }
        else if (Rock->RockMesh) Add(Rock->RockMesh->GetDynamicMesh());
    }
    return Version;
}

TArray<int32> FMiningTreasureDefinition::DefaultValues(FName TreasureId)
{
    const TArray<TArray<int32>> Values = {{2,3,4,5}, {8,10,15,20}, {40,60,80,100}, {200,300,400,500},
        {750,1000,1500,2000}, {4000,6000,8000,10000}, {15000,25000,35000,50000},
        {75000,125000,175000,250000}, {400000,600000,800000,1000000}};
    for (int32 Index = 0; Index < Values.Num(); ++Index)
        if (TreasureId == FName(*FString::FromInt(Index + 1))) return Values[Index];
    return {};
}

bool FMiningTreasureDefinition::HasValidSizeRules() const
{
    if (ValuesBySize.Num() != 4 || SizeMultipliers.Num() != 4) return false;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        if (ValuesBySize[Index] <= 0 || !FMath::IsFinite(SizeMultipliers[Index]) || SizeMultipliers[Index] <= 0) return false;
        if (Index > 0 && (ValuesBySize[Index] <= ValuesBySize[Index-1] || SizeMultipliers[Index] <= SizeMultipliers[Index-1])) return false;
    }
    return true;
}

bool FMiningTreasureDefinition::IsEligible(FName Region, bool bGuide) const
{
    return !Id.IsNone() && Model && FMath::IsFinite(Weight) && Weight > 0 && HasValidSizeRules() &&
        !Scale.ContainsNaN() && Scale.GetMin() > 0 && Regions.Contains(Region) && (bGuide ? bGuideLayer : bNormalLayer);
}

AMiningTreasure::AMiningTreasure()
{
    DropBody = CreateDefaultSubobject<UBoxComponent>(TEXT("DropBody"));
    SetRootComponent(DropBody);
    DropBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DropBody->SetCollisionObjectType(ECC_PhysicsBody);
    DropBody->SetCollisionResponseToAllChannels(ECR_Ignore);
    DropBody->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    DropBody->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    DropBody->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
    DropBody->SetGenerateOverlapEvents(false);
    DropBody->SetCanEverAffectNavigation(false);
    DropBody->SetUseCCD(true);
    DropBody->SetLinearDamping(.8f);
    DropBody->SetAngularDamping(2.f);
    Model = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TreasureModel"));
    Model->SetupAttachment(DropBody);
    Model->BodyInstance.bAutoWeld = false;
    Model->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Model->SetCollisionResponseToAllChannels(ECR_Ignore);
    Model->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Model->SetGenerateOverlapEvents(false);
    Model->SetCanEverAffectNavigation(false);
    Model->SetCullDistance(3000);
    GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ExposedGemLight"));
    GlowLight->SetupAttachment(Model);
    GlowLight->SetIntensityUnits(ELightUnits::Lumens);
    GlowLight->SetIntensity(28);
    GlowLight->SetAttenuationRadius(560);
    GlowLight->SetCastShadows(false);
    GlowLight->SetVisibility(false);
    GlowLight->IndirectLightingIntensity = 0;
    BuriedGlow=CreateDefaultSubobject<UDecalComponent>(TEXT("BuriedMineralGlow"));
    BuriedGlow->SetupAttachment(Model);BuriedGlow->SetVisibility(false);
    BuriedGlow->SetFadeScreenSize(0.f);
    CrystalClues = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("EmbeddedCrystalClues"));
    CrystalClues->SetupAttachment(DropBody);
    // Rock clues stay anchored to the terrain when the gem starts simulating physics.
    CrystalClues->SetAbsolute(true,true,true);
    CrystalClues->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CrystalClues->SetCastShadow(false);
    CrystalClues->SetCanEverAffectNavigation(false);
    CrystalClues->SetCullDistances(2800,3000);
}

float AMiningTreasure::ValueScale(const FMiningTreasureDefinition& Definition, int32 InValue)
{
    const int32 Tier=Definition.ValuesBySize.IndexOfByKey(InValue);
    return Definition.SizeMultipliers.IsValidIndex(Tier)?Definition.SizeMultipliers[Tier]:1.f;
}

bool AMiningTreasure::Configure(const FMiningTreasureDefinition& Definition, int32 InSizeTier, UMaterialInterface* Material)
{
    if (!Definition.HasValidSizeRules() || !Definition.ValuesBySize.IsValidIndex(InSizeTier)) return false;
    TreasureId = Definition.Id;
    TreasureName = Definition.Name;
    SizeTier = InSizeTier;
    Value = Definition.ValuesBySize[SizeTier];
    const FVector ModelScale = Definition.Scale * Definition.SizeMultipliers[SizeTier];
    BaseColor = Definition.Color;
    Model->SetStaticMesh(Definition.Model);
    Model->SetRelativeScale3D(ModelScale);
    if (Definition.Model)
    {
        const auto Bounds = Definition.Model->GetBounds();
        const FVector CenterOffset = Bounds.Origin * ModelScale;
        AddActorWorldOffset(GetActorQuat().RotateVector(Model->GetRelativeLocation() + CenterOffset));
        Model->SetRelativeLocation(-CenterOffset);
        DropBody->SetBoxExtent(Bounds.BoxExtent * ModelScale);
        auto* MaterialPhysics = NewObject<UPhysicalMaterial>(this);
        MaterialPhysics->Friction = .7f;
        MaterialPhysics->Restitution = .25f;
        DropBody->SetPhysMaterialOverride(MaterialPhysics);
    }
    BuriedTransform = GetActorTransform();
    if (Material)
    {
        auto* Tint = UMaterialInstanceDynamic::Create(Material, this);
        Tint->SetVectorParameterValue(TEXT("Tint"), Definition.Color);
        Tint->SetScalarParameterValue(TEXT("Emission"), Definition.EmissiveStrength);
        Tint->SetScalarParameterValue(TEXT("Roughness"), Definition.Roughness);
        Tint->SetScalarParameterValue(TEXT("Metallic"), Definition.Metallic);
        Tint->SetScalarParameterValue(TEXT("Style"), Definition.SurfaceStyle);
        Model->SetMaterial(0, Tint);
    }
    SurfaceSamples.Reset();
    GlowLight->SetLightColor(Definition.HintColor);
    const auto* RenderData = Definition.Model ? Definition.Model->GetRenderData() : nullptr;
    if (Definition.Model && Definition.Model->bAllowCPUAccess && RenderData && !RenderData->LODResources.IsEmpty())
    {
        const auto& LOD = RenderData->LODResources[0];
        const auto& Positions = LOD.VertexBuffers.PositionVertexBuffer;
        TArray<double> Areas;
        double TotalArea = 0;
        for (int32 Offset = 0; Offset + 2 < LOD.IndexBuffer.GetNumIndices(); Offset += 3)
        {
            const FVector First = FVector(Positions.VertexPosition(LOD.IndexBuffer.GetIndex(Offset)));
            const FVector Second = FVector(Positions.VertexPosition(LOD.IndexBuffer.GetIndex(Offset+1)));
            const FVector Third = FVector(Positions.VertexPosition(LOD.IndexBuffer.GetIndex(Offset+2)));
            TotalArea += FVector::CrossProduct((Second-First)*ModelScale, (Third-First)*ModelScale).Size()*.5;
            Areas.Add(TotalArea);
        }
        FRandomStream Samples(4701);
        for (int32 Attempt = 0; Attempt < 3072 && SurfaceSamples.Num() < 384 && TotalArea > 0; ++Attempt)
        {
            const double SelectedArea = Samples.FRand()*TotalArea;
            int32 Triangle = 0;
            while (Triangle+1 < Areas.Num() && Areas[Triangle] < SelectedArea) ++Triangle;
            const uint32 Vertex = LOD.IndexBuffer.GetIndex(Triangle*3);
            const FVector First(Positions.VertexPosition(Vertex));
            const FVector Second(Positions.VertexPosition(LOD.IndexBuffer.GetIndex(Triangle*3+1)));
            const FVector Third(Positions.VertexPosition(LOD.IndexBuffer.GetIndex(Triangle*3+2)));
            const double Root = FMath::Sqrt(Samples.FRand());
            const double Blend = Samples.FRand();
            const FVector Sample = First*(1-Root) + Second*(Root*(1-Blend)) + Third*(Root*Blend);
            const FVector WorldPoint = Model->GetComponentTransform().TransformPosition(Sample);
            const FVector Normal = Model->GetComponentTransform().TransformVectorNoScale(FVector(LOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(Vertex))).GetSafeNormal();
            FHitResult Hit;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(TreasureExternalSurface), true);
            if (Model->LineTraceComponent(Hit, WorldPoint+Normal*(Model->Bounds.SphereRadius*3), WorldPoint-Normal*.2, Query) && FVector::DistSquared(Hit.ImpactPoint, WorldPoint) < 1)
                SurfaceSamples.Add(Sample);
        }
    }
    if (!SurfaceSamples.IsEmpty()) return true;
    const FVector Center = Model->Bounds.Origin;
    const double Radius = Model->Bounds.SphereRadius * 2 + 1;
    for (int32 Index = 0; Index < 128; ++Index)
    {
        const double Height = 1 - 2 * (Index + .5) / 128;
        const double Angle = Index * 2.399963229728653;
        const double Ring = FMath::Sqrt(1 - Height * Height);
        const FVector Direction(Ring * FMath::Cos(Angle), Ring * FMath::Sin(Angle), Height);
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(TreasureSurface), true);
        if (Model->LineTraceComponent(Hit, Center + Direction * Radius, Center - Direction * Radius, Query))
            SurfaceSamples.Add(Model->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint));
    }
    return !SurfaceSamples.IsEmpty();
}

float AMiningTreasure::MeasureExposure(FVector* EscapeDirection) const
{
    if (bReleased) return 1;
    if (SurfaceSamples.IsEmpty()) return 0;
    const uint32 Version=GetTerrainVersion();
    const FTransform Transform=Model->GetComponentTransform();
    if(bExposureCached && ExposureVersion==Version && ExposureTransform.Equals(Transform))
    { if(EscapeDirection) *EscapeDirection=CachedEscapeDirection; return CachedExposure; }
    TArray<FVector> Points;for(const FVector& P:SurfaceSamples)Points.Add(Model->GetComponentTransform().TransformPosition(P));
    TArray<bool> Covered;MarkCoveredPoints(CoveringRocks,Points,Covered);
    int32 FreeCount = 0;
    FVector FreeDirection = FVector::ZeroVector;
    for (int32 Index = 0; Index < Covered.Num(); ++Index)
        if (!Covered[Index])
        {
            ++FreeCount;
            FreeDirection += (Model->GetComponentTransform().TransformPosition(SurfaceSamples[Index]) - Model->Bounds.Origin).GetSafeNormal();
        }
    CachedEscapeDirection=FreeDirection.GetSafeNormal();
    CachedExposure=float(FreeCount)/SurfaceSamples.Num();
    ExposureVersion=Version;ExposureTransform=Transform;bExposureCached=true;
    if(EscapeDirection) *EscapeDirection=CachedEscapeDirection;
    return CachedExposure;
}

void AMiningTreasure::MarkCoveredPoints(const TArray<TObjectPtr<ADiggableRock>>& Rocks,const TArray<FVector>& Points,TArray<bool>& Covered)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Mining_Coverage);
    Covered.Init(false,Points.Num());
    if(Points.IsEmpty())return;
    for(ADiggableRock* Rock:Rocks)
    {
        if(!IsValid(Rock))continue;
        TArray<FVector> Local;FBox QueryBounds(ForceInit);
        for(const FVector& P:Points){const FVector L=Rock->GetActorTransform().InverseTransformPosition(P);Local.Add(L);QueryBounds+=L;}
        auto Mark=[&](UDynamicMesh* QueryMesh,const FBox& Bounds)
        {
            if(!QueryMesh || !Bounds.Intersect(QueryBounds))return;
            const auto& Mesh=QueryMesh->GetMeshRef();if(!Mesh.TriangleCount())return;
            auto& Query=TerrainQuery(Rock).Get(QueryMesh);
            for(int I=0;I<Local.Num();++I)if(!Covered[I] && Bounds.IsInsideOrOn(Local[I]))
                Covered[I]=Query.Contains(Local[I]);
        };
        if(auto* Generated=Cast<AGeneratedMiningRock>(Rock);Generated && Generated->bChunked)
        {for(const auto& Chunk:Generated->Chunks)Mark(Generated->GetChunkQueryMesh(Chunk),Chunk.Bounds);}
        else if(Rock->RockMesh)
        {const auto Bounds=Rock->RockMesh->GetDynamicMesh()->GetMeshRef().GetBounds();Mark(Rock->RockMesh->GetDynamicMesh(),FBox(FVector(Bounds.Min),FVector(Bounds.Max)));}
    }
}

void AMiningTreasure::ConfigureCrystalClues(const FMiningTreasurePlacement& Placement, UStaticMesh* FragmentMesh)
{
    CrystalClues->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    CrystalClues->SetWorldTransform(FTransform::Identity);
    CrystalClues->SetStaticMesh(FragmentMesh);
    auto* GemMaterial=Cast<UMaterialInstanceDynamic>(Model->GetMaterial(0));
    auto* Tint=UMaterialInstanceDynamic::Create(GemMaterial?GemMaterial->Parent.Get():Model->GetMaterial(0),this);
    if(GemMaterial)Tint->CopyParameterOverrides(GemMaterial);
    // Tiny inclusions have no point light of their own. Preserve hue but lift their
    // value so the same mineral remains readable on an unlit cave wall.
    FLinearColor ClueColor=BaseColor/FMath::Max(.35f,FMath::Max3(BaseColor.R,BaseColor.G,BaseColor.B));
    // Coal keeps a warm mineral glint; tin reads as cool silver-blue instead of grey.
    if(TreasureId==TEXT("1")) ClueColor=FLinearColor(.62f,.30f,.10f);
    if(TreasureId==TEXT("3")) ClueColor=FLinearColor(.25f,.65f,1.f);
    // Iron uses a dark hematite red so it remains distinct from diamond.
    const bool bIron = TreasureId == TEXT("4");
    if(bIron) ClueColor=FLinearColor(.62f,.055f,.035f);
    ClueColor.A=1;
    Tint->SetVectorParameterValue(TEXT("Tint"),ClueColor);
    Tint->SetScalarParameterValue(TEXT("Style"),-1);
    Tint->SetScalarParameterValue(TEXT("Roughness"),bIron ? .58f : .19f);
    Tint->SetScalarParameterValue(TEXT("Metallic"),.12f);
    const float ClueEmission=bIron ? .7f : FMath::Max(1.15f,GemMaterial?GemMaterial->K2_GetScalarParameterValue(TEXT("Emission"))*1.2f:1.15f);
    Tint->SetScalarParameterValue(TEXT("Emission"),ClueEmission);
    CrystalClueColor=ClueColor; CrystalClueEmission=ClueEmission; CrystalClueBrightness=1.f;
    CrystalClueMaterial=Tint;
    CrystalClues->SetMaterial(0,Tint);
    CrystalTransforms=Placement.Fragments;CrystalSupports=Placement.FragmentSupports;
    for(ADiggableRock* Rock:CoveringRocks) if(IsValid(Rock))
    {
        Rock->OnTerrainCommitted.RemoveAll(this);
        Rock->OnTerrainCommitted.AddUObject(this,&AMiningTreasure::OnTerrainCommitted);
    }
    UpdateCrystalClues();
}

void AMiningTreasure::UpdateCrystalClues()
{
    CrystalSupportBounds = FBox(ForceInit);
    for (const FVector& Point : CrystalSupports) CrystalSupportBounds += Point;
    OnTerrainCommitted(FBox(ForceInit));
}

void AMiningTreasure::OnTerrainCommitted(const FBox& ChangedBounds)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Mining_CrystalSupport);
    if(CrystalSupports.IsEmpty())return;
    if (ChangedBounds.IsValid && CrystalSupportBounds.IsValid && !ChangedBounds.Intersect(CrystalSupportBounds)) return;
    if(SupportedCrystals.Num()!=CrystalSupports.Num())SupportedCrystals.Init(false,CrystalSupports.Num());
    TArray<FVector> Points;
    TArray<int32> Fragments;
    for(int32 I=0;I<CrystalSupports.Num();++I)
        if(!ChangedBounds.IsValid || (SupportedCrystals[I] && ChangedBounds.IsInsideOrOn(CrystalSupports[I])))
        { Points.Add(CrystalSupports[I]); Fragments.Add(I); }
    if(Points.IsEmpty())return;
    TArray<bool> Covered;MarkCoveredPoints(CoveringRocks,Points,Covered);
    TArray<int32> Added;
    for(int32 I=0;I<Fragments.Num();++I)
    {
        const int32 Fragment=Fragments[I];
        // Subtraction cannot create solid support. Only initialization/reset may
        // restore a removed inclusion, even near winding-number tolerances.
        const bool bSupported=Covered[I] && (!ChangedBounds.IsValid || SupportedCrystals[Fragment]);
        if(bSupported && !SupportedCrystals[Fragment])Added.Add(Fragment);
        SupportedCrystals[Fragment]=bSupported;
    }
    // Remove back-to-front, matching whichever removal policy the ISM uses.
    // No ClearInstances: unaffected instance identities/render history survive.
    TArray<int32> RemovedSlots;
    for(int32 Slot=CrystalInstanceFragments.Num()-1;Slot>=0;--Slot)
        if(!SupportedCrystals[CrystalInstanceFragments[Slot]])
            RemovedSlots.Add(Slot);
    if (CrystalClues->RemoveInstances(RemovedSlots, true))
        for (const int32 Slot : RemovedSlots)
        {
            if(CrystalClues->SupportsRemoveSwap())CrystalInstanceFragments.RemoveAtSwap(Slot);
            else CrystalInstanceFragments.RemoveAt(Slot);
        }
    for(int32 Fragment:Added)if(CrystalTransforms.IsValidIndex(Fragment))
    {
        CrystalClues->AddInstance(CrystalTransforms[Fragment],true);
        CrystalInstanceFragments.Add(Fragment);
    }
}

bool AMiningTreasure::TryRelease()
{
    if (bReleased || bCollected || !GetWorld()) return false;
    FVector EscapeDirection;
    Exposure = MeasureExposure(&EscapeDirection);
    if (!IsExposureEligible(Exposure)) return false;
    FVector TowardPlayer = EscapeDirection;
    if (auto* Controller = GetWorld()->GetFirstPlayerController(); Controller && Controller->GetPawn())
        TowardPlayer = (Controller->GetPawn()->GetPawnViewLocation() - GetActorLocation()).GetSafeNormal();
    const FVector Directions[] = {EscapeDirection.IsNearlyZero() ? FVector::UpVector : EscapeDirection, TowardPlayer, FVector::UpVector};
    FCollisionQueryParams Query(SCENE_QUERY_STAT(TreasureDropClearance), true, this);
    if (auto* Controller = GetWorld()->GetFirstPlayerController()) Query.AddIgnoredActor(Controller->GetPawn());
    const auto Shape = FCollisionShape::MakeBox(DropBody->GetScaledBoxExtent() + FVector(1));
    const float MaxOffset = FMath::Clamp(float(Model->Bounds.SphereRadius * 2 + 20), 40.f, 180.f);
    for (float Distance = 0; Distance <= MaxOffset; Distance += 5)
        for (const FVector& Direction : Directions)
        {
            const FVector Candidate = GetActorLocation() + Direction * Distance;
            if (GetWorld()->OverlapBlockingTestByChannel(Candidate, GetActorQuat(), ECC_PhysicsBody, Shape, Query)) continue;
            SetActorLocation(Candidate, false, nullptr, ETeleportType::TeleportPhysics);
            DropBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            DropBody->SetSimulatePhysics(true);
            if (!DropBody->IsSimulatingPhysics())
            {
                DropBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                SetActorTransform(BuriedTransform, false, nullptr, ETeleportType::TeleportPhysics);
                return false;
            }
            bReleased = true;
            Exposure = 1;
            DropBody->SetPhysicsLinearVelocity(Direction * 90 + FVector(0, 0, 100));
            DropBody->SetPhysicsAngularVelocityInDegrees(FVector(45, 70, 25));
            return true;
        }
    return false;
}

void AMiningTreasure::SetDropActive(bool bActive)
{
    if (!bReleased) return;
    if ((!bActive || bCollected) && !bDropPaused)
    {
        PausedVelocity = DropBody->GetPhysicsLinearVelocity();
        PausedSpin = DropBody->GetPhysicsAngularVelocityInDegrees();
        DropBody->SetSimulatePhysics(false);
        bDropPaused = true;
    }
    else if (bActive && !bCollected && bDropPaused)
    {
        DropBody->SetSimulatePhysics(true);
        DropBody->SetPhysicsLinearVelocity(PausedVelocity);
        DropBody->SetPhysicsAngularVelocityInDegrees(PausedSpin);
        bDropPaused = false;
    }
}

void AMiningTreasure::ResetDrop()
{
    if (!bReleased || bCollected) return;
    DropBody->SetSimulatePhysics(false);
    DropBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetActorTransform(BuriedTransform, false, nullptr, ETeleportType::TeleportPhysics);
    bReleased = bDropPaused = false;
    PausedVelocity = PausedSpin = FVector::ZeroVector;
    Exposure = MeasureExposure();
    HintAmount = BuriedHintAmount;
}

void AMiningTreasure::SetHighlighted(bool bEnabled)
{
    Model->SetRenderCustomDepth(bEnabled && !bCollected);
    Model->SetCustomDepthStencilValue(1);
    if (auto* Tint = Cast<UMaterialInstanceDynamic>(Model->GetMaterial(0)))
        Tint->SetScalarParameterValue(TEXT("Highlight"), bEnabled && !bCollected ? 1.f : 0.f);
}

void AMiningTreasure::AttachBuriedHint(const FMiningTreasureDefinition& Definition)
{
    HintMaterials.Reset();
    HintAmount = Definition.bBuriedHint ? 1.f : 0.f;
    BuriedHintAmount = HintAmount;
    for (ADiggableRock* Rock : CoveringRocks)
    {
        if (!IsValid(Rock) || !Rock->RockMaterial) continue;
        auto* Existing = Cast<UMaterialInstanceDynamic>(Rock->RockMaterial);
        auto* Hint = UMaterialInstanceDynamic::Create(Existing ? Existing->Parent.Get() : Rock->RockMaterial.Get(), this);
        if (Existing) Hint->CopyParameterOverrides(Existing);
        Hint->SetVectorParameterValue(TEXT("HintCenter"), FLinearColor(GetActorLocation()));
        Hint->SetVectorParameterValue(TEXT("HintColor"), Definition.HintColor);
        // The previous tuning already doubled the authoring radius.  Double
        // it once more so the buried mineral signal remains readable through
        // the thicker route layers in MainGame.
        Hint->SetScalarParameterValue(TEXT("HintRadius"), Definition.HintRadius * 4.f);
        Hint->SetScalarParameterValue(TEXT("HintStrength"), Definition.HintStrength);
        Hint->SetScalarParameterValue(TEXT("HintAmount"), HintAmount);
        Rock->RockMaterial = Hint;
        Rock->RockMesh->SetMaterial(0, Hint);
        HintMaterials.Add(Hint);
    }
}

void AMiningTreasure::AttachCampaignHint(const FMiningTreasureDefinition& Definition)
{
    auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MainGame/Materials/M_BuriedMineralGlow.M_BuriedMineralGlow"));
    if(!Parent)return;
    const float Radius=Definition.HintRadius*4.f;
    BuriedGlow->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    BuriedGlow->SetWorldTransform(FTransform(FQuat::Identity,BuriedTransform.GetLocation()));
    BuriedGlow->DecalSize=FVector(Radius);
    auto* Hint=UMaterialInstanceDynamic::Create(Parent,this);
    Hint->SetVectorParameterValue(TEXT("HintCenter"),FLinearColor(BuriedTransform.GetLocation()));
    Hint->SetVectorParameterValue(TEXT("HintColor"),Definition.HintColor);
    Hint->SetScalarParameterValue(TEXT("HintRadius"),Radius);
    Hint->SetScalarParameterValue(TEXT("HintStrength"),Definition.HintStrength);
    HintAmount=BuriedHintAmount=Definition.bBuriedHint?1.f:0.f;
    Hint->SetScalarParameterValue(TEXT("HintAmount"),HintAmount);
    BuriedGlow->SetDecalMaterial(Hint);HintMaterials.Add(Hint);
}

void AMiningTreasure::UpdateGlow(float DeltaSeconds, bool bAllowLight)
{
    if (bCollected || bReleased) HintAmount = FMath::Max(0.f, HintAmount-DeltaSeconds*4);
    for (UMaterialInstanceDynamic* Hint : HintMaterials) if (IsValid(Hint)) Hint->SetScalarParameterValue(TEXT("HintAmount"), HintAmount);
    if (bCollected)
    {
        CrystalClueBrightness=FMath::Max(0.f,CrystalClueBrightness-DeltaSeconds*.85f);
        if (IsValid(CrystalClueMaterial))
        {
            const FLinearColor Dimmed=FMath::Lerp(FLinearColor(.018f,.012f,.01f),CrystalClueColor,CrystalClueBrightness);
            CrystalClueMaterial->SetVectorParameterValue(TEXT("Tint"),Dimmed);
            CrystalClueMaterial->SetScalarParameterValue(TEXT("Emission"),CrystalClueEmission*CrystalClueBrightness);
            CrystalClueMaterial->SetScalarParameterValue(TEXT("Roughness"),FMath::Lerp(1.f,TreasureId==TEXT("4")?.58f:.19f,CrystalClueBrightness));
            CrystalClueMaterial->SetScalarParameterValue(TEXT("Metallic"),.12f*CrystalClueBrightness);
        }
    }
    BuriedGlow->SetVisibility(bAllowLight && HintAmount>.001f);
    GlowLight->SetVisibility(bAllowLight && !bCollected && Exposure > .02f);
    GlowLight->SetIntensity(28*FMath::Clamp(Exposure,0.f,1.f));
}

AMiningTreasureField::AMiningTreasureField()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Tint(TEXT("/Game/CollectionPrototype/Materials/M_CollectionTint"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Earth(TEXT("/Game/CollectionPrototype/Materials/M_CollectionEarth"));
    TreasureMaterial = Tint.Object;
    RockMaterial = Earth.Object;
    GuideMaterial = Tint.Object;
    const TCHAR* Names[] = {TEXT("煤炭"),TEXT("铜矿"),TEXT("锡矿"),TEXT("铁矿"),TEXT("硫磺"),TEXT("孔雀石"),TEXT("青金石"),TEXT("紫水晶"),TEXT("钻石")};
    const TCHAR* Colors[] = {TEXT("1C1C1C"),TEXT("D4763A"),TEXT("E4E4E0"),TEXT("9B3028"),TEXT("F2D63A"),TEXT("2F9E6B"),TEXT("2A4FA3"),TEXT("8B4FC4"),TEXT("E7FBFF")};
    for (int32 Index = 0; Index < 9; ++Index)
    {
        auto& Definition = Definitions.AddDefaulted_GetRef();
        Definition.Id = FName(*FString::FromInt(Index + 1));
        Definition.Name = FText::FromString(Names[Index]);
        Definition.Model = Index < 4 ? Sphere.Object : Index < 7 ? Cube.Object : Cone.Object;
        const FString ModelPath = FString::Printf(TEXT("/Game/Showcase/Treasure/Meshes/SM_Treasure_%02d.SM_Treasure_%02d"), Index+1,Index+1);
        if (auto* Art = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *ModelPath, nullptr, LOAD_NoWarn|LOAD_Quiet))) Definition.Model = Art;
        Definition.Color = FLinearColor::FromSRGBColor(FColor::FromHex(Colors[Index]));
        Definition.HintColor = Index == 0 ? FLinearColor(.38f,.3f,.2f) : Definition.Color/FMath::Max3(Definition.Color.R,Definition.Color.G,Definition.Color.B);
        Definition.SurfaceStyle = Index;
        Definition.EmissiveStrength = Index < 4 ? .12f : Index == 8 ? .7f : .45f;
        Definition.Roughness = Index == 0 ? .65f : Index < 4 ? .38f : .28f;
        Definition.Metallic = Index > 0 && Index < 4 ? .65f : .08f;
        Definition.ValuesBySize = FMiningTreasureDefinition::DefaultValues(Definition.Id);
        Definition.Regions.Add(RegionId);
    }
    if (auto* Art = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Showcase/Treasure/Materials/M_TreasureMineral.M_TreasureMineral"), nullptr, LOAD_NoWarn|LOAD_Quiet))) TreasureMaterial = Art;
    if (auto* Art = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Showcase/Treasure/Materials/M_TreasureRock.M_TreasureRock"), nullptr, LOAD_NoWarn|LOAD_Quiet))) { RockMaterial = Art; GuideMaterial = Art; }
}

void AMiningTreasureField::PostLoad()
{
    Super::PostLoad();
    for (auto& Definition : Definitions)
        if (Definition.ValuesBySize.IsEmpty())
        {
            Definition.ValuesBySize = FMiningTreasureDefinition::DefaultValues(Definition.Id);
            if(bUseAuthoredPlacements && Definition.ValuesBySize.Num()==4)
                for(int32 I=0;I<4;++I) Definition.SizeMultipliers[I]=FMath::Lerp(.7f,1.35f,
                    float(Definition.ValuesBySize[I]-Definition.ValuesBySize[0])/float(Definition.ValuesBySize[3]-Definition.ValuesBySize[0]));
        }
}

int32 AMiningTreasureField::ChooseDefinition(const TArray<FMiningTreasureDefinition>& Catalogue, FName Region, bool bGuide, FRandomStream& Random)
{
    double Total = 0;
    for (const auto& Definition : Catalogue) if (Definition.IsEligible(Region, bGuide)) Total += Definition.Weight;
    if (Total <= 0) return INDEX_NONE;
    double Choice = Random.FRand() * Total;
    int32 LastEligible = INDEX_NONE;
    for (int32 Index = 0; Index < Catalogue.Num(); ++Index)
    {
        if (!Catalogue[Index].IsEligible(Region, bGuide)) continue;
        LastEligible = Index;
        Choice -= Catalogue[Index].Weight;
        if (Choice < 0) return Index;
    }
    return LastEligible;
}

void AMiningTreasureField::BeginPlay()
{
    Super::BeginPlay();
    SetActorTickInterval(0);
    ResetExhibit();
}

void AMiningTreasureField::ClearGenerated()
{
    Target = nullptr;
    PendingAuthoredItems.Reset();
    for (AMiningTreasure* Item : Items) if (IsValid(Item)) Item->Destroy();
    for (ADiggableRock* Rock : Rocks) if (IsValid(Rock)) Rock->Destroy();
    Items.Reset();
    Rocks.Reset();
    ObservedCuts.Reset();
    ObservedTerrain.Reset();
    TerrainUpdateCursor=0;
}

void AMiningTreasureField::ResetExhibit()
{
    // Authored campaign ore belongs to this run: re-entering/resetting soil must not mint it again.
    if(bUseAuthoredPlacements && bAuthoredGenerated)return;
    ClearGenerated();
    Coins = bUseSharedWallet && GetGameInstance() ? GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>()->Balance : 0;
    CollectedCount = 0;
    ToastRemaining = 0;
    FRandomStream Random(RandomSeed);
    RewardRandom.Initialize(RandomSeed ^ 0x51D39A);
    if(bUseAuthoredPlacements){SpawnAuthored();return;}
    TSet<FName> Seen;
    for (const auto& Definition : Definitions)
    {
        if (Definition.Id.IsNone() || Seen.Contains(Definition.Id))
        {
            GenerationStatus = TEXT("生成失败：宝物 ID 为空或重复，请检查 Definitions。");
            return;
        }
        Seen.Add(Definition.Id);
    }
    const int32 Slots = FMath::Clamp(SlotsPerLayer, 1, 9);
    for (int32 Layer = 0; Layer < 2; ++Layer)
    {
        for (int32 Slot = 0; Slot < Slots; ++Slot)
        {
            const int32 DefinitionIndex = ChooseDefinition(Definitions, RegionId, Layer == 1, Random);
            if (DefinitionIndex == INDEX_NONE) continue;
            const auto& Definition = Definitions[DefinitionIndex];
            const FVector Local((Slot - (Slots - 1) * .5) * 360, Layer == 0 ? 400 : -450, 130);
            const FTransform RockTransform(GetActorQuat(), GetActorTransform().TransformPosition(Local));
            auto* Rock = GetWorld()->SpawnActorDeferred<ADiggableRock>(ADiggableRock::StaticClass(), RockTransform, this);
            Rock->RockSize = FVector(240, 280, 240);
            Rock->RockMaterial = Layer == 0 ? RockMaterial : GuideMaterial;
            Rock->FinishSpawning(RockTransform);
            if (Layer == 1 && GuideMaterial)
            {
                auto* GuideTint = UMaterialInstanceDynamic::Create(GuideMaterial, this);
                GuideTint->SetVectorParameterValue(TEXT("Tint"), FLinearColor(.065f,.04f,.09f));
                Rock->RockMaterial = GuideTint;
                Rock->RockMesh->SetMaterial(0, GuideTint);
            }
            Rocks.Add(Rock);
            const FVector Offset(Random.FRandRange(-12,12), 65, Random.FRandRange(-10,10));
            auto* Item = GetWorld()->SpawnActor<AMiningTreasure>(RockTransform.TransformPosition(Offset), GetActorRotation());
            Item->SetOwner(this);
            Item->CoveringRocks.Add(Rock);
            const int32 SizeTier = Random.RandRange(0, 3);
            const bool bHasSurface = Item->Configure(Definition, SizeTier, TreasureMaterial);
            const FBox LocalBounds = Item->Model->Bounds.GetBox().TransformBy(RockTransform.ToInverseMatrixWithScale());
            const FBox SafeBounds(-Rock->RockSize * .5 + FVector(8), Rock->RockSize * .5 - FVector(8));
            if (!bHasSurface || !SafeBounds.IsInsideOrOn(LocalBounds.Min) || !SafeBounds.IsInsideOrOn(LocalBounds.Max))
            {
                Item->Destroy();
                continue;
            }
            Items.Add(Item);
            Item->AttachBuriedHint(Definition);
            ObservedCuts.Add(INDEX_NONE);
        }
    }
    GenerationStatus = FString::Printf(TEXT("已生成 %d / %d 件 · 空缺请检查区域、权重、模型碰撞及尺寸"), Items.Num(), Slots * 2);
}

void AMiningTreasureField::SetAreaActive(bool bActive)
{
    bAreaActive = bActive;
    // Initialization is prepared incrementally by Tick before the player arrives.
    for (AMiningTreasure* Item : Items) if (IsValid(Item)) Item->SetDropActive(bActive && (!bUseAuthoredPlacements || Item->ActorHasTag(ActiveAreaTag)));
    if (!bActive && IsValid(Target)) Target->SetHighlighted(false);
    if (!bActive) Target = nullptr;
    if (!bActive) for (AMiningTreasure* Item : Items) if (IsValid(Item)) Item->GlowLight->SetVisibility(false);
}

void AMiningTreasureField::RefreshTarget(APlayerController* Controller)
{
    if (IsValid(Target)) Target->SetHighlighted(false);
    Target = nullptr;
    if (!bAreaActive || !Controller || !Controller->GetPawn() || Controller->IsPaused()) return;
    FVector View;
    FRotator Rotation;
    Controller->GetPlayerViewPoint(View, Rotation);
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(TreasureAim), true, Controller->GetPawn());
    if (MiningInteraction::Trace(GetWorld(),Hit,View,Rotation,400,Query,[this](const FHitResult& Candidate)
        {
            auto* Item=Cast<AMiningTreasure>(Candidate.GetActor());
            return IsValid(Item) && Items.Contains(Item) && !Item->bCollected && (!bUseAuthoredPlacements || Item->ActorHasTag(ActiveAreaTag));
        }))
    {
        auto* Item = Cast<AMiningTreasure>(Hit.GetActor());
        if (IsValid(Item) && Items.Contains(Item) && !Item->bCollected && (!bUseAuthoredPlacements || Item->ActorHasTag(ActiveAreaTag)))
        {
            Target = Item;
            Target->Exposure = Target->MeasureExposure();
            Target->SetHighlighted(AMiningTreasure::IsExposureEligible(Target->Exposure));
        }
    }
}

void AMiningTreasureField::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!PendingAuthoredItems.IsEmpty())
    {
        const int32 Before=PendingAuthoredItems.Num();
        if(bAreaActive)PrepareAuthoredArea(ActiveAreaTag,1);
        if(PendingAuthoredItems.Num()==Before)PrepareAuthoredArea(NAME_None,1);
    }
    ToastRemaining = FMath::Max(0.f, ToastRemaining - DeltaSeconds);
    auto* Controller = GetWorld()->GetFirstPlayerController();
    const FVector Observer = Controller && Controller->GetPawn() ? Controller->GetPawn()->GetActorLocation() : FVector(1.e9);
    TArray<AMiningTreasure*> Nearby;
    for (AMiningTreasure* Item : Items) if (IsValid(Item) && !PendingAuthoredItems.Contains(Item) && !Item->bCollected && FVector::DistSquared(Observer, Item->GetActorLocation()) < FMath::Square(1600.f)) Nearby.Add(Item);
    Nearby.Sort([Observer](const AMiningTreasure& First, const AMiningTreasure& Second) { return FVector::DistSquared(Observer, First.GetActorLocation()) < FVector::DistSquared(Observer, Second.GetActorLocation()); });
    if (Nearby.Num() > 8) Nearby.SetNum(8);
    for (AMiningTreasure* Item : Items) if (IsValid(Item)) Item->UpdateGlow(DeltaSeconds, bAreaActive && Nearby.Contains(Item));
    if (!bAreaActive) return;
    const double TerrainStarted=FPlatformTime::Seconds();
    LastUpdatedDeposits=0;
    // Only reward exposure/release is time-sliced. Terrain inclusions are updated
    // synchronously by OnTerrainCommitted, before a cut can reach the renderer.
    for (int32 Visited = 0; Visited < Items.Num(); ++Visited)
    {
        if(Visited>0 && (FPlatformTime::Seconds()-TerrainStarted)>.004)break;
        const int32 Index=TerrainUpdateCursor++%Items.Num();
        if(TerrainUpdateCursor>=Items.Num())TerrainUpdateCursor=0;
        auto* Item = Items[Index].Get();
        if (!IsValid(Item) || PendingAuthoredItems.Contains(Item) || (bUseAuthoredPlacements && !Item->ActorHasTag(ActiveAreaTag))) continue;
        int32 Cuts=0;for(ADiggableRock* Rock:Item->CoveringRocks)Cuts+=IsValid(Rock)?Rock->SuccessfulCuts:1000003;
        const uint32 Version=Item->GetTerrainVersion();
        if (!ObservedTerrain.Contains(Item) || ObservedTerrain[Item]!=Version)
        {
            ++LastUpdatedDeposits;
            if(!Item->bCollected)
            {
                if (Cuts == 0 && ObservedCuts[Index] > 0) Item->ResetDrop();
                Item->TryRelease();
            }
            ObservedCuts[Index] = Cuts;
            ObservedTerrain.Add(Item,Version);
        }
    }
    LastTerrainUpdateMilliseconds=(FPlatformTime::Seconds()-TerrainStarted)*1000;
    RefreshTarget(GetWorld()->GetFirstPlayerController());
}

bool AMiningTreasureField::CollectTarget(APlayerController* Controller)
{
    RefreshTarget(Controller);
    if (!IsValid(Target) || Target->bCollected || !AMiningTreasure::IsExposureEligible(Target->Exposure)) return false;
    auto* Item = Target.Get();
    auto* Inventory = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>() : nullptr;
    const bool bMaterials = Inventory && Inventory->UsesMineralInventory();
    if (bMaterials && !Inventory->AddMineral(Item->TreasureId)) return false;
    if (Controller && Controller->GetPawn())
        if (auto* Audio = Controller->GetPawn()->FindComponentByClass<UMiningAudioComponent>()) Audio->PlayCollect();
    Item->bCollected = true;
    Item->SetDropActive(false);
    Item->SetHighlighted(false);
    Item->CollectedValue = Item->Value;
    if (bMaterials) Item->CollectedValue = 1;
    else if (bUseSharedWallet && GetGameInstance())
    {
        auto* Wallet = GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
        Item->CollectedValue = Wallet->CalculateMineralReward(Item->Value, RewardRandom.FRand());
        Wallet->AddCoins(Item->CollectedValue);
        Coins = Wallet->Balance;
    }
    else
    {
        Coins += Item->Value;
        if (auto* MiningController = Cast<ABrainMiningPlayerController>(Controller); MiningController && MiningController->InteractionHUD)
            MiningController->InteractionHUD->ShowCoinGain(Item->Value);
    }
    ++CollectedCount;
    Toast = FString::Printf(TEXT("%s  +%lld 金币%s"), *Item->TreasureName.ToString(), Item->CollectedValue,
        Item->CollectedValue > Item->Value ? TEXT(" · 养成奖励翻倍！") : TEXT(""));
    ToastRemaining = 3;
    if (bMaterials) Toast = FString::Printf(TEXT("%s ×1  已收入背包"), *Item->TreasureName.ToString());
    Item->SetActorEnableCollision(false);
    // Hide only the reward. Terrain inclusions survive collection in every field.
    Item->Model->SetVisibility(false,false);
    Item->GlowLight->SetVisibility(false);
    Target = nullptr;
    return true;
}

FString AMiningTreasureField::GetPrompt() const
{
    if (ToastRemaining > 0) return Toast;
    if (GetGameInstance() && GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>()->UsesMineralInventory())
    {
        if (IsValid(Target)) return FString::Printf(TEXT("%s ×1 · 暴露 %.0f%%\n%s"), *Target->TreasureName.ToString(), Target->Exposure * 100,
            AMiningTreasure::IsExposureEligible(Target->Exposure) ? TEXT("[ E ] 收入背包") : TEXT("继续挖掘，暴露超过 50% 后可领取"));
        return FString::Printf(TEXT("矿石收入背包 · 已领取 %d / %d · Tab 升级矿镐"), CollectedCount, Items.Num());
    }
    if (IsValid(Target)) return FString::Printf(TEXT("%s · 大小 %d/4 · 基础 %d 金币 · 暴露 %.0f%%\n%s"), *Target->TreasureName.ToString(), Target->SizeTier+1, Target->Value, Target->Exposure * 100,
        AMiningTreasure::IsExposureEligible(Target->Exposure) ? TEXT("[ E ] 领取宝物") : TEXT("继续挖掘，暴露超过 50% 后可领取"));
    if (Items.IsEmpty()) return GenerationStatus;
    if (bUseSharedWallet) return FString::Printf(TEXT("左键挖掘 · 瞄准宝物按 E 领取 · 已领取 %d / %d\n金币供全部体验区共用；Tab 打开升级面板"), CollectedCount, Items.Num());
    return FString::Printf(TEXT("左键挖掘 · 瞄准宝物按 E 领取 · 已领取 %d / %d\nF5 重开本展区并清空展示金币；R 只恢复岩石，不补发宝物"), CollectedCount, Items.Num());
}

void AMiningTreasureField::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearGenerated();
    Super::EndPlay(Reason);
}
