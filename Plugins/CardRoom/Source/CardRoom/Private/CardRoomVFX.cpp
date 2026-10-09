#include "CardRoomStage.h"
#include "CardRoomSymbols.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

namespace
{
    struct FFacet { FVector3f A,B,C; FLinearColor Color; };
    UStaticMesh* BuildFacetMesh(UObject* Outer,const TArray<FFacet>& Faces,bool bReverseNormals=false)
    {
        FMeshDescription Desc; FStaticMeshAttributes Attr(Desc); Attr.Register();
        auto Positions=Attr.GetVertexPositions(); auto Normals=Attr.GetVertexInstanceNormals();
        auto Tangents=Attr.GetVertexInstanceTangents(); auto Signs=Attr.GetVertexInstanceBinormalSigns();
        auto UV=Attr.GetVertexInstanceUVs(); UV.SetNumChannels(1);
        auto Colors=Attr.GetVertexInstanceColors();
        TArray<FLinearColor> Palette; TArray<FPolygonGroupID> Groups;
        UStaticMesh* Mesh=NewObject<UStaticMesh>(Outer,NAME_None,RF_Transient);
        auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Materials/M_CardRoomSurface.M_CardRoomSurface"));
        for(const auto& F:Faces)
        {
            int32 Slot=Palette.IndexOfByKey(F.Color);
            if(Slot==INDEX_NONE)
            {
                Slot=Palette.Add(F.Color); Groups.Add(Desc.CreatePolygonGroup());
                auto* Mat=UMaterialInstanceDynamic::Create(Base,Mesh); Mat->SetVectorParameterValue(TEXT("Color"),F.Color);
                const FName SlotName(*FString::Printf(TEXT("Facet%d"),Slot));
                Mesh->GetStaticMaterials().Add(FStaticMaterial(Mat,SlotName));
                Attr.GetPolygonGroupMaterialSlotNames()[Groups[Slot]]=SlotName;
            }
            FVector3f N=FVector3f::CrossProduct(F.B-F.A,F.C-F.A).GetSafeNormal();
            if(bReverseNormals) N=-N;
            TArray<FVertexInstanceID> Verts;
            for(FVector3f P:{F.A,F.B,F.C})
            {
                const FVertexID V=Desc.CreateVertex(); Positions[V]=P;
                const FVertexInstanceID I=Desc.CreateVertexInstance(V); Verts.Add(I);
                Normals[I]=N; Tangents[I]=(F.B-F.A).GetSafeNormal(); Signs[I]=1;
                UV.Set(I,0,FVector2f(.5f,.5f)); Colors[I]=FVector4f(1,1,1,1);
            }
            Desc.CreatePolygon(Groups[Slot],Verts);
        }
        UStaticMesh::FBuildMeshDescriptionsParams Params;
        Params.bFastBuild=true; Params.bBuildSimpleCollision=false; Params.bMarkPackageDirty=false;
        Params.bCommitMeshDescription=false;
        if(!Mesh->BuildFromMeshDescriptions({&Desc},Params)) return nullptr;
        return Mesh;
    }
}

