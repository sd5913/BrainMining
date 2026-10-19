#include "CardRoomStage.h"
#include "CardRoomRobot.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "CardRoomUISettings.h"

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
    UISettings=LoadObject<UCardRoomUISettings>(nullptr,TEXT("/Game/Rooms/CardDuel/UI/DA_CardRoomUISettings.DA_CardRoomUISettings"));
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("RoomOrigin"));
    ToyRoot=CreateDefaultSubobject<USceneComponent>(TEXT("Opponent")); ToyRoot->SetupAttachment(RootComponent); ToyRoot->SetRelativeLocation(FVector(180,0,112));
    RobotOpponent=CreateDefaultSubobject<UChildActorComponent>(TEXT("RobotOpponent"));RobotOpponent->SetupAttachment(ToyRoot);
    RabbitVisual=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RabbitOpponent"));
    RabbitVisual->SetupAttachment(ToyRoot);
    RabbitVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RabbitVisual->SetGenerateOverlapEvents(false);
    RabbitVisual->SetVisibility(false,true);
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
    RobotOpponent->SetChildActorClass(bUseRobotOpponent?ACardRoomRobot::StaticClass():nullptr);
    if(auto* Robot=Cast<ACardRoomRobot>(RobotOpponent->GetChildActor()))Robot->bEncounterDriven=true;
    if(RabbitVisual)
    {
        if(RabbitSkeletalMesh)
        {
            RabbitVisual->SetSkeletalMesh(RabbitSkeletalMesh);
            RabbitVisual->SetRelativeLocation(RabbitVisualOffset);
            // The FBX faces local -Y; the card opponent faces local -X.
            RabbitVisual->SetRelativeRotation(FRotator(0.f,-90.f,0.f));
            RabbitVisual->SetRelativeScale3D(FVector(FMath::Max(.01f,RabbitVisualScale)));
            RabbitVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            RabbitVisual->SetVisibility(true,true);
            RabbitInitialRelativeTransform=RabbitVisual->GetRelativeTransform();
            for(const int32 BodyIndex:{0,1,2,3,7,8,9})
                if(ToyParts.IsValidIndex(BodyIndex) && ToyParts[BodyIndex]) ToyParts[BodyIndex]->SetVisibility(false,true);
            // The imported rabbit has a single plastic face and no usable facial
            // animation in UE. Attach small authored controls to its head bone.
            for(const int32 FaceIndex:{4,5,10,11,12})
                if(ToyParts.IsValidIndex(FaceIndex) && ToyParts[FaceIndex])
                    ToyParts[FaceIndex]->AttachToComponent(RabbitVisual,FAttachmentTransformRules::SnapToTargetNotIncludingScale,TEXT("head"));
            ToyParts[4]->SetRelativeLocation(FVector(4.f,-10.f,39.f));
            ToyParts[5]->SetRelativeLocation(FVector(-4.f,-10.f,39.f));
            ToyParts[10]->SetRelativeLocation(FVector(0.f,-10.f,31.f));
            ToyParts[11]->SetRelativeLocation(FVector(4.f,-10.f,44.f));
            ToyParts[12]->SetRelativeLocation(FVector(-4.f,-10.f,44.f));
            ToyParts[4]->SetRelativeScale3D(FVector(.0015f,.0015f,.0015f));
            ToyParts[5]->SetRelativeScale3D(FVector(.0015f,.0015f,.0015f));
            ToyParts[10]->SetRelativeScale3D(FVector(.0012f,.002f,.0007f));
            ToyParts[11]->SetRelativeScale3D(FVector(.001f,.002f,.0003f));
            ToyParts[12]->SetRelativeScale3D(FVector(.001f,.002f,.0003f));
            for(const int32 FaceIndex:{4,5,10,11,12}) ToyParts[FaceIndex]->SetVisibility(true,true);
            Tint(ToyParts[4],FLinearColor(.02f,.012f,.01f)); Tint(ToyParts[5],FLinearColor(.02f,.012f,.01f));
            Tint(ToyParts[10],FLinearColor(.12f,.015f,.01f)); Tint(ToyParts[11],FLinearColor(.02f,.012f,.01f)); Tint(ToyParts[12],FLinearColor(.02f,.012f,.01f));
            // Existing face pieces remain active so the rabbit still blinks and emotes
            // if the imported FBX's morph targets are unavailable in this UE build.
            for(const int32 FaceIndex:{4,5,6,10,11,12,13,14,15,16,17})
                if(ToyParts.IsValidIndex(FaceIndex) && ToyParts[FaceIndex]) ToyParts[FaceIndex]->SetVisibility(true,true);
        }
        else
        {
            RabbitVisual->SetVisibility(false,true);
        }
    }
    if(bStyledEffects)
    {
        for(auto& Part:ToyParts)
        {
            if(auto* Mesh=StyledToyMeshes.Find(Part->GetFName()))Part->SetStaticMesh(*Mesh);
            if(auto* Material=StyledToyMaterials.Find(Part->GetFName()))Part->SetMaterial(0,*Material);
        }
    }
    if(!bStyledEffects && !ActorHasTag(TEXT("CountrysideP2Art")))
    {
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
    }
    for(const auto& Dot:GhostDots) Tint(Dot,FLinearColor(.4f,.8f,.7f),true);
    for(const auto& Puff:Puffs) Tint(Puff,FLinearColor(.22f,.8f,.08f),true);
    for(const auto& Tail:Trails) Tint(Tail,FLinearColor(.18f,.7f,1.f),true);
    Tint(Beam,FLinearColor(.3f,.85f,1.f),true); Tint(Ball,FLinearColor(1.f,.26f,.04f),true);
    Reward->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Drill/Stylized/SM_DrillStylized.SM_DrillStylized")));
    Reward->EmptyOverrideMaterials();
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
    if(RabbitVisual && RabbitSkeletalMesh)
    {
        RabbitInitialRelativeTransform=RabbitVisual->GetRelativeTransform();
        RabbitVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        RabbitVisual->SetSimulatePhysics(false);
        bRabbitPhysicsDropped=false;
        bRabbitPhysicsSettled=false;
    }
    InitializeStyledEffects();
}

