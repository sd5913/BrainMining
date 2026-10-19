#include "EndingEmergence.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DynamicMeshToMeshDescription.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "ScopedTransaction.h"
#include "StaticMeshAttributes.h"
#include "UDynamicMesh.h"
#include "UObject/SavePackage.h"

namespace EndingScenery
{
bool SaveAsset(UObject* Asset)
{
    FAssetRegistryModule::AssetCreated(Asset);
    Asset->MarkPackageDirty();
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    return UPackage::SavePackage(Asset->GetOutermost(),Asset,
        *FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}

struct FPart
{
    UMeshComponent* Source=nullptr;
    UStaticMesh* Mesh=nullptr;
    TArray<UMaterialInterface*> Materials;
    TArray<FTransform> LocalTransforms;
    bool bCell=false;
};
}
#endif

bool AEndingEmergence::ConvertSceneryToEditableActors()
{
#if WITH_EDITOR
    // Never re-create a deleted prop or overwrite a hand-edited transform.
    if(bEditableScenery)return true;
    if(!GetWorld()||GetWorld()->IsGameWorld()||Sets.Num()<2)return false;
    const FString AssetRoot=TEXT("/Game/EndingEmergence/Editable/Bake_")+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
    TArray<EndingScenery::FPart> Parts;
    TArray<UMeshComponent*> Meshes;GetComponents(Meshes);
    TMap<UMaterialInterface*,UMaterialInterface*> Materials;
    for(auto* Source:Meshes)
    {
        bool bCell=Source->GetName().StartsWith(TEXT("EllipsoidalCellInterior"));
        bool bCity=false,bExcluded=false;
        for(USceneComponent* Parent=Source;Parent;Parent=Parent->GetAttachParent())
        {
            bCell|=Parent->GetName().StartsWith(TEXT("CellDress"));
            bCity|=Parent==Sets[1];
            bExcluded|=Parent==NightContinent||Parent==EyePerson||Parent==Crowd||Parent==CrowdHeads;
        }
        if((!bCell&&!bCity)||bExcluded)continue;
        EndingScenery::FPart Part;Part.Source=Source;Part.bCell=bCell;
        if(auto* Static=Cast<UStaticMeshComponent>(Source))
        {
            Part.Mesh=Static->GetStaticMesh();
            if(!Part.Mesh)continue;
            if(auto* Instances=Cast<UInstancedStaticMeshComponent>(Static))
            {
                for(int32 I=0;I<Instances->GetInstanceCount();++I)
                {FTransform T;Instances->GetInstanceTransform(I,T,false);Part.LocalTransforms.Add(T);}
                if(Part.LocalTransforms.IsEmpty())continue;
            }
            else Part.LocalTransforms.Add(FTransform::Identity);
        }
        else if(auto* Dynamic=Cast<UDynamicMeshComponent>(Source))
        {
            FMeshDescription Description;FStaticMeshAttributes(Description).Register();
            Dynamic->GetDynamicMesh()->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Mesh)
            {FDynamicMeshToMeshDescription().Convert(&Mesh,Description);});
            if(Description.Triangles().Num()==0)return false;
            const FString Name=TEXT("SM_")+Source->GetName();
            Part.Mesh=NewObject<UStaticMesh>(CreatePackage(*(AssetRoot+TEXT("/")+Name)),*Name,RF_Public|RF_Standalone);
            for(int32 I=0;I<Source->GetNumMaterials();++I)
                Part.Mesh->GetStaticMaterials().Add(FStaticMaterial(Source->GetMaterial(I)));
            // The room keeps its exact curved, walkable surface rather than a convex hull.
            Part.Mesh->CreateBodySetup();
            Part.Mesh->GetBodySetup()->CollisionTraceFlag=CTF_UseComplexAsSimple;
            Part.Mesh->GetBodySetup()->bDoubleSidedGeometry=false;
            UStaticMesh::FBuildMeshDescriptionsParams Build;Build.bFastBuild=false;Build.bBuildSimpleCollision=false;
            Part.Mesh->BuildFromMeshDescriptions({&Description},Build);
            Part.LocalTransforms.Add(FTransform::Identity);
        }
        else continue;

