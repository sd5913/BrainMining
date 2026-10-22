#include "CardRoomStage.h"
#include "CardRoomSymbols.h"
#include "CardRoomUISettings.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Components/PostProcessComponent.h"

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
    // Use the same authored artwork as the hand UI, including future UI asset swaps.
    CardFaceMaterials.Reset();
    auto* FaceBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Materials/M_PlayedCardFace.M_PlayedCardFace"));
    if(FaceBase && UISettings)
    {
        for(UTexture2D* Art:{UISettings->CardLaserArt.Get(),UISettings->CardPoisonArt.Get(),UISettings->CardFireArt.Get()})
        {
            auto* Face=UMaterialInstanceDynamic::Create(FaceBase,this);
            Face->SetTextureParameterValue(TEXT("CardArt"),Art);
            CardFaceMaterials.Add(Face);
        }
        for(const auto& Mark:CardMarks)
        {
            Mark->SetStaticMesh(Plane);
            Mark->SetRelativeScale3D(FVector(.98f,.98f,1));
        }
    }
}

void ACardRoomStage::TickEffects()
{
    const bool Resolve=Run.Phase==ECardRoomPhase::Resolving;
    const bool RewardVisible=Run.Phase==ECardRoomPhase::Victory;
    if(!RewardOutline)
    {
        RewardOutline=NewObject<UPostProcessComponent>(this,TEXT("DrillRewardOutline"));
        AddInstanceComponent(RewardOutline);RewardOutline->SetupAttachment(RootComponent);
        RewardOutline->bUnbound=true;RewardOutline->Priority=50;
        if(auto* M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionSilhouette.M_CollectionSilhouette")))
            RewardOutline->Settings.AddBlendable(M,1.f);
        RewardOutline->RegisterComponent();
    }
    RewardOutline->bEnabled=RewardVisible;
    const float T=Run.PhaseTime,Time=GetWorld()->GetTimeSeconds();
    Reward->SetVisibility(RewardVisible);
    Reward->SetRelativeLocation(RewardLocal+FVector(0,0,FMath::Sin(Time*2)*8));
    Reward->SetRelativeRotation(FRotator(0,Time*25,15));Reward->SetRelativeScale3D(FVector(2.5f));
    Reward->SetRenderCustomDepth(RewardVisible);Reward->SetCustomDepthStencilValue(2);
    if(bStyledEffects)
    {
        // Styled effects replace the prototype presentation. Running both first
        // rebuilt smoke/flame MIDs and transforms only to hide them in the same
        // frame, particularly throughout the poison ignition in round three.
        if(Resolve && (Run.Round==1 || Run.Round==3) && T>=1.75f && T<2.35f && !bReplySound)
        {
            Sound(TEXT("S_Impact"));
            if(AudioOverride.IsBound())Sound(Run.Round==1?TEXT("S_CardFire"):TEXT("S_LowHealth"),Run.Round==1?.3f:.45f);
            bReplySound=true;
        }
        TickStyledEffects();
        return;
    }
    const float S=FMath::Clamp(SpellPresentationScale,1.f,4.f);
    const FLinearColor Fire(2.8f,.27f,.012f),Hot(4.f,2.f,.30f),Blue(.025f,.55f,3.f),Ice(.65f,2.3f,3.8f),Gold(3.f,1.5f,.18f);
    auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);
    const FVector CameraLocal=Camera?GetActorTransform().InverseTransformPosition(Camera->GetCameraLocation()):FVector(-270,0,188);
    auto* Glow=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Materials/M_CardRoomFlameSprite.M_CardRoomFlameSprite"));
    auto* Smoke=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Materials/M_CardRoomSmokeSprite.M_CardRoomSmokeSprite"));
    auto Sprite=[&](UStaticMeshComponent* Part,FVector P,FVector2D Size,FLinearColor Color,float Alpha,bool Cloud,float Angle=0.f)
    {
        Part->SetVisibility(Alpha>.005f);
        Part->SetRelativeLocation(P);Part->SetRelativeScale3D(FVector(Size.X*S/100,Size.Y*S/100,1));
        const FVector Normal=(CameraLocal-P).GetSafeNormal();
        const FQuat Facing=FRotationMatrix::MakeFromZY(Normal,FVector::UpVector).ToQuat();
        Part->SetRelativeRotation(Facing*FQuat(FVector::UpVector,FMath::DegreesToRadians(Angle)));
        auto* Base=Cloud?Smoke:Glow;if(!Base)return;
        auto* MID=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0));
        if(!MID || MID->Parent!=Base){MID=UMaterialInstanceDynamic::Create(Base,this);Part->SetMaterial(0,MID);}
        MID->SetVectorParameterValue(TEXT("Color"),Color*(Cloud?1.f:1.6f));
        MID->SetScalarParameterValue(TEXT("Opacity"),FMath::Clamp(Alpha,0.f,1.f));
    };
    for(const auto& P:FxShards)P->SetVisibility(false);
    for(const auto& P:Trails)P->SetVisibility(false);
    for(const auto& P:Puffs)P->SetVisibility(false);
    Beam->SetVisibility(false);Ball->SetVisibility(false);SpellLight->SetIntensity(0);
    SpellLight->SetAttenuationRadius(600*S*GetActorScale3D().GetMax());
    int32 Next=0;
    auto Puff=[&](FVector P,FVector2D Size,FLinearColor Color,float Alpha=1.f,float Angle=0.f)
    {if(Next<FxShards.Num())Sprite(FxShards[Next++],P,Size,Color,Alpha,false,Angle);};
    // Aim at the actual plush torso; all gas and impacts follow this anchor after room edits.
    const FVector Target=GetSpellTargetLocal();
    const FVector Muzzle=GetSpellSourceLocal();
    auto Illuminate=[&](FVector P,FLinearColor Color,float Intensity)
    {SpellLight->SetRelativeLocation(P);SpellLight->SetLightColor(Color.GetClamped());SpellLight->SetIntensity(Intensity*S*S);};
    auto Flash=[&](FVector P,float Age,float Life,FLinearColor Color,float Radius,bool Grand=false)
    {
        if(Age<0 || Age>=Life)return;
        const float A=Age/Life;
        Puff(P,FVector2D(Radius,Radius)*(1+A*.9f),Color,(1-A),Age*35);
        Puff(P+FVector(-12*S,0,0),FVector2D(Radius*.48f,Radius*.48f),Ice,FMath::Max(0.f,1-A*3));
        for(int32 i=0;i<12;++i)
        {
            const float Angle=i*2*PI/12;
            const FVector Dir(-.18f,FMath::Cos(Angle),FMath::Sin(Angle));
            const float Size=Radius*.30f*(1-A);
            Puff(P+Dir*(Radius*S*(.25f+A*.55f)),FVector2D(Size,Size*1.3f),i%3?Color:Hot,(1-A),Angle*57);
        }
        for(int32 i=0;i<24;++i)
        {
            const float Angle=i*2.39996f;
            const FVector Dir(-.12f,FMath::Cos(Angle),FMath::Sin(Angle));
            Puff(P+Dir*(S*(15+A*(Radius*.7f+i%4*18))),FVector2D(7,30)*(1-A),i%3?Color:Gold,1-A,Angle*57);
        }
        if(Grand)for(int32 Ring=0;Ring<2;++Ring)for(int32 i=0;i<24;++i)
        {
            const float Angle=i*2*PI/24+Ring*.1f;
            const float R=Radius*S*(.4f+A*(1.4f+Ring*.4f));
            Puff(P+FVector(-25*S,FMath::Cos(Angle)*R,FMath::Sin(Angle)*R*.7f),FVector2D(20,35)*(1-A),Ring?Gold:Ice,(1-A)*.8f,Angle*57);
        }
        Illuminate(P,Color,5000*(1-A));
    };
    const bool Laser=Resolve&&(Run.Round==1||Run.Round==3)&&T>.70f&&T<1.08f;
    if(Laser)
    {
        const bool Fiery=Run.Round==3;const FVector D=Target-Muzzle;
        Beam->SetVisibility(true);Beam->SetRelativeLocation((Muzzle+Target)*.5f);
        Beam->SetRelativeRotation(FRotationMatrix::MakeFromZ(D).Rotator());
        Beam->SetRelativeScale3D(FVector((Fiery?.22f:.13f)*S,(Fiery?.22f:.13f)*S,D.Size()/100));
        Tint(Beam,(Fiery?Hot:Ice)*55,true);
        for(int32 i=0;i<26;++i)
        {
            const float A=i/25.f;
            const FVector P=FMath::Lerp(Muzzle,Target,A)+FVector(0,FMath::Sin(i*1.7f+T*17)*7*S,FMath::Cos(i+T*15)*5*S);
            const float Size=(Fiery?62:40)*(1+.18f*FMath::Sin(i+T*14));
            Puff(P,FVector2D(Size,Size),Fiery?(i%3?Fire:Hot):(i%3?Blue:Ice),.8f,i*19+Time*65);
        }
        Puff(Muzzle,FVector2D(100,100),Fiery?Hot:Ice);
        Illuminate(Target,Fiery?Fire:Blue,4200);
    }
    const bool Igniting=Resolve&&Run.Round==3&&T>=1&&T<1.9f;
    const bool Gas=Run.bPoisoned || (Resolve&&Run.Round==2&&T>.65f);
    if(Gas||Igniting)
    {
        for(int32 i=0;i<10;++i)
        {
            const float A=Time*.75f+i*2*PI/10;
            const FVector P=Target+FVector(-20+FMath::Sin(A*1.4f)*15,FMath::Cos(A)*65,FMath::Sin(A)*50)*S;
            const float Size=100+FMath::Sin(A*1.3f)*18;
            const float Fade=Igniting?FMath::Max(0.f,1-(T-1)/.9f):.8f;
            Sprite(Puffs[i],P,FVector2D(Size,Size*1.15f),Igniting?FLinearColor(.24f,.10f,.025f):i%2?FLinearColor(.22f,.65f,.035f):FLinearColor(.07f,.25f,.02f),Fade,true,Time*13+i*30);
        }
        if(!Igniting)for(int32 i=0;i<10;++i)
        {
            const float A=FMath::Fmod(Time*.7f+i*.1f,1.f);
            Puff(Target+FVector(-25,(i-4.5f)*15,-30+A*120)*S,FVector2D(17,23)*(1-A),FLinearColor(.35f,1.8f,.04f),.8f*(1-A));
        }
    }
    const bool Reply=Resolve&&(Run.Round==1||Run.Round==3)&&T>=1.75f&&T<2.35f;
    const bool Finale=Resolve&&Run.Round==4&&T>.15f&&T<1.f;
    if(Reply||Finale)
    {
        const bool Charging=Finale&&T<.65f;
        const float A=Reply?FMath::Clamp((T-1.75f)/.6f,0.f,1.f):FMath::Clamp((T-.65f)/.35f,0.f,1.f);
        const FVector Start=Reply?Target:(S>1.f?CardLandingLocal+FVector(0,0,240):FVector(-100,0,190));
        const FVector End=Reply?CameraLocal+FVector(90,0,-20):Target;
        const FVector Dir=(End-Start).GetSafeNormal();
        const FVector P=FMath::Lerp(Start,End,A);
        const float Charge=Charging?FMath::SmoothStep(.15f,.65f,T):1.f;
        const float Radius=Finale?FMath::Lerp(42.f,175.f,Charge):Run.Round==3?105.f:78.f;
        Puff(P,FVector2D(Radius,Radius),Finale?Blue:Fire);
        Puff(P-Dir*8*S,FVector2D(Radius*.58f,Radius*.58f),Finale?Ice:Hot);
        if(Finale)
        {
            // A faceted solid core, two orbiting ember streams and golden fragments retain lowpoly volume.
            Ball->SetVisibility(true);Ball->SetRelativeLocation(P);Ball->SetRelativeScale3D(FVector(Radius*S*.006f));
            Ball->SetRelativeRotation(FRotator(Time*65,Time*90,Time*35));Tint(Ball,Blue*24,true);
            for(int32 Ring=0;Ring<2;++Ring)for(int32 i=0;i<20;++i)
            {
                const float Angle=i*2*PI/20+T*(Ring?7.f:-5.f);
                const float R=Radius*S*(.62f+Ring*.17f);
                const FVector Orbit(FMath::Sin(Angle)*R*.35f,FMath::Cos(Angle)*R,FMath::Sin(Angle)*R);
                Puff(P+Orbit,FVector2D(16,28),Ring?Gold:Ice,.95f,Angle*57);
            }
            for(int32 i=0;i<Trails.Num();++i)
            {
                const float Angle=i*2.39996f+T*8;auto* Shard=Trails[i].Get();Shard->SetVisibility(true);
                Shard->SetRelativeLocation(P+FVector(FMath::Sin(Angle)*.3f,FMath::Cos(Angle),FMath::Sin(Angle))*(Radius*S*.82f));
                Shard->SetRelativeScale3D(FVector(.075,.075,.25)*S);Shard->SetRelativeRotation(FRotator(Angle*25,Time*140,i*36));Tint(Shard,Gold*22,true);
            }
        }
        if(!Charging)for(int32 i=0;i<32;++i)
        {
            const float U=i/31.f,Swirl=i*2.4f+T*9;
            const float Spread=(1-U)*(Finale?35:22)*S;
            const FVector W=P-Dir*(U*(Finale?260:170)*S)+FVector(0,FMath::Sin(Swirl)*Spread,FMath::Cos(Swirl)*Spread+U*20*S);
            const float Size=Radius*(1-U)*.68f+12;
            Puff(W,FVector2D(Size,Size*(1.1f+U*.7f)),Finale?(i%5?Blue:Gold):(i%5?Fire:Hot),(1-U)*.85f,Swirl*30);
        }
        Illuminate(P,Finale?Blue:Fire,Finale?8500:5000);
        if(Reply&&!bReplySound){Sound(TEXT("S_Impact"));if(AudioOverride.IsBound()){if(Run.Round==1)Sound(TEXT("S_CardFire"),.3f);if(Run.Round==3)Sound(TEXT("S_LowHealth"),.45f);}bReplySound=true;}
    }
    if(Resolve&&Run.Round==1)Flash(Target,T-1,.65f,Blue,110);
    if(Resolve&&Run.Round==3)Flash(Target,T-1,.9f,Fire,170);
    if(Resolve&&Run.Round==4)Flash(Target,T-1,1.35f,Blue,260,true);
    if(Resolve&&(Run.Round==1||Run.Round==3))Flash(CameraLocal+FVector(100,0,-20),T-2.35f,.5f,Fire,Run.Round==3?115:75);
    if(RewardVisible)
    {
        const FVector P=Reward->GetRelativeLocation();
        Illuminate(P,FLinearColor(1,.85f,.65f),1800);
    }
}
