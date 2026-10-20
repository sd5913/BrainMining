#include "CardRoomStage.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

void ACardRoomStage::InitializeStyledEffects()
{
    if(!bStyledEffects)return;
#if WITH_EDITOR
    for(auto& System:StyledSystems)if(System)System->WaitForCompilationComplete(true,false);
#endif
    // Independent components avoid teleporting the particles of a previous cast.
    const int32 Assets[]={0,1,2,1,3,4,1,1,1,1,1,1};
    for(int32 I=0;I<UE_ARRAY_COUNT(Assets);++I)
    {
        auto* FX=NewObject<UNiagaraComponent>(this,I==5?FName(TEXT("P2Fx_PoisonHit")):FName(*FString::Printf(TEXT("StyledSpell%d"),I)));
        AddInstanceComponent(FX);FX->SetupAttachment(RootComponent);FX->SetAutoActivate(false);
        if(StyledSystems.IsValidIndex(Assets[I]))FX->SetAsset(StyledSystems[Assets[I]]);
        FX->SetCastShadow(false);FX->RegisterComponent();StyledFX.Add(FX);
    }
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    while(Trails.Num()<17)
    {
        auto* Trail=NewObject<UStaticMeshComponent>(this,*FString::Printf(TEXT("StyledCardTrail%d"),Trails.Num()));
        AddInstanceComponent(Trail);Trail->SetupAttachment(RootComponent);Trail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Trail->SetCastShadow(false);Trail->SetVisibility(false);Trail->RegisterComponent();Trails.Add(Trail);
    }
    Ball->SetStaticMesh(Sphere);for(auto& P:Trails)P->SetStaticMesh(Sphere);
    for(int32 I=0;I<3;++I)
    {
        auto* Ring=NewObject<UStaticMeshComponent>(this,*FString::Printf(TEXT("StyledImpactRing%d"),I));
        AddInstanceComponent(Ring);Ring->SetupAttachment(RootComponent);
        Ring->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
        Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);Ring->SetCastShadow(false);
        Ring->SetVisibility(false);Ring->SetMaterial(0,StyledRingMaterial);Ring->RegisterComponent();StyledRings.Add(Ring);
    }
}