void ACardRoomStage::ResetRabbitPhysicsDrop()
{
    if(!RabbitVisual || !RabbitSkeletalMesh) return;
    RabbitVisual->SetSimulatePhysics(false);
    RabbitVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RabbitVisual->AttachToComponent(ToyRoot,FAttachmentTransformRules::KeepRelativeTransform);
    RabbitVisual->SetRelativeTransform(RabbitInitialRelativeTransform);
    RabbitVisual->SetVisibility(true,true);
    bRabbitPhysicsDropped=false;
    bRabbitPhysicsSettled=false;
}

void ACardRoomStage::TriggerRabbitPhysicsDrop()
{
    if(!bRabbitPhysicsDrop || bRabbitPhysicsDropped || !RabbitVisual || !RabbitSkeletalMesh) return;
    const FVector Launch=RabbitVisual->GetComponentLocation();
    RabbitVisual->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    RabbitVisual->SetCollisionProfileName(TEXT("PhysicsActor"));
    RabbitVisual->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    RabbitVisual->SetSimulatePhysics(true);
    RabbitVisual->SetEnableGravity(true);
    // True free fall: no damping or hidden easing. The existing chair collision
    // is responsible for stopping the rabbit when it lands.
    RabbitVisual->SetLinearDamping(0.f);
    RabbitVisual->SetAngularDamping(0.f);
    RabbitVisual->SetUseCCD(true);
    RabbitVisual->SetPhysicsLinearVelocity(FVector(0.f,0.f,-120.f));
    RabbitVisual->SetPhysicsAngularVelocityInDegrees(FVector(0.f,0.f,35.f));
    bRabbitPhysicsDropped=true;
    bRabbitPhysicsSettled=false;
    UE_LOG(LogTemp,Log,TEXT("CardRoom rabbit physics drop at %s"),*Launch.ToString());
}

