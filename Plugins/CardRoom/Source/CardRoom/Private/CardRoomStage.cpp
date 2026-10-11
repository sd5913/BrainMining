#include "CardRoomStage.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

UStaticMeshComponent* ACardRoomStage::Shape(FName Name,USceneComponent* Parent,const TCHAR* Path,FVector Position,FVector Scale)
{
    UStaticMeshComponent* Part=CreateDefaultSubobject<UStaticMeshComponent>(Name);
    Part->SetupAttachment(Parent); Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Path));
    Part->SetRelativeLocation(Position); Part->SetRelativeScale3D(Scale);
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); Part->SetCastShadow(false); return Part;
}
ACardRoomStage::ACardRoomStage()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("RoomOrigin"));
    ToyRoot=CreateDefaultSubobject<USceneComponent>(TEXT("Opponent")); ToyRoot->SetupAttachment(RootComponent); ToyRoot->SetRelativeLocation(FVector(180,0,112));
    const TCHAR* Sphere=TEXT("/Engine/BasicShapes/Sphere.Sphere");
    const TCHAR* Cube=TEXT("/Engine/BasicShapes/Cube.Cube");
    ToyParts.Add(Shape(TEXT("VelvetBody"),ToyRoot,Sphere,FVector(0,0,35),FVector(.70,.76,.85)));
    ToyParts.Add(Shape(TEXT("RoundHead"),ToyRoot,Sphere,FVector(-6,0,94),FVector(.75,.82,.75)));
    ToyParts.Add(Shape(TEXT("LeftEar"),ToyRoot,Sphere,FVector(-1,-27,133),FVector(.25,.25,.44)));
    ToyParts.Add(Shape(TEXT("RightEar"),ToyRoot,Sphere,FVector(-1,27,133),FVector(.25,.25,.44)));
    ToyParts.Add(Shape(TEXT("LeftEye"),ToyRoot,Sphere,FVector(-39,-17,102),FVector(.08,.12,.13)));
    ToyParts.Add(Shape(TEXT("RightEye"),ToyRoot,Sphere,FVector(-39,17,102),FVector(.08,.12,.13)));
    ToyParts.Add(Shape(TEXT("Nose"),ToyRoot,Sphere,FVector(-45,0,90),FVector(.09,.13,.09)));
    ToyParts.Add(Shape(TEXT("LeftMitten"),ToyRoot,Sphere,FVector(-23,-49,40),FVector(.27,.27,.40)));
    ToyParts.Add(Shape(TEXT("RightMitten"),ToyRoot,Sphere,FVector(-23,49,40),FVector(.27,.27,.40)));
    ToyParts.Add(Shape(TEXT("BowTie"),ToyRoot,Cube,FVector(-37,0,66),FVector(.10,.40,.13)));
    ToyParts.Add(Shape(TEXT("Mouth"),ToyRoot,Sphere,FVector(-43,0,78),FVector(.06,.22,.065)));
    ToyParts.Add(Shape(TEXT("LeftBrow"),ToyRoot,Cube,FVector(-38,-17,113),FVector(.05,.20,.04)));
    ToyParts.Add(Shape(TEXT("RightBrow"),ToyRoot,Cube,FVector(-38,17,113),FVector(.05,.20,.04)));
    HeadPivot=CreateDefaultSubobject<USceneComponent>(TEXT("OpponentHeadAim"));HeadPivot->SetupAttachment(ToyRoot);HeadPivot->SetRelativeLocation(FVector(0,0,94));
    for(int32 Index:{1,2,3,4,5,6,10,11,12})
    {
        const FVector Old=ToyParts[Index]->GetRelativeLocation();ToyParts[Index]->SetupAttachment(HeadPivot);ToyParts[Index]->SetRelativeLocation(Old-FVector(0,0,94));
    }
    // Keep the muzzle, smile and nose on the same aiming pivot as the head.
    ToyParts.Add(Shape(TEXT("LeftMuzzle"),HeadPivot,Sphere,FVector(-42,-9,-5),FVector(.14,.24,.18)));
    ToyParts.Add(Shape(TEXT("RightMuzzle"),HeadPivot,Sphere,FVector(-42,9,-5),FVector(.14,.24,.18)));
    ToyParts.Add(Shape(TEXT("SmileLeft"),HeadPivot,Cube,FVector(-49,-4,-11),FVector(.035,.095,.025)));
    ToyParts.Add(Shape(TEXT("SmileRight"),HeadPivot,Cube,FVector(-49,4,-11),FVector(.035,.095,.025)));
    ToyParts.Add(Shape(TEXT("NoseStem"),HeadPivot,Cube,FVector(-49,0,-5),FVector(.035,.022,.07)));
    for(int32 i=0;i<17;++i)
    {
        auto* Card=Shape(*FString::Printf(TEXT("PlayedCard%d"),i),RootComponent,Cube,FVector::ZeroVector,FVector(.38,.26,.006));
        PlayedCards.Add(Card);
        CardMarks.Add(Shape(*FString::Printf(TEXT("CardMark%d"),i),Card,Cube,FVector(0,0,70),FVector(.55,.6,1)));
    }
    for(int32 i=0;i<16;++i)
    {
        float A=i*2*PI/16;
        GhostDots.Add(Shape(*FString::Printf(TEXT("Ghost%d"),i),RootComponent,Sphere,FVector(166,FMath::Cos(A)*43,195+FMath::Sin(A)*60),FVector(.055)));
    }
    for(int32 i=0;i<10;++i)
    {
        Puffs.Add(Shape(*FString::Printf(TEXT("Gas%d"),i),RootComponent,Sphere,FVector::ZeroVector,FVector(.2)));
        Trails.Add(Shape(*FString::Printf(TEXT("Tail%d"),i),RootComponent,Sphere,FVector::ZeroVector,FVector(.2)));
    }
    SpellLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("SpellLight"));
    SpellLight->SetupAttachment(RootComponent); SpellLight->SetIntensity(0); SpellLight->SetAttenuationRadius(430); SpellLight->SetCastShadows(false);
    for(int32 i=0;i<160;++i)
        FxShards.Add(Shape(*FString::Printf(TEXT("FxShard%d"),i),RootComponent,Cube,FVector::ZeroVector,FVector(.1)));
    Beam=Shape(TEXT("Laser"),RootComponent,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),FVector::ZeroVector,FVector(.1));
    Ball=Shape(TEXT("SpellOrb"),RootComponent,Sphere,FVector::ZeroVector,FVector(.3));
    Reward=Shape(TEXT("CollectibleOrb"),RootComponent,Sphere,FVector(0,0,135),FVector(.23));
}
void ACardRoomStage::Tint(UStaticMeshComponent* Part,FLinearColor Color,bool bGlow)
{
    UMaterialInterface* Base=LoadObject<UMaterialInterface>(nullptr,bGlow?TEXT("/Game/Rooms/CardDuel/Materials/M_CardRoomGlow.M_CardRoomGlow"):TEXT("/Game/Rooms/CardDuel/Materials/M_CardRoomSurface.M_CardRoomSurface"));
    if(!Base) return;
    UMaterialInstanceDynamic* MID=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0));
    if(!MID || MID->Parent!=Base) { MID=UMaterialInstanceDynamic::Create(Base,this); Part->SetMaterial(0,MID); }
    MID->SetVectorParameterValue(TEXT("Color"),Color);
}
void ACardRoomStage::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    for(int32 i=0;i<ToyParts.Num();++i) Tint(ToyParts[i],((i>=4&&i<=6)||i>=10)?FLinearColor(.025f,.016f,.013f):i==9?FLinearColor(.65f,.19f,.09f):FLinearColor(.73f,.43f,.24f));
    // Reset authored transforms too: older map component overrides must not detach the face.
    HeadPivot->SetRelativeLocation(FVector(0,0,105));
    ToyParts[1]->SetRelativeLocation(FVector(-6,0,0));
    for(int32 Side=0;Side<2;++Side)
    {
        const float Sign=Side==0?-1.f:1.f;
        ToyParts[2+Side]->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cone.Cone")));
        ToyParts[2+Side]->SetRelativeLocation(FVector(-1,Sign*27,36));
        ToyParts[2+Side]->SetRelativeScale3D(FVector(.32,.38,.46));
        ToyParts[4+Side]->SetRelativeLocation(FVector(-39,Sign*17,12));
        ToyParts[13+Side]->SetRelativeLocation(FVector(-42,Sign*9,-5));
        Tint(ToyParts[13+Side],FLinearColor(.92f,.75f,.51f));
        ToyParts[15+Side]->SetRelativeRotation(FRotator(0,0,Sign*18));
    }
    ToyParts[6]->SetRelativeLocation(FVector(-49,0,2));
    ToyParts[6]->SetRelativeScale3D(FVector(.08,.115,.08));
    Tint(ToyParts[6],FLinearColor(.42f,.09f,.09f));
    ToyParts[10]->SetRelativeLocation(FVector(-49,0,-13));
    ToyParts[9]->SetRelativeLocation(FVector(-36,0,58));
    ToyParts[9]->SetRelativeScale3D(FVector(.08,.28,.08));
    Tint(ToyParts[9],FLinearColor(.22f,.035f,.025f));
    for(const auto& Dot:GhostDots) Tint(Dot,FLinearColor(.4f,.8f,.7f),true);
    for(const auto& Puff:Puffs) Tint(Puff,FLinearColor(.22f,.8f,.08f),true);
    for(const auto& Tail:Trails) Tint(Tail,FLinearColor(.18f,.7f,1.f),true);
    Tint(Beam,FLinearColor(.3f,.85f,1.f),true); Tint(Ball,FLinearColor(1.f,.26f,.04f),true); Tint(Reward,FLinearColor(.25f,.9f,1.f),true);
    Beam->SetVisibility(false); Ball->SetVisibility(false); Reward->SetVisibility(false);
    for(const auto& Part:Puffs) Part->SetVisibility(false);
    for(const auto& Part:Trails) Part->SetVisibility(false);
    for(const auto& Shard:FxShards) Shard->SetVisibility(false);
    ToyRoot->SetVisibility(false,true);
    for(const auto& Card:PlayedCards) { Tint(Card,FLinearColor(.95f,.85f,.62f)); Card->SetVisibility(false,true); }

}
void ACardRoomStage::BeginPlay()
{
    Super::BeginPlay();
    // Native component lists are transient: old map arrays must not override new native parts.
    // Initialize new component materials even when loading an already-saved level.
    if(!ensure(ToyParts.Num()==18 && PlayedCards.Num()==17 && CardMarks.Num()==17))
    {
        SetActorTickEnabled(false); return;
    }
    InitializeEffectMeshes();
    OnConstruction(GetActorTransform());
}
void ACardRoomStage::Sound(const TCHAR* Name,float Volume)
{
    const FString Path=FString::Printf(TEXT("/Game/Rooms/CardDuel/Audio/%s.%s"),Name,Name);
    if(USoundBase* Cue=LoadObject<USoundBase>(nullptr,*Path)) UGameplayStatics::PlaySound2D(this,Cue,Volume);
}
bool ACardRoomStage::BeginEncounter() { return Run.Seat(); }
bool ACardRoomStage::CollectReward()
{
    if(!Run.Collect()) return false;
    Sound(TEXT("S_Reward")); OnCollectibleClaimed.Broadcast(RoomId); return true;
}
void ACardRoomStage::RestartEncounter() { Run.Restart(); bWinSent=false; bReplySound=false; bImpactSent=false; }
bool ACardRoomStage::IsPlayerNear(const FVector& Position) const
{
    return FVector::Dist2D(Position,GetApproachLocation())<180 && FMath::Abs(Position.Z-GetApproachLocation().Z)<220;
}
void ACardRoomStage::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); Run.Tick(DeltaSeconds);
    const auto Phase=Run.Phase;
    if(Phase!=LastPhase)
    {
        if(Phase==ECardRoomPhase::Arrival) Sound(TEXT("S_Arrival"));
        if(Phase==ECardRoomPhase::Choosing) Sound(TEXT("S_Draw"),.3f);
        if(Phase==ECardRoomPhase::Resolving) { Sound(Run.Round==2?TEXT("S_Poison"):Run.Round==4?TEXT("S_Finale"):TEXT("S_Laser")); bReplySound=false; }
        if(Phase==ECardRoomPhase::Victory && !bWinSent) { bWinSent=true; Sound(TEXT("S_Reward")); OnRoomCompleted.Broadcast(RoomId); }
        LastPhase=Phase;
    }
    const float T=Run.PhaseTime;
    const bool bBattle=Phase==ECardRoomPhase::Choosing || Phase==ECardRoomPhase::Resolving;
    const bool bResolve=Phase==ECardRoomPhase::Resolving;
    const bool bGhost=Phase==ECardRoomPhase::Seated || Phase==ECardRoomPhase::Matching || Phase==ECardRoomPhase::Victory || Phase==ECardRoomPhase::Collected;
    for(const auto& Dot:GhostDots) Dot->SetVisibility(bGhost);
    ToyRoot->SetVisibility(bBattle || Phase==ECardRoomPhase::Arrival,true);
    const float Time=GetWorld()->GetTimeSeconds();
    const float Breath=FMath::Sin(Time*2.25f);
    FVector Offset=OpponentBaseLocal+FVector(0,0,Breath*1.6f); FRotator Spin(0,0,FMath::Sin(Time*1.1f)*1.2f);
    ToyRoot->SetRelativeScale3D(FVector(1.f,1.f+Breath*.012f,1.f+Breath*.015f)*FMath::Max(.1f,OpponentScale));
    const bool Hurt=bResolve && T>=1.f && T<1.75f;
    const bool Angry=bResolve && Run.Round==3 && T>=1.75f;
    const bool Panic=(Run.Round==4 && (bResolve || Run.Staged.Num()>4));
    const bool Sick=Run.bPoisoned;
    const bool Blink=FMath::Fmod(Time,4.1f)<.14f;
    float EyeHeight=Blink?.018f:Hurt?.035f:Sick?.05f:Panic?.24f:Angry?.085f:.13f;
    ToyParts[4]->SetRelativeScale3D(FVector(.085f,Panic?.18f:.12f,EyeHeight));
    ToyParts[5]->SetRelativeScale3D(FVector(.085f,Panic?.18f:.12f,EyeHeight));
    ToyParts[10]->SetRelativeScale3D(FVector(.075f,Hurt?.17f:Sick?.13f:Panic?.18f:.11f,Hurt?.15f:Sick?.10f:Panic?.20f:Angry?.09f:.035f));
    for(int32 i=0;i<2;++i)
    {
        const float Side=i==0?-1.f:1.f;
        ToyParts[11+i]->SetRelativeRotation(FRotator(0,0,Side*(Angry?32.f:Sick?-25.f:Hurt?-36.f:Panic?-18.f:-7.f)));
        ToyParts[11+i]->SetRelativeLocation(FVector(-38,Side*17,23+(Panic?6.f:Hurt?4.f:0.f)));
        ToyParts[2+i]->SetRelativeRotation(FRotator(Sick?25.f:Breath*4.f,0,Side*(Hurt?38.f:Panic?23.f:Angry?-12.f:4.f)));
        ToyParts[7+i]->SetRelativeLocation(FVector(-23,Side*(Panic?60.f:49.f),40+(Panic?35.f:Angry?22.f:Breath*2.f)));
    }
    // Selected cards become physical cards and arc onto the table on confirm.
    for(int32 i=0;i<PlayedCards.Num();++i)
    {
        const bool Visible=bResolve && Run.Hand.IsValidIndex(i) && T<2.2f;
        PlayedCards[i]->SetVisibility(Visible,true);
        if(!Visible) continue;
        const float A=FMath::Clamp((T-i*.008f)/.62f,0.f,1.f);
        const float Centered=i-(Run.Hand.Num()-1)*.5f;
        const FVector From(-220,Centered*6.f,138.f);
        const FVector To=GetCardLanding(i,Run.Hand.Num());
        FVector Pos=FMath::Lerp(From,To,A); Pos.Z+=FMath::Sin(A*PI)*CardThrowHeight;
        PlayedCards[i]->SetRelativeLocation(Pos);
        // Present the illustrated face during the arc, then flatten only near touchdown.
        const float AirPitch=75.f+FMath::Sin(A*PI)*8.f;
        const float LandingBlend=FMath::SmoothStep(.70f,1.f,A);
        PlayedCards[i]->SetRelativeRotation(FRotator(FMath::Lerp(AirPitch,CardLandingPitch,LandingBlend),(1-A)*(Centered*3.f-35.f),0));
        const float Fade=T>1.9f?FMath::Max(.01f,(2.2f-T)/.3f):1.f;
        PlayedCards[i]->SetRelativeScale3D(FVector(.38,.26,.006)*CardVisualScale*Fade);
        const auto Type=Run.Hand[i];
        // Symbol meshes carry their own layered palette, matching the hand icons.
        CardMarks[i]->SetStaticMesh(SymbolMeshes[static_cast<int32>(Type)]);
    }
    if(Phase==ECardRoomPhase::Arrival) Offset.Z+=FMath::Square(1-FMath::Clamp(T/1.0f,0.f,1.f))*430;
    if(bResolve && T>=1 && T<1.7f) { Offset.Y+=FMath::Sin(T*60)*7; Spin.Pitch=FMath::Sin(T*45)*8; }
    if(Run.bPoisoned) { Spin.Pitch=15; Offset.Y+=FMath::Sin(GetWorld()->GetTimeSeconds()*3)*4; }
    bool bReachedWall=false;
    if(bResolve && Run.Round==4 && T>1)
    {
        const float Fly=FMath::Clamp((T-1)/1.7f,0.f,1.f);
        Offset=FMath::Lerp(Offset,OpponentExitLocal,Fly);Offset.Z+=FMath::Sin(Fly*PI)*120;Spin.Roll=Fly*600;
        Offset-=Spin.RotateVector(OpponentExitCenterOffset*ToyRoot->GetRelativeScale3D())*Fly;
        bReachedWall=Fly>=1;
    }
    ToyRoot->SetRelativeLocation(Offset); ToyRoot->SetRelativeRotation(Spin);
    if(bReachedWall) {ToyRoot->SetVisibility(false,true);if(!bImpactSent){bImpactSent=true;OnOpponentWallImpact.Broadcast(RoomId);}}
    if(auto* PC=GetWorld()->GetFirstPlayerController())
    {
        FVector Eye;FRotator Aim;PC->GetPlayerViewPoint(Eye,Aim);
        const FVector To=ToyRoot->GetComponentTransform().InverseTransformPosition(Eye)-FVector(0,0,105);
        const float Pitch=FMath::RadiansToDegrees(FMath::Atan2(-To.Z,FMath::Max(1.,-To.X)));
        const float Yaw=FMath::RadiansToDegrees(FMath::Atan2(-To.Y,FMath::Max(1.,-To.X)));
        HeadPivot->SetRelativeRotation(FMath::RInterpTo(HeadPivot->GetRelativeRotation(),FRotator(FMath::Clamp(Pitch,-20.f,20.f),FMath::Clamp(Yaw,-30.f,30.f),0),DeltaSeconds,3.f));
    }
    TickEffects();
}

FVector ACardRoomStage::GetCardLanding(int32 Index,int32 Count) const
{
    const float Unit=FMath::Max(1.f,CardVisualScale);
    const float X=Count>6?(Index/6-1)*42.f*Unit:0.f;
    const float Y=Count>6?(Index%6-2.5f)*31.f*Unit:(Index-(Count-1)*.5f)*32.f*Unit;
    return CardLandingLocal+FRotator(CardLandingPitch,0,0).RotateVector(FVector(X,Y,1.2f+Index*.035f));
}