void ACardRoomStage::TickStyledEffects()
{
    const bool Resolve=Run.Phase==ECardRoomPhase::Resolving;
    const float T=Run.PhaseTime,S=FMath::Clamp(SpellPresentationScale,1.f,4.f);
    const FVector Target=GetSpellTargetLocal();
    const FVector Impact=Target+FVector(-14*OpponentScale,0,0);
    const FVector Muzzle=GetSpellSourceLocal();
    auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);
    const FVector Eye=Camera?GetActorTransform().InverseTransformPosition(Camera->GetCameraLocation()):Muzzle;
    const FLinearColor Cyan(.06f,.62f,1.f),Amber(1.f,.28f,.045f),Cream(1.f,.75f,.32f);
    if(!Resolve || StyledRound!=Run.Round || T<StyledLastTime)
    {
        for(int32 I=0;I<StyledFX.Num();++I)if(I!=5)StyledFX[I]->DeactivateImmediate();
        StyledFired=0;StyledRound=Run.Round;
    }
    StyledLastTime=T;
    Beam->SetVisibility(false);Ball->SetVisibility(false);for(auto& P:Trails)P->SetVisibility(false);
    for(auto& Ring:StyledRings)Ring->SetVisibility(false);
    // P2 uses the original, unmodified Niagara poison resource. Suppress the
    // legacy card-room sprites instead of drawing both presentations together.
    for(auto& P:FxShards)P->SetVisibility(false);
    for(auto& P:Puffs)P->SetVisibility(false);
    SpellLight->SetIntensity(0);
    auto Energy=[&](UStaticMeshComponent* P,FVector Pos,FVector Scale,FLinearColor Color,float Opacity)
    {
        P->SetVisibility(Opacity>.01f);P->SetRelativeLocation(Pos);P->SetRelativeScale3D(Scale);
        auto* Base=StyledRings.Contains(P)?StyledRingMaterial.Get():StyledEnergyMaterial.Get();
        if(!Base)return;
        auto* M=Cast<UMaterialInstanceDynamic>(P->GetMaterial(0));
        if(!M || M->Parent!=Base){M=UMaterialInstanceDynamic::Create(Base,this);P->SetMaterial(0,M);}
        M->SetVectorParameterValue(TEXT("Color"),Color);M->SetScalarParameterValue(TEXT("Opacity"),Opacity);
    };
    auto Fire=[&](int32 Index,float At,FVector P,float Scale,FLinearColor Color)
    {
        if(!Resolve || T<At || !StyledFX.IsValidIndex(Index))return;
        auto* FX=StyledFX[Index].Get();
        if(!(StyledFired&(1u<<Index)))
        {
            FX->SetRelativeLocation(P);FX->SetRelativeScale3D(FVector(Scale*S));
            FX->Activate(true);StyledFired|=1u<<Index;
        }
    };
    if(StyledFX.IsValidIndex(5))
    {
        auto* Poison=StyledFX[5].Get();
        const bool Burning=Resolve && Run.Round==3 && T>=1.f && T<1.7f;
        const bool KeepPoison=Run.bPoisoned || Burning || (Resolve && Run.Round==2 && T>=1.f);
        if(KeepPoison && !bStyledPoisonLive)
        {
            Poison->SetRelativeLocation(Target);Poison->SetRelativeScale3D(FVector(S));
            Poison->Activate(true);bStyledPoisonLive=true;
        }
        else if(!KeepPoison && bStyledPoisonLive)
        {
            // Stop emission after the flame front enters the cloud. Existing
            // particles finish naturally, so fire visibly consumes the poison.
            if(Resolve && Run.Round==3)Poison->Deactivate();
            else Poison->DeactivateImmediate();
            bStyledPoisonLive=false;
        }
        if(!Resolve && !Run.bPoisoned)Poison->DeactivateImmediate();
    }
    // A visible flame front occupies the gas volume while the Niagara smoke
    // finishes. This stays readable even when the imported fire burst is faint.
    if(Resolve && Run.Round==3 && T>=1.f && T<2.25f)
    {
        auto* Flame=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Materials/M_CardRoomFlameSprite"));
        for(int32 I=0;I<FMath::Min(10,Puffs.Num());++I)
        {
            const float Life=T-1.f-I*.045f;
            if(Life<=0 || Life>=.85f)continue;
            const float A=Life/.85f,Angle=I*2.39996f;
            auto* P=Puffs[I].Get();
            const FVector Position=Target+FVector(-45*S,FMath::Cos(Angle)*(45+65*A)*S,(FMath::Sin(Angle)*50+75*A)*S);
            P->SetVisibility(true);P->SetRelativeLocation(Position);
            P->SetRelativeScale3D(FVector(1.1f*S*(1-A*.5f),1.8f*S*(1-A*.25f),1));
            P->SetRelativeRotation(FRotationMatrix::MakeFromZY((Eye-Position).GetSafeNormal(),FVector::UpVector).Rotator());
            auto* M=Cast<UMaterialInstanceDynamic>(P->GetMaterial(0));
            if(Flame && (!M || M->Parent!=Flame)){M=UMaterialInstanceDynamic::Create(Flame,this);P->SetMaterial(0,M);}
            if(M){M->SetVectorParameterValue(TEXT("Color"),FMath::Lerp(FLinearColor(3,1.1f,.08f),FLinearColor(1.6f,.12f,.01f),A));M->SetScalarParameterValue(TEXT("Opacity"),FMath::SmoothStep(0.f,.1f,Life)*(1-A));}
        }
    }
    if(Resolve && Run.Round==2 && T>=.25f && T<1.f)
    {
        for(int32 I=0;I<FMath::Min(8,Trails.Num());++I)
        {
            const float A=FMath::Clamp((T-.25f-I*.025f)/(.75f-I*.025f),0.f,1.f);
            const FVector Spread(0,FMath::Cos(I*2.4f)*35*S,FMath::Sin(I*2.4f)*25*S);
            const FVector P=FMath::Lerp(Muzzle,Target+Spread,A)+FVector(0,0,FMath::Sin(A*PI)*(60+I*5)*S);
            Energy(Trails[I],P,FVector(.10f*S),FLinearColor(.24f,.85f,.06f),.75f);
        }
    }
    if(Resolve && (Run.Round==1 || Run.Round==3))
    {
        const bool Hot=Run.Round==3;const FLinearColor Color=Hot?Cream:Cyan;
        if(T>=.58f && T<1.18f)
        {
            const FVector D=Impact-Muzzle;
            const float Fade=FMath::Min((T-.58f)/.07f,(1.18f-T)/.18f);
            Energy(Beam,(Muzzle+Impact)*.5f,FVector(.11f*S,.11f*S,D.Size()/100),Color,FMath::Clamp(Fade,0.f,1.f));
            Beam->SetRelativeRotation(FRotationMatrix::MakeFromZ(D).Rotator());
            Fire(4,.70f,Muzzle,.28f,Color);
        }
        // The arc-impact pack leaves dark tangled ribbons on the toy's chest.
        // The first laser already has the luminous core and soft impact rings.
        if(Hot)Fire(1,1.f,Impact,1.25f,Amber);
        if(Hot)
        {
            // Ignite the volume around the robot in a quick outward succession,
            // rather than replacing the entire poison cloud with one impact.
            for(int32 I=0;I<6;++I)
            {
                const float Angle=I*PI/3.f;
                const FVector Spread(18*S,FMath::Cos(Angle)*72*S,FMath::Sin(Angle)*62*S);
                Fire(6+I,1.04f+I*.055f,Target+Spread,.60f,Amber);
            }
            if(T>=1.f && T<1.72f)for(int32 I=0;I<8;++I)
            {
                const float A=FMath::Clamp((T-1.f-I*.025f)/.55f,0.f,1.f);
                const float Angle=I*PI/4.f;
                const FVector P=Impact+FVector(0,FMath::Cos(Angle),FMath::Sin(Angle))*(25+100*A)*S+FVector(0,0,45*A*S);
                Energy(Trails[I],P,FVector(.22f*S*(1-A),.22f*S*(1-A),.45f*S),I%2?Amber:Cream,(1-A)*.65f);
            }
        }
        if(Hot && T>=.58f && T<1.f)for(int32 I=0;I<8;++I)
        {
            const float A=FMath::Clamp((T-.58f)/.42f-I*.035f,0.f,1.f);
            const float Angle=I*2.4f+T*15;
            const FVector P=FMath::Lerp(Muzzle,Impact,A)+FVector(0,FMath::Cos(Angle),FMath::Sin(Angle))*(28*S*FMath::Sin(A*PI));
            Energy(Trails[I],P,FVector(.12f*S),Amber,.7f);
        }
        if(T>=1.75f && T<2.35f)
        {
            const float A=(T-1.75f)/.6f;const FVector P=FMath::Lerp(Target,Eye+FVector(100,0,-20),A)+FVector(0,0,FMath::Sin(A*PI)*35*S);
            Fire(3,1.75f,P,.35f,Amber);StyledFX[3]->SetRelativeLocation(P);
            Energy(Ball,P,FVector((Hot?.40f:.28f)*S),Cream,.65f);
        }
        else if(StyledFX.IsValidIndex(3))StyledFX[3]->Deactivate();
    }
    if(Resolve && Run.Round==4)
    {
        const FVector Source=CardLandingLocal+FVector(0,0,400);
        const float Charge=FMath::SmoothStep(.15f,.65f,T);
        if(T>.15f && T<1.f)
        {
            const float Flight=FMath::Clamp((T-.65f)/.35f,0.f,1.f);
            const FVector P=FMath::Lerp(Source,Impact,Flight);
            Energy(Ball,P,FVector(FMath::Lerp(.35f,4.8f,Charge)*S),Cyan,.82f);
            for(int32 I=0;I<FMath::Min(17,Trails.Num());++I)
            {
                FVector Start=GetCardLanding(I,17)+FVector(0,0,22);
                const float Angle=I*2.39996f+T*10;
                FVector W=T<.65f?FMath::Lerp(Start,Source,Charge):P-(Target-Source).GetSafeNormal()*(I*28*S);
                W+=FVector(0,FMath::Cos(Angle),FMath::Sin(Angle))*(T<.65f?FMath::Sin(Charge*PI)*180*S:45*S);
                const FLinearColor CardColor=I%3==0?Cyan:I%3==1?FLinearColor(.24f,.85f,.06f):Amber;
                Energy(Trails[I],W,FVector((T<.65f?.32f:.55f)*(1-I/25.f)*S),CardColor,.85f);
            }
        }
        Fire(2,1.f,Impact,7.f,Cyan);
        if(T>=1.f && T<2.6f)
        {
            const float A=(T-1.f)/1.6f;
            for(int32 I=0;I<Trails.Num();++I)
            {
                const float Angle=I*2.39996f;
                const FVector D=FVector(FMath::Sin(I*3.1f)*.35f,FMath::Cos(Angle),FMath::Sin(Angle)).GetSafeNormal();
                Energy(Trails[I],Impact+D*(120+1100*A)*S,FVector(.18f,.18f,1.8f)*S,Cyan,(1-A)*.75f);
                Trails[I]->SetRelativeRotation(FRotationMatrix::MakeFromZ(D).Rotator());
            }
        }
    }
    // Readable impact silhouette over the source pack's natural fire/energy
    // motion: a luminous core, expanding soft rings and a brief room light.
    if(Resolve && Run.Round!=2 && T>=1.f && T<(Run.Round==4?2.6f:1.9f))
    {
        const bool Finale=Run.Round==4,Hot=Run.Round==3;
        const float A=(T-1.f)/(Finale?1.6f:.9f);const FLinearColor Color=Hot?Amber:Cyan;
        const float Size=Finale?7.5f:Hot?1.35f:.85f;
        if(T<1.65f)Energy(Ball,Impact,FVector(Size*S*(.5f+A)),Color,FMath::Square(1-A)*.9f);
        for(int32 I=0;I<StyledRings.Num();++I)
        {
            const float R=FMath::Clamp(A-I*.09f,0.f,1.f);if(R<=0)continue;
            auto* Ring=StyledRings[I].Get();const float Diameter=Size*S*(1.f+R*(I?3.f:4.5f));
            Energy(Ring,Impact+FVector(-I*8*S,0,0),FVector(Diameter,Diameter,1),I==1?Cream:Color,FMath::Square(1-R)*(.8f-I*.17f));
            Ring->SetRelativeRotation(FRotationMatrix::MakeFromZ(Eye-Impact).Rotator());
        }
        SpellLight->SetRelativeLocation(Impact);SpellLight->SetLightColor(Color);
        SpellLight->SetAttenuationRadius((Finale?2600:650)*S);SpellLight->SetIntensity((Finale?95000.f:9000.f)*S*(1-A));
    }
    // Each authored burst has a bounded lifetime; source pack loops cannot leak
    // into the next hand or into exploration after collecting the reward.
    if(Resolve && T>2.6f)for(int32 I=0;I<StyledFX.Num();++I)if(I!=5)StyledFX[I]->Deactivate();
    // The reward is the player's physical drill; keep its authored materials.
}