void ACardRoomStage::InitializeEffectMeshes()
{
    // Eight flat-shaded faces replace smooth prototype spheres throughout the spells.
    TArray<FFacet> Faces;
    const FVector3f Ring[]={{50,0,0},{0,50,0},{-50,0,0},{0,-50,0}};
    for(int32 i=0;i<4;++i)
    {
        Faces.Add({{0,0,50},Ring[i],Ring[(i+1)%4],FLinearColor::White});
        Faces.Add({{0,0,-50},Ring[(i+1)%4],Ring[i],FLinearColor::White});
    }
    UStaticMesh* Crystal=BuildFacetMesh(this,Faces);
    if(Crystal)
    {
        for(auto* Part:{Ball.Get(),Reward.Get()}) Part->SetStaticMesh(Crystal);


        for(const auto& Part:Trails) Part->SetStaticMesh(Crystal);
    }
    auto* Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
    for(const auto& P:FxShards) { P->SetStaticMesh(Plane); P->SetTranslucentSortPriority(1); }
    for(const auto& P:Puffs) P->SetStaticMesh(Plane);
    SymbolMeshes.Reset();
    for(int32 Type=0;Type<3;++Type)
    {
        Faces.Reset(); int32 Layer=0;
        for(const auto& Tri:CardRoomSymbols::Make(static_cast<ERoomCard>(Type)))
        {
            auto Point=[&](FVector2f P){return FVector3f((.5f-P.Y)*100,(P.X-.5f)*100,Layer*2.f);};
            FFacet F={Point(Tri.A),Point(Tri.B),Point(Tri.C),Tri.Color};
            // UE front faces use clockwise winding: cross(C-A, B-A) points up.
            // Keep the card art facing the player above the table, not into the paper.
            if(FVector3f::CrossProduct(F.B-F.A,F.C-F.A).Z>0) Swap(F.B,F.C);
            Faces.Add(F); ++Layer;
        }
        SymbolMeshes.Add(BuildFacetMesh(this,Faces,true));
    }
    for(const auto& Mark:CardMarks) Mark->EmptyOverrideMaterials();
}

