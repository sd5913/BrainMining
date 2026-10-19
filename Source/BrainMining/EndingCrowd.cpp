#include "EndingCrowd.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "CrowdInstancer.h"
#include "OverCrowdDataAsset.h"

namespace
{
template<class T> T* Part(AActor* Owner, USceneComponent* Parent)
{
    T* C=NewObject<T>(Owner,NAME_None,RF_Transient);
    Owner->AddInstanceComponent(C);
    C->SetupAttachment(Parent);
    C->RegisterComponent();
    return C;
}
void CheapMesh(UPrimitiveComponent* C)
{
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetCastShadow(false);
    C->SetVisibleInRayTracing(false);
}
FTransform HeadBind(USkeletalMesh* Mesh)
{
    const auto& R=Mesh->GetRefSkeleton();
    int32 B=R.FindBoneIndex(TEXT("head"));
    FTransform T=FTransform::Identity;
    while(B!=INDEX_NONE){T=T*R.GetRefBonePose()[B];B=R.GetParentIndex(B);}
    return T;
}

UHierarchicalInstancedStaticMeshComponent* OverCrowdBatch(ACrowdInstancer* Owner, UStaticMesh* Mesh, int32 Index)
{
    if(!Owner||!Mesh)return nullptr;
    const FGameplayTag WardrobeID=FGameplayTag::RequestGameplayTag(FName("OverCrowd.Wardrobe.Default"));
    UVatStaticMesh* Pair=NewObject<UVatStaticMesh>(Owner,*FString::Printf(TEXT("EndingVatPair_%d"),Index));
    Pair->VatStaticMesh=Mesh;
    Pair->BodyPart=FGameplayTag::RequestGameplayTag(FName("OverCrowd.BodyPart.Body"));
    FWardrobe Wardrobe;Wardrobe.WardrobeID=WardrobeID;
    Owner->AddHISMComponent(Pair,Wardrobe);
    if(const FDataHISMLookUp* Lookup=Owner->HISMLookupWithWardrobeID.Find(WardrobeID))
        if(const FHISMData* Data=Lookup->HISMLookup.Find(Pair))return Data->HISM;
    return nullptr;
}
}

