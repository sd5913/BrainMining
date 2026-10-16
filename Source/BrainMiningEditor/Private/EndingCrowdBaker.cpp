#include "EndingCrowdBaker.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "StaticMeshResources.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Materials/Material.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "SkinnedAssetCompiler.h"
#include "Editor.h"

namespace
{
bool Save(UObject* Asset)
{
    Asset->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    return UPackage::SavePackage(Asset->GetOutermost(),Asset,*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}
struct FPiece
{
    USkeletalMeshComponent* Component=nullptr;
    const FSkeletalMeshLODRenderData* Data=nullptr;
    TArray<FVector3f> Bind,Normals;
    TArray<uint32> Indices;
    FLinearColor Color;
    bool bRigid=false;
    int32 Offset=0;
};
}

UStaticMesh* UEndingCrowdBaker::BakeWalker(const FEndingPedestrian& Person,const FString& PackagePath,int32 LOD,bool bIdle,FLinearColor ShirtColor)
{
    if(!Person.Driver||!Person.Face||!Person.Hair||!Person.Walk||!Person.Idle)return nullptr;
    UWorld* World=GEditor->GetEditorWorldContext().World();
    AActor* Scratch=World->SpawnActor<AActor>();
    auto Make=[&](USkeletalMesh* Mesh)
    {
        FSkinnedAssetCompilingManager::Get().FinishCompilation({Mesh});
        auto* C=NewObject<USkeletalMeshComponent>(Scratch);Scratch->AddInstanceComponent(C);
        C->SetDisablePostProcessBlueprint(true);C->SetSkeletalMesh(Mesh);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->RegisterComponent();
        C->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        return C;
    };
    auto* Driver=Make(Person.Driver);Driver->SetForcedLOD(1);
    UAnimSequence* Clip=bIdle?Person.Idle.Get():Person.Walk.Get();Driver->PlayAnimation(Clip,true);
    TArray<FPiece> Pieces;
    int32 VertexCount=0;
    auto AddSkeletal=[&](USkeletalMesh* Mesh,int32 Level,FLinearColor Color,bool Rigid)
    {
        FPiece P;P.Component=Make(Mesh);P.bRigid=Rigid;P.Color=Color;P.Offset=VertexCount;
        const int32 Index=FMath::Min(Level,Mesh->GetResourceForRendering()->LODRenderData.Num()-1);
        P.Component->SetForcedLOD(Index+1);
        if(!Rigid)P.Component->SetLeaderPoseComponent(Driver);
        P.Data=&Mesh->GetResourceForRendering()->LODRenderData[Index];
        const auto& VB=P.Data->StaticVertexBuffers;
        for(uint32 V=0;V<VB.PositionVertexBuffer.GetNumVertices();++V)
        {P.Bind.Add(VB.PositionVertexBuffer.VertexPosition(V));P.Normals.Add(VB.StaticMeshVertexBuffer.VertexTangentZ(V));}
        P.Data->MultiSizeIndexContainer.GetIndexBuffer(P.Indices);
        VertexCount+=P.Bind.Num();Pieces.Add(MoveTemp(P));
    };
    for(int32 I=0;I<Person.Clothing.Num();++I)
        AddSkeletal(Person.Clothing[I],LOD,I==0?FLinearColor(.32f,.19f,.12f):I==1?ShirtColor:I==2?FLinearColor(.055f,.075f,.1f):FLinearColor(.028f,.024f,.021f),false);
    AddSkeletal(Person.Face,LOD>=3?6:4,FLinearColor(.36f,.22f,.15f),true);
    FPiece Hair;Hair.Color=FLinearColor(.027f,.018f,.012f);Hair.bRigid=true;Hair.Offset=VertexCount;
    const auto& H=Person.Hair->GetRenderData()->LODResources[0];
    for(uint32 V=0;V<H.VertexBuffers.PositionVertexBuffer.GetNumVertices();++V)
    {Hair.Bind.Add(H.VertexBuffers.PositionVertexBuffer.VertexPosition(V));Hair.Normals.Add(H.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(V));}
    for(int32 I=0;I<H.IndexBuffer.GetNumIndices();++I)Hair.Indices.Add(H.IndexBuffer.GetIndex(I));
    VertexCount+=Hair.Bind.Num();Pieces.Add(MoveTemp(Hair));

    const auto& Ref=Person.Driver->GetRefSkeleton();int32 Bone=Ref.FindBoneIndex(TEXT("head"));
    FTransform HeadBind=FTransform::Identity;
    while(Bone!=INDEX_NONE){HeadBind=HeadBind*Ref.GetRefBonePose()[Bone];Bone=Ref.GetParentIndex(Bone);}
    constexpr int32 Width=256;
    const int32 Frames=bIdle?1:16,Rows=FMath::DivideAndRoundUp(VertexCount,Width),Height=Rows*Frames;
    TArray<FLinearColor> Pixels;Pixels.SetNumZeroed(Width*Height);
    TArray<FVector3f> Base;Base.SetNum(VertexCount);
    for(int32 Frame=0;Frame<Frames;++Frame)
    {
        Driver->SetPosition(Clip->GetPlayLength()*Frame/Frames,false);
        Driver->TickAnimation(0,false);Driver->RefreshBoneTransforms();
        const FTransform Head=HeadBind.Inverse()*Driver->GetBoneTransform(Ref.FindBoneIndex(TEXT("head")));
        for(auto& P:Pieces)
        {
            TArray<FVector3f> Positions;
            if(P.bRigid)
                for(auto V:P.Bind)Positions.Add(FVector3f(Head.TransformPosition(FVector(V))));
            else
            {
                TArray<FMatrix44f> Matrices;P.Component->GetCurrentRefToLocalMatrices(Matrices,LOD);
                USkinnedMeshComponent::ComputeSkinnedPositions(P.Component,Positions,Matrices,*P.Data,*P.Data->GetSkinWeightVertexBuffer());
            }
            if(Positions.Num()!=P.Bind.Num()){Scratch->Destroy();return nullptr;}
            for(int32 V=0;V<Positions.Num();++V)
            {
                const int32 ID=P.Offset+V;if(Frame==0)Base[ID]=Positions[V];
                const FVector3f Delta=Positions[V]-Base[ID];
                Pixels[Frame*Rows*Width+ID]=FLinearColor(Delta.X,Delta.Y,Delta.Z,1);
            }
        }
    }
    const FString TexPath=PackagePath+TEXT("_Motion"),TexName=FPackageName::GetShortName(TexPath);
    auto* Texture=NewObject<UTexture2D>(CreatePackage(*TexPath),*TexName,RF_Public|RF_Standalone);
    Texture->Source.Init(Width,Height,1,1,TSF_RGBA32F,reinterpret_cast<const uint8*>(Pixels.GetData()));
    Texture->SRGB=false;Texture->CompressionSettings=TC_HDR;Texture->MipGenSettings=TMGS_NoMipmaps;
    Texture->Filter=TF_Nearest;Texture->AddressX=TA_Clamp;Texture->AddressY=TA_Clamp;Texture->NeverStream=true;
    Texture->UpdateResource();FAssetRegistryModule::AssetCreated(Texture);if(!Save(Texture)){Scratch->Destroy();return nullptr;}

    FMeshDescription Description;FStaticMeshAttributes A(Description);A.Register();A.GetVertexInstanceUVs().SetNumChannels(1);
    const auto Group=Description.CreatePolygonGroup();A.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Crowd");
    TArray<FVertexID> Vertices;
    for(auto P:Base){auto V=Description.CreateVertex();A.GetVertexPositions()[V]=P;Vertices.Add(V);}
    for(const auto& P:Pieces)for(int32 T=0;T+2<P.Indices.Num();T+=3)
    {
        TArray<FVertexInstanceID> Triangle;
        for(int32 Corner=0;Corner<3;++Corner)
        {
            const int32 Local=P.Indices[T+Corner],ID=P.Offset+Local;
            auto V=Description.CreateVertexInstance(Vertices[ID]);Triangle.Add(V);
            A.GetVertexInstanceNormals()[V]=P.Normals[Local];A.GetVertexInstanceTangents()[V]=FVector3f(1,0,0);A.GetVertexInstanceBinormalSigns()[V]=1;
            const float Shade=.72f+.28f*FMath::Clamp(P.Normals[Local].Z,0.f,1.f);
            A.GetVertexInstanceColors()[V]=FVector4f(P.Color.R*Shade,P.Color.G*Shade,P.Color.B*Shade,1);
            A.GetVertexInstanceUVs().Set(V,0,FVector2f((ID%Width+.5f)/Width,(ID/Width+.5f)/Rows));
        }
        Description.CreatePolygon(Group,Triangle);
    }
    const FString Name=FPackageName::GetShortName(PackagePath);
    auto* Mesh=NewObject<UStaticMesh>(CreatePackage(*PackagePath),*Name,RF_Public|RF_Standalone);
    Mesh->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface),TEXT("Crowd")));
    UStaticMesh::FBuildMeshDescriptionsParams Settings;Settings.bFastBuild=false;Settings.bBuildSimpleCollision=false;
    Mesh->BuildFromMeshDescriptions({&Description},Settings);Mesh->SetPositiveBoundsExtension(FVector(65,65,30));Mesh->SetNegativeBoundsExtension(FVector(65,65,30));
    FAssetRegistryModule::AssetCreated(Mesh);const bool Saved=Save(Mesh);Scratch->Destroy();
    UE_LOG(LogTemp,Display,TEXT("ENDING_CROWD_BAKE %s vertices=%d triangles=%d frames=%d duration=%.3f"),*PackagePath,VertexCount,Description.Triangles().Num(),Frames,Clip->GetPlayLength());
    return Saved?Mesh:nullptr;
}
