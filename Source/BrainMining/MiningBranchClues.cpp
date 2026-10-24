#include "MiningCollection.h"
#include "MiningAromaComponent.h"
#include "DiggableRock.h"
#include "BrainMiningFonts.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/DecalComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void AMiningCollectionField::SpawnBranchClues(const FMiningCollectionPlacement& P)
{
    using namespace UE::Geometry;
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"));
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder"));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MainGame/Materials/ScenePolish/M_CollectibleDetail"));
    if(!Cube || !Sphere || !Cylinder || !Base)return;
    const FLinearColor Colors[]={FLinearColor(.84f,.73f,.55f),FLinearColor(.035f,.025f,.02f),FLinearColor(.95f,.64f,.12f),FLinearColor(.018f,.025f,.035f),FLinearColor(.68f,.88f,.91f),FLinearColor(.34f,.045f,.055f),FLinearColor(.15f,.38f,.44f)};
    TArray<UMaterialInstanceDynamic*> Materials;
    for(int I=0;I<UE_ARRAY_COUNT(Colors);++I)
    {
        auto* M=UMaterialInstanceDynamic::Create(Base,this);M->SetVectorParameterValue(TEXT("Tint"),Colors[I]);
        M->SetScalarParameterValue(TEXT("Roughness"),I==2?.36f:.85f);M->SetScalarParameterValue(TEXT("Metallic"),I==2?.65f:0.f);Materials.Add(M);
    }
    if(auto* Flat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_BranchClueFlat")))
        for(int I:{3,5}){auto* M=UMaterialInstanceDynamic::Create(Flat,this);M->SetVectorParameterValue(TEXT("Tint"),I==3?FLinearColor(.009f,.012f,.017f):FLinearColor(.21f,.018f,.035f));Materials[I]=M;}
    // Every anchor follows the actual wall, and disappears when its supporting soil is removed.
    auto Anchor=[&](FVector Local,bool Snap=true)
    {
        FHitResult Hit;
        const auto& Frame=P.ClueTransform;
        FVector Surface=Local;FQuat Rotation=FQuat::Identity;
        if(Snap && P.HostRock->ActorLineTraceSingle(Hit,Frame.TransformPosition(Local-FVector(100,0,0)),Frame.TransformPosition(Local+FVector(160,0,0)),ECC_Visibility,FCollisionQueryParams(NAME_None,true)))
        {
            Surface=Frame.InverseTransformPosition(Hit.ImpactPoint);
            Rotation=FRotationMatrix::MakeFromX(-Frame.InverseTransformVectorNoScale(Hit.ImpactNormal)).ToQuat();
        }
        auto* A=GetWorld()->SpawnActor<AActor>();Spawned.Add(A);A->SetOwner(P.HostRock);
        A->Tags.Add(FName(*FString::Printf(TEXT("BranchClue_%d"),P.ItemId)));
        auto* Root=NewObject<USceneComponent>(A);A->SetRootComponent(Root);A->AddInstanceComponent(Root);Root->RegisterComponent();
        A->SetActorLocationAndRotation(Surface+Rotation.RotateVector(FVector(-1.5f,0,0)),Rotation);
        FCollectionClue C;C.Actor=A;C.Rock=P.HostRock;C.Support=Surface+Rotation.RotateVector(FVector(3,0,0));Clues.Add(C);
        return A;
    };
    auto Part=[&](AActor* A,UStaticMesh* Mesh,FVector Position,FVector Size,int Color,FRotator Rotation=FRotator::ZeroRotator)
    {
        auto* C=NewObject<UStaticMeshComponent>(A);A->AddInstanceComponent(C);C->SetupAttachment(A->GetRootComponent());
        C->SetStaticMesh(Mesh);C->SetMaterial(0,Materials[Color]);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);
        C->SetRelativeLocationAndRotation(Position,Rotation);C->SetRelativeScale3D(Size/100.f);C->RegisterComponent();return C;
    };
    auto Line=[&](AActor* A,FVector From,FVector To,float Width,int Color)
    {Part(A,Cylinder,(From+To)*.5f,FVector(Width,Width,FVector::Dist(From,To)),Color,FRotationMatrix::MakeFromZ(To-From).Rotator());};
    auto Surface=[&](AActor* A,FDynamicMesh3&& Mesh,UMaterialInterface* Material)
    {
        FMeshNormals::InitializeOverlayToPerVertexNormals(Mesh.Attributes()->PrimaryNormals());
        auto* C=NewObject<UDynamicMeshComponent>(A);A->AddInstanceComponent(C);C->SetupAttachment(A->GetRootComponent());
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);C->SetMaterial(0,Material);C->SetMesh(MoveTemp(Mesh));C->RegisterComponent();return C;
    };
    const FVector Buried=P.ClueTransform.InverseTransformPosition(P.Transform.GetLocation());
    if(P.ItemId==16)
    {
        auto* A=Anchor(FVector::ZeroVector);
        if(auto* Vent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_BranchVent")))
        {
            auto* D=NewObject<UDecalComponent>(A);A->AddInstanceComponent(D);D->SetupAttachment(A->GetRootComponent());
            D->SetDecalMaterial(Vent);D->DecalSize=FVector(25,23,23);D->SetRelativeLocationAndRotation(FVector(8,0,0),FRotator(0,0,90));D->SetFadeScreenSize(.0001f);D->RegisterComponent();
        }
        A=Anchor(FVector::ZeroVector,false);
        A->Tags.Add(TEXT("CollectionPersistentAroma"));
        auto* Steam=NewObject<UMiningAromaComponent>(A);A->AddInstanceComponent(Steam);
        Steam->SourceLocal=Buried;Steam->RegisterComponent();
    }
    else if(P.ItemId==17)
    {
        // Six staggered soil bands, with seven shells per band. Supports stay
        // behind each fan so its full front can emerge before the support is cut.
        for(int I=0;I<42;++I)
        {
            const int Band=I/7;const float Depth=Band/6.f;
            auto* A=Anchor(Buried*Depth+FVector(Band?((I%3)-1)*12:0,-85+(I*47)%171,-48+(I*31)%97),Band==0);
            Clues.Last().Support+=FVector(Band?12:0,0,0);
            // A scallop shell narrows to a hinge, with a broad scalloped fan and radial ridges.
            FDynamicMesh3 Mesh;Mesh.EnableAttributes();constexpr int Segments=48,Rings=8;
            Mesh.AppendVertex(FVector3d(-2,0,-13));
            for(int R=1;R<=Rings;++R)for(int J=0;J<=Segments;++J)
            {
                const float T=R/float(Rings),Angle=-1.18f+2.36f*J/Segments;
                const float Ridge=.5f+.5f*FMath::Cos(J*PI/2),Radius=27.f+Ridge*1.5f;
                Mesh.AppendVertex(FVector3d(-2-4*FMath::Sin(T*PI*.85f)-Ridge*T*.9f,T*Radius*FMath::Sin(Angle),-13+T*Radius*FMath::Cos(Angle)));
            }
            for(int J=0;J<Segments;++J)Mesh.AppendTriangle(0,1+J,2+J);
            for(int R=0;R<Rings-1;++R)for(int J=0;J<Segments;++J)
            {int B=1+R*(Segments+1)+J;Mesh.AppendTriangle(B,B+Segments+1,B+1);Mesh.AppendTriangle(B+1,B+Segments+1,B+Segments+2);}
            Mesh.ReverseOrientation();
            auto* Shell=Surface(A,MoveTemp(Mesh),Materials[0]);Shell->SetRelativeRotation(FRotator(0,0,-28+I*13));
            Part(A,Cube,FVector(-2,0,-12),FVector(3,8,4),0,FRotator(0,0,-28+I*13));
        }
    }
    else if(P.ItemId==18)
    {
        for(int I=0;I<21;++I)
        {
            const float Depth=I<7?0.f:(I-6)/15.f;
            auto* A=Anchor(Buried*Depth+FVector(0,-80+(I*47)%163,-30+(I*29)%66),I<7);
            Part(A,Cylinder,FVector(-1,0,0),FVector(12+(I%3)*2,12+(I%3)*2,2),2,FRotator(0,90,90));
            Part(A,Cube,FVector(-2.4f,0,0),FVector(.6f,3,5),0,FRotator(0,0,I*23));
        }
    }
    else if(P.ItemId==19)
    {
        // Thin citrus slices: distinct rind, pale pith and eight wedge-shaped segments.
        for(int I=0;I<10;++I)
        {
            const float Depth=I<3?0.f:(I-2)/8.f;
            auto* A=Anchor(Buried*Depth+FVector(0,-48+(I*41)%97,-22+(I*29)%47),I<3);
            const FRotator Face(0,90,90);
            const bool Lime=I%3==1;
            auto CitrusMaterial=[&](FLinearColor Tint)
            {auto* M=UMaterialInstanceDynamic::Create(Base,A);M->SetVectorParameterValue(TEXT("Tint"),Tint);M->SetScalarParameterValue(TEXT("Metallic"),0.f);M->SetScalarParameterValue(TEXT("Roughness"),.72f);return M;};
            auto* Rind=CitrusMaterial(Lime?FLinearColor(.26f,.46f,.025f):FLinearColor(.82f,.57f,.025f));
            auto* Pith=CitrusMaterial(Lime?FLinearColor(.83f,.94f,.48f):FLinearColor(1.f,.88f,.44f));
            auto* Flesh=CitrusMaterial(Lime?FLinearColor(.33f,.59f,.035f):FLinearColor(.97f,.64f,.06f));
            Part(A,Cylinder,FVector(-1,0,0),FVector(29,29,4.5f),2,Face)->SetMaterial(0,Rind);
            Part(A,Cylinder,FVector(-3.4f,0,0),FVector(25.5f,25.5f,1),0,Face)->SetMaterial(0,Pith);
            FDynamicMesh3 Segments;Segments.EnableAttributes();
            for(int J=0;J<8;++J)
            {
                const float Angle=J*PI/4+I*.37f;
                const int32 Tip=Segments.AppendVertex(FVector3d(-6.f,1.1f*FMath::Cos(Angle),1.1f*FMath::Sin(Angle)));
                for(int K=0;K<=10;++K)
                {
                    const float A0=Angle+(K/10.f-.5f)*(PI/4-.075f);
                    const float Radius=11.65f- .6f*FMath::Pow(FMath::Abs(K/5.f-1.f),8.f);
                    const int32 V=Segments.AppendVertex(FVector3d(-6.f,Radius*FMath::Cos(A0),Radius*FMath::Sin(A0)));
                    if(K>0)Segments.AppendTriangle(Tip,V,V-1);
                }
            }
            Segments.ReverseOrientation();
            Surface(A,MoveTemp(Segments),Flesh);
            if(I%2==0)
            {
                Part(A,Sphere,FVector(-2,16,-10),FVector(8,9,9),5);
                Line(A,FVector(-2,16,-6),FVector(-2,18,5),.8f,1);
                Line(A,FVector(-2,18,5),FVector(-2,23,8),.7f,1);
            }
        }
    }
    else if(P.ItemId==20)
    {
        auto* A=Anchor(FVector::ZeroVector);
        if(auto* Ink=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_BranchInkClue")))
        {
            auto* D=NewObject<UDecalComponent>(A);A->AddInstanceComponent(D);D->SetupAttachment(A->GetRootComponent());
            D->SetDecalMaterial(Ink);D->DecalSize=FVector(70,82,82);D->SetRelativeLocation(FVector(25,0,0));D->SetFadeScreenSize(.0001f);D->RegisterComponent();
        }
    }
    else if(P.ItemId==21)
    {
        auto* A=Anchor(FVector::ZeroVector);
        // A rigid sign stands just proud of the uneven soil, so corners do not clip into it.
        A->AddActorLocalOffset(FVector(-20,0,0));
        Part(A,Cube,FVector(0,0,0),FVector(5,154,83),0);
        Part(A,Cube,FVector(-3,0,0),FVector(2,147,76),3);
        auto* Sign=NewObject<UWidgetComponent>(A);A->AddInstanceComponent(Sign);Sign->SetupAttachment(A->GetRootComponent());
        Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);Sign->SetWidgetSpace(EWidgetSpace::World);Sign->SetDrawSize(FVector2D(640,320));
        Sign->SetBackgroundColor(FLinearColor::Transparent);Sign->SetTwoSided(false);Sign->SetCastShadow(false);Sign->SetRedrawTime(.5f);
        Sign->SetRelativeLocationAndRotation(FVector(-4.2f,0,0),FRotator(0,180,0));Sign->SetRelativeScale3D(FVector(.22f));
        Sign->SetSlateWidget(SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("↑"))).Font(BrainMiningFonts::Body(150)).ColorAndOpacity(FLinearColor::White)]
            +SHorizontalBox::Slot().FillWidth(2).VAlign(VAlign_Center)[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("乘车"))).Font(BrainMiningFonts::Body(86)).ColorAndOpacity(FLinearColor::White)]
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("To Train"))).Font(BrainMiningFonts::Body(42)).ColorAndOpacity(FLinearColor::White)]]);
        Sign->RegisterComponent();
    }
    else if(P.ItemId==22)
    {
        for(int I=0;I<5;++I)
        {
            auto* A=Anchor(FVector(0,-75+I*37,-28+(I*31)%75));
            Part(A,Cube,FVector(-1,0,0),FVector(2,32+I%2*8,19),I%2?4:0,FRotator(0,0,-12+I*5));
            Part(A,Cube,FVector(-1,8,-9),FVector(2,6,6),I%2?4:0,FRotator(0,0,45));
            Part(A,Sphere,FVector(-2.4f,-10,2),FVector(.8f,5,5),6);
            for(int J=0;J<2;++J)Part(A,Cube,FVector(-2.5f,3,3-J*6),FVector(.7f,14-J*4,1.3f),6);
        }
    }
}