void ACardRoomStage::TickRabbitExpression(float Time,float DeltaSeconds)
{
    if(!RabbitVisual || !RabbitSkeletalMesh || bRabbitPhysicsDropped) return;
    // The prepared FBX adds lightweight Blink/MouthOpen/Angry targets; bones provide the
    // larger readable gesture so the character still works if a target is stripped by import.
    const float Breath=FMath::Sin(Time*2.25f);
    const bool Angry=Run.Round>=3 && (Run.Phase==ECardRoomPhase::Resolving || Run.Phase==ECardRoomPhase::Victory);
    const bool Hurt=Run.Phase==ECardRoomPhase::Resolving && Run.PhaseTime>=1.f && Run.PhaseTime<1.75f;
    const float Blink=(FMath::Fmod(Time,4.1f)<.14f)?1.f:0.f;
    RabbitVisual->SetMorphTarget(TEXT("Blink"),Blink);
    RabbitVisual->SetMorphTarget(TEXT("MouthOpen"),(Angry||Hurt)?1.f:0.f);
    RabbitVisual->SetMorphTarget(TEXT("Angry"),Angry?1.f:0.f);
    RabbitVisual->SetRelativeRotation(FRotator(0.f,-90.f+FMath::Sin(Time*.9f)*1.5f+(Angry?-4.f:0.f),FMath::Sin(Time*2.1f)*2.f));
}
void ACardRoomStage::Sound(const TCHAR* Name,float Volume)
{
    if(AudioOverride.IsBound()) {AudioOverride.Execute(FName(Name),Volume);return;}
    if(FCString::Strcmp(Name,TEXT("S_Victory"))==0) Name=TEXT("S_Reward");
    const FString Path=FString::Printf(TEXT("/Game/Rooms/CardDuel/Audio/%s.%s"),Name,Name);
    if(USoundBase* Cue=LoadObject<USoundBase>(nullptr,*Path)) UGameplayStatics::PlaySound2D(this,Cue,Volume);
}
bool ACardRoomStage::BeginEncounter() { return Run.Seat(); }
bool ACardRoomStage::CollectReward()
{
    if(!Run.Collect()) return false;
    Sound(TEXT("S_Reward")); OnCollectibleClaimed.Broadcast(RoomId); return true;
}
void ACardRoomStage::RestartEncounter() { Run.Restart(); ResetRabbitPhysicsDrop(); bWinSent=false; bReplySound=false; bImpactSent=false; }
bool ACardRoomStage::IsPlayerNear(const FVector& Position) const
{
    return FVector::Dist2D(Position,GetApproachLocation())<GetApproachRadius() && FMath::Abs(Position.Z-GetApproachLocation().Z)<220;
}
void ACardRoomStage::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); Run.Tick(DeltaSeconds);
    const auto Phase=Run.Phase;
    if(Phase!=LastPhase)
    {
        PreviousAudioTime=0;
        if(SubtitleOverride.IsBound() && Phase==ECardRoomPhase::Matching)SubtitleOverride.Execute(TEXT("正在匹配中"),2.2f);
        if(SubtitleOverride.IsBound() && Phase==ECardRoomPhase::Arrival)SubtitleOverride.Execute(TEXT("匹配成功！"),1.4f);
        if(Phase==ECardRoomPhase::Arrival) Sound(TEXT("S_Arrival"));
        if(Phase==ECardRoomPhase::Choosing) Sound(TEXT("S_Draw"),.3f);
        if(Phase==ECardRoomPhase::Seated && AudioOverride.IsBound()) Sound(TEXT("S_Seat"),.4f);
        if(Phase==ECardRoomPhase::Matching && AudioOverride.IsBound()) Sound(TEXT("S_Confirm"),.35f);
        if(Phase==ECardRoomPhase::Resolving) { Sound(Run.Round==2?TEXT("S_Poison"):Run.Round==4?TEXT("S_Finale"):TEXT("S_Laser")); if(AudioOverride.IsBound())Sound(TEXT("S_CardPlace"),.35f); bReplySound=false; }
        if(Phase==ECardRoomPhase::Victory && !bWinSent) { bWinSent=true; Sound(TEXT("S_Victory")); OnRoomCompleted.Broadcast(RoomId); }
        LastPhase=Phase;
    }
    const float T=Run.PhaseTime;
    const bool bBattle=Phase==ECardRoomPhase::Choosing || Phase==ECardRoomPhase::Resolving;
    const bool bResolve=Phase==ECardRoomPhase::Resolving;
    if(AudioOverride.IsBound() && bResolve && PreviousAudioTime<1.f && T>=1.f)
        Sound(Run.Round==3?TEXT("S_CardFire"):Run.Round==2?TEXT("S_CardPoison"):TEXT("S_Impact"),Run.Round==3?.7f:.3f);
    if(SubtitleOverride.IsBound() && bResolve && PreviousAudioTime<1.f && T>=1.f)
    {
        if(Run.Round==2) SubtitleOverride.Execute(TEXT("呕——好臭臭"),2.4f);
        if(Run.Round==3) SubtitleOverride.Execute(TEXT("啊啊啊！我的眉毛！"),2.4f);
        if(Run.Round==4) SubtitleOverride.Execute(TEXT("我一定会回来的！——"),3.f);
    }
    PreviousAudioTime=T;
    const bool bGhost=Phase==ECardRoomPhase::Seated || Phase==ECardRoomPhase::Matching || Phase==ECardRoomPhase::Victory || Phase==ECardRoomPhase::Collected;
    for(const auto& Dot:GhostDots) Dot->SetVisibility(bGhost);
    const bool bOpponentVisible=RabbitSkeletalMesh ? (Phase!=ECardRoomPhase::Exploring && Phase!=ECardRoomPhase::Collected) : (bBattle || Phase==ECardRoomPhase::Arrival);
    ToyRoot->SetVisibility(bOpponentVisible,true);
    // Keep the authored P2 replacement authoritative at runtime. Older saved
    // instances can retain component visibility overrides from the static toy;
    // re-apply the skeletal mesh and hide every legacy toy part before drawing.
    if(RabbitSkeletalMesh && RabbitVisual)
    {
        if(RabbitVisual->GetSkeletalMeshAsset()!=RabbitSkeletalMesh)
            RabbitVisual->SetSkeletalMesh(RabbitSkeletalMesh);
        RabbitVisual->SetVisibility(bOpponentVisible,true);
        // Keep the static body hidden, but retain the small authored eye/mouth
        // pieces attached to the rabbit head as the animation fallback.
        for(const int32 BodyIndex:{0,1,2,3,7,8,9})
            if(ToyParts.IsValidIndex(BodyIndex) && ToyParts[BodyIndex]) ToyParts[BodyIndex]->SetVisibility(false,true);
    }
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
    const float FaceWidth=RabbitSkeletalMesh?.0015f:.085f;
    const float FaceDepth=RabbitSkeletalMesh?.0015f:(Panic?.18f:.12f);
    const float MouthWidth=RabbitSkeletalMesh?.0012f:.075f;
    const float MouthHeight=RabbitSkeletalMesh?.0007f:(Hurt?.15f:Sick?.10f:Panic?.20f:Angry?.09f:.035f);
    ToyParts[4]->SetRelativeScale3D(FVector(FaceWidth,FaceDepth,RabbitSkeletalMesh?FaceWidth:EyeHeight));
    ToyParts[5]->SetRelativeScale3D(FVector(FaceWidth,FaceDepth,RabbitSkeletalMesh?FaceWidth:EyeHeight));
    ToyParts[10]->SetRelativeScale3D(FVector(MouthWidth,RabbitSkeletalMesh?.002f:(Hurt?.17f:Sick?.13f:Panic?.18f:.11f),MouthHeight));
    for(int32 i=0;i<2;++i)
    {
        const float Side=i==0?-1.f:1.f;
        ToyParts[11+i]->SetRelativeRotation(FRotator(0,0,Side*(Angry?32.f:Sick?-25.f:Hurt?-36.f:Panic?-18.f:-7.f)));
        if(!RabbitSkeletalMesh) ToyParts[11+i]->SetRelativeLocation(FVector(-38,Side*17,23+(Panic?6.f:Hurt?4.f:0.f)));
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
        if(CardFaceMaterials.IsValidIndex(static_cast<int32>(Type)))
            CardMarks[i]->SetMaterial(0,CardFaceMaterials[static_cast<int32>(Type)]);
        else
            CardMarks[i]->SetStaticMesh(SymbolMeshes[static_cast<int32>(Type)]);
    }
    if(Phase==ECardRoomPhase::Arrival) Offset.Z+=FMath::Square(1-FMath::Clamp(T/1.0f,0.f,1.f))*430;
    if(bResolve && T>=1 && T<1.7f) { Offset.Y+=FMath::Sin(T*60)*7; Spin.Pitch=FMath::Sin(T*45)*8; }
    if(Run.bPoisoned) { Spin.Pitch=15; Offset.Y+=FMath::Sin(GetWorld()->GetTimeSeconds()*3)*4; }
    bool bReachedWall=false;
    if(bResolve && Run.Round==4 && T>1)
    {
        const float Fly=FMath::Clamp((T-1)/1.7f,0.f,1.f);
        const FVector DropAboveBench=RabbitLandingLocal+FVector(0.f,0.f,900.f);
        const FVector ExitTarget=RabbitSkeletalMesh && !bUseRobotOpponent ? DropAboveBench : OpponentExitLocal;
        Offset=FMath::Lerp(Offset,ExitTarget,Fly);Offset.Z+=FMath::Sin(Fly*PI)*120;Spin.Roll=Fly*600;
        Offset-=Spin.RotateVector(OpponentExitCenterOffset*ToyRoot->GetRelativeScale3D())*Fly;
        if(Fly>=.78f && !bUseRobotOpponent) TriggerRabbitPhysicsDrop();
        bReachedWall=Fly>=1;
    }
    ToyRoot->SetRelativeLocation(Offset); ToyRoot->SetRelativeRotation(Spin);
    if(bReachedWall) {ToyRoot->SetVisibility(false,true);if(!bImpactSent){bImpactSent=true;OnOpponentWallImpact.Broadcast(RoomId);}}
    if(bRabbitPhysicsDropped && !bRabbitPhysicsSettled && RabbitVisual)
    {
        const FVector Landing=GetActorTransform().TransformPosition(RabbitLandingLocal);
        const FVector Current=RabbitVisual->GetComponentLocation();
        const FVector Horizontal(Current.X-Landing.X,Current.Y-Landing.Y,0.f);
        if(Current.Z<=Landing.Z+140.f && Horizontal.SizeSquared()<=300.f*300.f && RabbitVisual->GetComponentVelocity().SizeSquared()<35.f*35.f)
        {
            RabbitVisual->SetPhysicsLinearVelocity(FVector::ZeroVector);
            RabbitVisual->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            RabbitVisual->SetSimulatePhysics(false);
            RabbitVisual->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            RabbitVisual->SetWorldLocation(Landing);
            bRabbitPhysicsSettled=true;
        }
    }
    if(auto* PC=GetWorld()->GetFirstPlayerController())
    {
        FVector Eye;FRotator Aim;PC->GetPlayerViewPoint(Eye,Aim);
        const FVector To=ToyRoot->GetComponentTransform().InverseTransformPosition(Eye)-FVector(0,0,105);
        const float Pitch=FMath::RadiansToDegrees(FMath::Atan2(-To.Z,FMath::Max(1.,-To.X)));
        const float Yaw=FMath::RadiansToDegrees(FMath::Atan2(-To.Y,FMath::Max(1.,-To.X)));
        HeadPivot->SetRelativeRotation(FMath::RInterpTo(HeadPivot->GetRelativeRotation(),FRotator(FMath::Clamp(Pitch,-20.f,20.f),FMath::Clamp(Yaw,-30.f,30.f),0),DeltaSeconds,3.f));
    }
    TickRabbitExpression(Time,DeltaSeconds);
    if(bUseRobotOpponent)
    {
        for(const auto& Part:ToyParts)Part->SetVisibility(false,true);
        RabbitVisual->SetVisibility(false,true);
        if(auto* Robot=Cast<ACardRoomRobot>(RobotOpponent->GetChildActor()))
        {
            // Child actors recreated during level load must not also run their idle pose tick.
            Robot->bEncounterDriven=true;
            Robot->SetActorHiddenInGame(!(bBattle || Phase==ECardRoomPhase::Arrival) || bReachedWall);
            Robot->SetActorEnableCollision(false);
            const bool Fly=bResolve && Run.Round==4 && T>1;
            const FName State=Fly?TEXT("Fly"):Hurt?TEXT("Hurt"):bResolve&&T>=1.75f?TEXT("Attack"):Panic?TEXT("Panic"):Sick?TEXT("Sick"):Phase==ECardRoomPhase::Arrival?TEXT("Arrival"):TEXT("Idle");
            Robot->Pose(State,Time,DeltaSeconds,HeadPivot->GetRelativeRotation());
        }
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