        for(int32 I=0;I<Source->GetNumMaterials();++I)
        {
            auto* Material=Source->GetMaterial(I);
            if(auto* Dynamic=Cast<UMaterialInstanceDynamic>(Material))
            {
                if(auto** Existing=Materials.Find(Material))Material=*Existing;
                else
                {
                    const FString Name=FString::Printf(TEXT("MI_%s_%d"),*Source->GetName(),I);
                    auto* Constant=NewObject<UMaterialInstanceConstant>(CreatePackage(*(AssetRoot+TEXT("/")+Name)),*Name,RF_Public|RF_Standalone);
                    // Flatten the instance's uniform values into a persistent, editable asset.
                    UMaterialInterface* Parent=Dynamic->Parent;
                    while(auto* ParentDynamic=Cast<UMaterialInstanceDynamic>(Parent))Parent=ParentDynamic->Parent;
                    Constant->SetParentEditorOnly(Parent);
                    Constant->CopyMaterialUniformParametersEditorOnly(Dynamic);
                    Constant->PostEditChange();
                    if(!EndingScenery::SaveAsset(Constant))return false;
                    Materials.Add(Material,Constant);Material=Constant;
                }
            }
            Part.Materials.Add(Material);
        }
        if(Cast<UDynamicMeshComponent>(Source))
        {
            for(int32 I=0;I<Part.Materials.Num();++I)Part.Mesh->SetMaterial(I,Part.Materials[I]);
            if(!EndingScenery::SaveAsset(Part.Mesh))return false;
        }
        Parts.Add(MoveTemp(Part));
    }
    if(Parts.IsEmpty())return false;

    const FScopedTransaction Transaction(NSLOCTEXT("Ending", "EditableScenery", "Convert ending scenery to editable actors"));
    Modify();GetLevel()->Modify();
    TArray<AStaticMeshActor*> Created;
    for(const auto& Part:Parts)for(int32 I=0;I<Part.LocalTransforms.Num();++I)
    {
        FActorSpawnParameters Spawn;Spawn.OverrideLevel=GetLevel();Spawn.ObjectFlags|=RF_Transactional;
        auto* Actor=GetWorld()->SpawnActor<AStaticMeshActor>(Spawn);
        if(!Actor)
        {for(auto* Added:Created)Added->Destroy();return false;}
        Created.Add(Actor);Actor->Modify();
        auto* Mesh=Actor->GetStaticMeshComponent();Mesh->Modify();
        Mesh->SetMobility(EComponentMobility::Movable);Mesh->SetStaticMesh(Part.Mesh);
        for(int32 Slot=0;Slot<Part.Materials.Num();++Slot)Mesh->SetMaterial(Slot,Part.Materials[Slot]);
        Mesh->SetCollisionProfileName(Part.Source->GetCollisionProfileName());
        Mesh->SetCollisionEnabled(Part.Source->GetCollisionEnabled());
        Mesh->SetCollisionResponseToChannels(Part.Source->GetCollisionResponseToChannels());
        Mesh->SetGenerateOverlapEvents(false);Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetCastShadow(Part.Source->CastShadow);Mesh->SetVisibleInRayTracing(Part.Source->bVisibleInRayTracing);
        // The original component is now an invisible animation/visibility anchor.
        // SetVisibility(..., true) crosses actor attachment boundaries; no Tick
        // copies transforms back over the artist's saved local transform.
        Actor->AttachToComponent(Part.Source,FAttachmentTransformRules::KeepRelativeTransform);
        Mesh->SetRelativeTransform(Part.LocalTransforms[I]);
        Actor->Tags.Add(TEXT("EndingEditableScenery"));
        Actor->Tags.Add(Part.bCell?TEXT("EndingEditableCell"):TEXT("EndingEditableCity"));
        Actor->SetActorLabel(FString::Printf(TEXT("EDIT_%s_%s_%03d"),Part.bCell?TEXT("细胞房"):TEXT("街景"),*Part.Source->GetName(),I+1));
        Actor->SetFolderPath(Part.bCell?TEXT("Ending_Edit/01_Cell"):TEXT("Ending_Edit/02_City"));
        Actor->MarkPackageDirty();
    }
    // Commit only after all actors exist. Keep source geometry as flow anchors
    // and historical validation data, but never render or collide with it twice.
    for(const auto& Part:Parts)
    {
        Part.Source->Modify();Part.Source->ComponentTags.AddUnique(TEXT("EndingSceneryAnchor"));
        Part.Source->SetRenderInMainPass(false);Part.Source->SetRenderInDepthPass(false);
        Part.Source->SetCastShadow(false);Part.Source->SetVisibleInRayTracing(false);
        Part.Source->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    bEditableScenery=true;
    // The brain and street occupy the same coordinates in different shots.
    // Show the authored cell and street together without the other stages
    // obscuring the editor view. RestartDemo restores stage visibility in play.
    Sets[0]->SetVisibility(false,true);Sets[1]->SetVisibility(true,true);
    if(NightContinent)NightContinent->SetVisibility(false,true);
    for(const auto& Part:Parts)if(Part.bCell)Part.Source->SetVisibility(true,true);
    MarkPackageDirty();
    UE_LOG(LogTemp,Display,TEXT("ENDING_SCENERY_READY: %d editable actors; generated layout locked; assets=%s"),Created.Num(),*AssetRoot);
    return true;
#else
    return false;
#endif
}