void ACardRoomStage::TickEffects()
{
    const bool Resolve=Run.Phase==ECardRoomPhase::Resolving;
    const bool RewardVisible=Run.Phase==ECardRoomPhase::Victory;
    const float T=Run.PhaseTime,Time=GetWorld()->GetTimeSeconds();
    const FLinearColor Fire(2.3f,.21f,.008f),Hot(3.8f,1.7f,.20f),Blue(.03f,.48f,2.5f),Ice(.7f,1.9f,3.f);
    auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);
    const FVector CameraLocal=Camera?GetActorTransform().InverseTransformPosition(Camera->GetCameraLocation()):FVector(-270,0,188);
    auto* Glow=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Materials/M_CardRoomFlameSprite.M_CardRoomFlameSprite"));
    auto* Smoke=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Materials/M_CardRoomSmokeSprite.M_CardRoomSmokeSprite"));
    auto Sprite=[&](UStaticMeshComponent* Part,FVector P,FVector2D Size,FLinearColor Color,float Alpha,bool Cloud,float Angle=0.f)
    {
        Part->SetVisibility(Alpha>.005f);
        Part->SetRelativeLocation(P); Part->SetRelativeScale3D(FVector(Size.X/100,Size.Y/100,1));
        const FVector Normal=(CameraLocal-P).GetSafeNormal();
        FQuat Facing=FRotationMatrix::MakeFromZY(Normal,FVector::UpVector).ToQuat();
        Part->SetRelativeRotation(Facing*FQuat(FVector::UpVector,FMath::DegreesToRadians(Angle)));
        auto* Base=Cloud?Smoke:Glow;
        auto* MID=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0));
        if(!MID || MID->Parent!=Base) { MID=UMaterialInstanceDynamic::Create(Base,this); Part->SetMaterial(0,MID); }
        MID->SetVectorParameterValue(TEXT("Color"),Color);
        MID->SetScalarParameterValue(TEXT("Opacity"),Alpha);
    };
    for(const auto& P:FxShards) P->SetVisibility(false);
    for(const auto& P:Trails) P->SetVisibility(false);
    for(const auto& P:Puffs) P->SetVisibility(false);
    Beam->SetVisibility(false); Ball->SetVisibility(false); SpellLight->SetIntensity(0);
    int32 Next=0;
    auto GlowPuff=[&](FVector P,FVector2D Size,FLinearColor Color,float Alpha=1.f,float Angle=0.f)
    {
        if(Next<FxShards.Num()) Sprite(FxShards[Next++],P,Size,Color,Alpha,false,Angle);
    };
    const FVector Target(133,0,190),Muzzle(-100,0,140);
    auto Flash=[&](FVector P,float Age,float Life,FLinearColor Color,float Radius)
    {
        if(Age<0 || Age>=Life) return;
        float A=Age/Life;
        // A hot central bloom, rolling lobes and outward sparks create a readable impact silhouette.
        GlowPuff(P,FVector2D(Radius,Radius)*(1+A*.9f),Color,(1-A)*1.2f,Age*35);
        GlowPuff(P+FVector(-4,0,0),FVector2D(Radius*.48f,Radius*.48f),FLinearColor(3,2.2f,1.1f),FMath::Max(0.f,1-A*3));
        for(int32 i=0;i<12;++i)
        {
            float Angle=i*2*PI/12;
            FVector Dir(-.18f,FMath::Cos(Angle),FMath::Sin(Angle));
            float Size=Radius*.25f*(1-A);
            GlowPuff(P+Dir*(Radius*.25f+A*Radius*.5f),FVector2D(Size,Size*1.3f),i%3?Color:Hot,(1-A)*.9f,Angle*57);
        }
        for(int32 i=0;i<18;++i)
        {
            float Angle=i*2.39996f;
            FVector Dir(-.12f,FMath::Cos(Angle),FMath::Sin(Angle));
            GlowPuff(P+Dir*(15+A*(70+i%4*16)),FVector2D(4,15)*(1-A),i%3?Color:Hot,1-A,Angle*57);
        }
        SpellLight->SetRelativeLocation(P); SpellLight->SetLightColor(Color.GetClamped()); SpellLight->SetIntensity(2200*(1-A));
    };
    const bool Laser=Resolve&&(Run.Round==1||Run.Round==3)&&T>.48f&&T<1.04f;
    if(Laser)
    {
        const bool Fiery=Run.Round==3; const FVector D=Target-Muzzle;
        Beam->SetVisibility(true); Beam->SetRelativeLocation((Muzzle+Target)*.5f);
        Beam->SetRelativeRotation(FRotationMatrix::MakeFromZ(D).Rotator());
        Beam->SetRelativeScale3D(FVector(Fiery?.13f:.075f,Fiery?.13f:.075f,D.Size()/100));
        Tint(Beam,(Fiery?Hot:Ice)*35,true);
        for(int32 i=0;i<20;++i)
        {
            float A=i/19.f;
            FVector P=FMath::Lerp(Muzzle,Target,A)+FVector(0,FMath::Sin(i*1.7f+T*17)*(Fiery?8:3),FMath::Cos(i+T*15)*(Fiery?7:3));
            float Size=(Fiery?50:28)*(1+.2f*FMath::Sin(i+T*14));
            GlowPuff(P,FVector2D(Size,Size),Fiery?(i%3?Fire:Hot):(i%3?Blue:Ice),Fiery?.5f:.36f,i*19+Time*65);
        }
        GlowPuff(Muzzle,FVector2D(80,80),Fiery?Hot:Ice,.8f);
        SpellLight->SetRelativeLocation(Target);SpellLight->SetLightColor(Fiery?FLinearColor(1,.3f,.03f):FLinearColor(.05f,.6f,1));SpellLight->SetIntensity(1700);
    }
    // Thick, overlapping translucent clouds make poison a volume instead of green diamonds.
    const bool Gas=Run.bPoisoned || (Resolve&&Run.Round==2&&T>.55f);
    const bool Igniting=Resolve&&Run.Round==3&&T>=1&&T<1.75f;
    if(Gas||Igniting)
    {
        for(int32 i=0;i<10;++i)
        {
            float A=Time*.75f+i*2*PI/10;
            FVector P(144+FMath::Sin(A*1.4f)*18,FMath::Cos(A)*46,161+FMath::Sin(A)*33);
            float Size=66+FMath::Sin(A*1.3f)*14;
            float Fade=Igniting?FMath::Max(0.f,1-(T-1)/.75f):.58f;
            Sprite(Puffs[i],P,FVector2D(Size,Size*1.15f),Igniting?FLinearColor(.12f,.07f,.025f):i%2?FLinearColor(.11f,.28f,.025f):FLinearColor(.055f,.12f,.015f),Fade,true,Time*13+i*30);
        }
        if(!Igniting) for(int32 i=0;i<6;++i)
        {
            float A=FMath::Fmod(Time*.7f+i*.167f,1.f);
            GlowPuff(FVector(125,(i-2.5f)*18,160+A*90),FVector2D(12,18)*(1-A),FLinearColor(.2f,.9f,.04f),.35f*(1-A));
        }
    }
    const bool Reply=Resolve&&(Run.Round==1||Run.Round==3)&&T>=1.75f&&T<2.35f;
    const bool Finale=Resolve&&Run.Round==4&&T>.22f&&T<1.10f;
    if(Reply||Finale)
    {
        float A=Reply?FMath::Clamp((T-1.75f)/.6f,0.f,1.f):FMath::Clamp((T-.22f)/.78f,0.f,1.f);
        FVector Start=Reply?FVector(138,0,190):FVector(-100,0,150);
        FVector End=Reply?FVector(-155,0,185):Target;
        FVector P=FMath::Lerp(Start,End,A),Dir=(End-Start).GetSafeNormal();
        float Radius=Finale?88:Run.Round==3?75:55;
        // Core and several offset lobes read as a rolling fire/plasma mass, with an unbroken wake.
        GlowPuff(P,FVector2D(Radius,Radius),Finale?Blue:Fire,1.f);
        GlowPuff(P-Dir*4,FVector2D(Radius*.58f,Radius*.58f),Finale?Ice:Hot,1.1f);
        for(int32 i=0;i<28;++i)
        {
            float U=i/27.f;
            float Swirl=i*2.4f+T*9;
            float Spread=(1-U)*(Finale?20:15);
            FVector W=P-Dir*(U*(Finale?180:145))+FVector(0,FMath::Sin(Swirl)*Spread,FMath::Cos(Swirl)*Spread+U*20);
            float Size=Radius*(1-U)*.75f+12;
            GlowPuff(W,FVector2D(Size,Size*(1.1f+U*.7f)),Finale?(i%5?Blue:Ice):(i%5?Fire:Hot),(1-U)*.6f,Swirl*30);
        }
        for(int32 i=0;i<10;++i)
        {
            float U=FMath::Fmod(T*1.8f+i*.1f,1.f);
            GlowPuff(P-Dir*(30+U*160)+FVector(0,FMath::Sin(i*4.f)*30,FMath::Cos(i*3.f)*25),FVector2D(4,10)*(1-U),Finale?Ice:Hot,1-U,i*31.f);
        }
        SpellLight->SetRelativeLocation(P); SpellLight->SetLightColor(Finale?FLinearColor(.05f,.45f,1):FLinearColor(1,.25f,.015f));SpellLight->SetIntensity(2600);
        if(Reply&&!bReplySound){Sound(TEXT("S_Impact"));bReplySound=true;}
    }
    if(Resolve && Run.Round==1) Flash(Target,T-1,.55f,Blue,78);
    if(Resolve && Run.Round==3) Flash(Target,T-1,.72f,Fire,140);
    if(Resolve && Run.Round==4) Flash(Target,T-1,1.05f,Blue,180);
    if(Resolve&&(Run.Round==1||Run.Round==3)) Flash(FVector(-145,0,180),T-2.35f,.5f,Fire,Run.Round==3?115:75);
    Reward->SetVisibility(RewardVisible);
    Reward->SetRelativeLocation(FVector(-30,0,143+FMath::Sin(Time*2)*5));
    Reward->SetRelativeRotation(FRotator(0,Time*45,25));
    if(RewardVisible)
    {
        GlowPuff(Reward->GetRelativeLocation(),FVector2D(40,40),Blue,.7f);
        for(int32 i=0;i<10;++i) GlowPuff(Reward->GetRelativeLocation()+FVector(i*7,FMath::Sin(Time*2+i*.4f)*6,i*2),FVector2D(20-i,20-i),i%2?Blue:Ice,.5f);
    }
}