bool UEndingCrowdComponent::Build(UEndingCrowdAssets* Assets)
{
    if(Assets&&Assets->BakedWalkers.Num()==8&&Assets->BakedHero)return BuildBaked(Assets);
    if(!Assets||Assets->People.IsEmpty())return false;
    for(const auto& P:Assets->People)
        if(!P.Driver||!P.Face||!P.Hair||!P.Walk||!P.Idle||P.Clothing.IsEmpty()||P.Clothing.Contains(nullptr))return false;
    AActor* Owner=GetOwner();
    constexpr int32 Phases=8, Count=240;
    for(const auto& P:Assets->People)
    {
        for(int32 Phase=0;Phase<Phases;++Phase)
        {
            auto* C=Part<USkeletalMeshComponent>(Owner,this);
            C->SetSkeletalMesh(P.Driver);CheapMesh(C);
            C->SetForcedLOD(3);
            C->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            C->PlayAnimation(P.Walk,true);
            C->SetPosition(P.Walk->GetPlayLength()*Phase/Phases);
            C->SetPlayRate(.9f+.025f*Phase);
            C->SetHiddenInGame(true);
            Drivers.Add(C);
        }
    }
    FRandomStream R(4917);
    for(int32 I=0;I<=Count;++I)
    {
        const bool Center=I==Count;
        const int32 Preset=Center?0:(I*7+I/11)%Assets->People.Num(),Phase=(I/Assets->People.Num())%Phases;
        const auto& P=Assets->People[Preset];
        auto* Root=Part<USceneComponent>(Owner,this);
        USkeletalMeshComponent* Driver=Drivers[Preset*Phases+Phase];
        if(Center)
        {
            Driver=Part<USkeletalMeshComponent>(Owner,this);
            Driver->SetSkeletalMesh(P.Driver);CheapMesh(Driver);
            Driver->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            Driver->SetHiddenInGame(true);Driver->PlayAnimation(P.Idle,true);Drivers.Add(Driver);
        }
        USkeletalMeshComponent* Body=nullptr;
        for(auto Mesh:P.Clothing)
        {
            auto* C=Part<USkeletalMeshComponent>(Owner,Root);
            C->SetSkeletalMesh(Mesh);CheapMesh(C);
            C->SetForcedLOD(Center?2:3);
            C->SetLeaderPoseComponent(Driver);
            if(!Body)Body=C;
        }
        // Face meshes contain a separate facial rig. Keep their neutral reference
        // pose and attach rigidly to the animated head; no incompatible leader rig.
        const FTransform InverseHead=HeadBind(P.Driver).Inverse();
        auto* Face=Part<USkeletalMeshComponent>(Owner,Body);
        Face->AttachToComponent(Body,FAttachmentTransformRules::KeepRelativeTransform,TEXT("head"));
        Face->SetDisablePostProcessBlueprint(true);
        Face->SetSkeletalMesh(P.Face);Face->SetRelativeTransform(InverseHead);
        Face->SetForcedLOD(Center?5:6);Face->SetComponentTickEnabled(false);CheapMesh(Face);
        auto* Hair=Part<UStaticMeshComponent>(Owner,Body);
        Hair->AttachToComponent(Body,FAttachmentTransformRules::KeepRelativeTransform,TEXT("head"));
        Hair->SetStaticMesh(P.Hair);Hair->SetRelativeTransform(InverseHead);CheapMesh(Hair);
        const bool AlongY=(I/2)%2!=0;
        const float Direction=I%2?1.f:-1.f;
        FVector Start=Center?FVector(0,0,0):FVector(R.FRandRange(-2700,2700),Direction*R.FRandRange(75,330),0);
        if(AlongY)Swap(Start.X,Start.Y);
        const float Scale=Center?1.f:R.FRandRange(.96f,1.04f);
        Root->SetRelativeScale3D(FVector(Scale));
        // The acquired meshes face +Y in their reference pose.
        Root->SetRelativeRotation(FRotator(0,Center?90.f:(AlongY?0.f:-90.f)+(Direction<0?180.f:0.f),0));
        Root->SetRelativeLocation(Start);
        People.Add(Root);Origins.Add(Start);
        Speeds.Add(Center?0:Direction*P.Speed*(.9f+.025f*Phase)*Scale);
    }
    // Original city meshes were unlit. These lights are enabled only in its two shots.
    for(int32 I=0;I<2;++I)
    {
        auto* L=Part<UDirectionalLightComponent>(Owner,this);
        L->SetRelativeRotation(I?FRotator(-25,155,0):FRotator(-55,-35,0));
        L->SetIntensity(I?1.2f:3.f);L->SetLightColor(I?FLinearColor(.45,.65,1):FLinearColor(1,.8,.6));
        L->SetCastShadows(false);Lights.Add(L);
    }
    UpdateCrowd(false,0);
    return true;
}

void UEndingCrowdComponent::UpdateCrowd(bool bActive,float DeltaTime)
{
    if(!Batches.IsEmpty())
    {
        if(bCrowdActive!=bActive){bCrowdActive=bActive;SetVisibility(bActive,true);}
        if(OverCrowdInstancer)OverCrowdInstancer->SetActorHiddenInGame(!bActive);
        if(!bActive){TravelTime=0;return;}
        TravelTime+=DeltaTime;
        for(int32 B=0;B<Batches.Num()-1;++B)
        {
            for(int32 I=0;I<BatchTransforms[B].Num();++I)
            {
                FVector P=BatchOrigins[B][I]+BatchVelocities[B][I]*TravelTime;
                if(BatchVelocities[B][I].X!=0)P.X=FMath::Fmod(P.X+42000+4200,8400)-4200;
                else P.Y=FMath::Fmod(P.Y+42000+4200,8400)-4200;
                BatchTransforms[B][I].SetLocation(P);
            }
            if(!BatchTransforms[B].IsEmpty())Batches[B]->BatchUpdateInstancesTransforms(0,BatchTransforms[B],false,true,true);
        }
        return;
    }
    SetVisibility(bActive,true);
    for(auto L:Lights)L->SetVisibility(bActive);
    for(auto D:Drivers){D->SetHiddenInGame(true);D->SetComponentTickEnabled(bActive);}
    if(!bActive){TravelTime=0;return;}
    TravelTime+=DeltaTime;
    for(int32 I=0;I<People.Num();++I)
    {
        FVector P=Origins[I];
        if(Speeds[I]!=0)
        {
            double& Axis=(I/2)%2?P.Y:P.X;
            Axis=FMath::Fmod(Axis+Speeds[I]*TravelTime+2700+54000,5400)-2700;
        }
        People[I]->SetRelativeLocation(P);
    }
}

bool UEndingCrowdComponent::BuildBaked(UEndingCrowdAssets* Assets)
{
    for(auto Mesh:Assets->BakedWalkers)if(!Mesh)return false;
    AActor* Owner=GetOwner();
    if(!Owner||!Owner->GetWorld())return false;
    FActorSpawnParameters SpawnInfo;SpawnInfo.Owner=Owner;SpawnInfo.Name=TEXT("EndingOverCrowdInstancer");
    // The crowd component is attached to the city set (Sets[1]); spawn the
    // OverCrowd HISM owner at that component transform so its local lanes land
    // in the same street space as the JapaneseCity replacement.
    OverCrowdInstancer=Owner->GetWorld()->SpawnActor<ACrowdInstancer>(ACrowdInstancer::StaticClass(),GetComponentTransform(),SpawnInfo);
    if(!OverCrowdInstancer)return false;
    OverCrowdInstancer->SetActorEnableCollision(false);
    OverCrowdInstancer->SetActorHiddenInGame(true);
    for(int32 I=0;I<9;++I)
    {
        UStaticMesh* Mesh=I==8?Assets->BakedHero:Assets->BakedWalkers[I];
        auto* C=OverCrowdBatch(OverCrowdInstancer,Mesh,I);
        if(!C){OverCrowdInstancer->Destroy();OverCrowdInstancer=nullptr;return false;}
        C->NumCustomDataFloats=2;CheapMesh(C);C->SetBoundsScale(1.25f);
        Batches.Add(C);BatchTransforms.AddDefaulted();BatchOrigins.AddDefaulted();BatchVelocities.AddDefaulted();
    }
    FRandomStream Random(9127);
    for(int32 I=0;I<1200;++I)
    {
        const bool AlongY=I%2!=0;const int32 Lane=(I/2)%12;
        const float Direction=Lane<6?1.f:-1.f,Rate=Random.FRandRange(.88f,1.12f);
        FVector P(-4000+(I/24)*160+Random.FRandRange(-22,22),-440+Lane*80+Random.FRandRange(-9,9),0);
        if(AlongY)Swap(P.X,P.Y);
        if(P.Size2D()<85)P+=FVector(AlongY?110:0,AlongY?0:110,0);
        const int32 B=(I*7+I/11)%4+(P.Size2D()<850?0:4);
        FTransform T(FRotator(0,(AlongY?0:-90)+(Direction<0?180:0),0),P,FVector(Random.FRandRange(.95f,1.06f)));
        const int32 Instance=Batches[B]->AddInstance(T);
        Batches[B]->SetCustomDataValue(Instance,0,Random.FRand(),false);
        Batches[B]->SetCustomDataValue(Instance,1,Rate,false);
        BatchTransforms[B].Add(T);BatchOrigins[B].Add(P);
        const float Speed=Direction*(B%4==1?105.f:120.f)*Rate*T.GetScale3D().X;
        BatchVelocities[B].Add(AlongY?FVector(0,Speed,0):FVector(Speed,0,0));++BakedCount;
    }
    Batches[8]->AddInstance(FTransform(FRotator(0,90,0),FVector::ZeroVector));
    Batches[8]->SetCustomDataValue(0,1,1,true);++BakedCount;
    for(auto B:Batches){B->MarkRenderStateDirty();B->PrecachePSOs();}
    SetVisibility(false,true);bCrowdActive=false;
    UE_LOG(LogTemp, Display, TEXT("ENDING_OVERCROWD_HISM_READY: batches=%d instances=%d"), Batches.Num(), BakedCount);
    return true;
}
