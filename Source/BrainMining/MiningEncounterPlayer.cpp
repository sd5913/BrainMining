#include "MiningEncounterPlayer.h"
#include "MiningComponent.h"
#include "MiningToolEffectsComponent.h"
#include "MiningRoomEscape.h"
#include "MiningGiantRoomEntrance.h"
#include "CardRoomStage.h"
#include "Components/InputComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "EchoPassagePlayer.h"
#include "EchoPassageStage.h"
#include "EngineUtils.h"
#include "CardRoomPlayer.h"
#include "Components/LightComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "UObject/ConstructorHelpers.h"

void UMiningEncounterPlayerPolicy::Apply(APawn* Pawn)
{
    if(AppliedPawn.Get()==Pawn)return;
    Release();AppliedPawn=Pawn;if(!Pawn)return;
    Mining=Pawn->FindComponentByClass<UMiningComponent>();
    Movement=Pawn->FindComponentByClass<UMiningCharacterMovementComponent>();
    if(Mining.IsValid())
    {
        bOldInputContextEnabled=Mining->IsInputContextEnabled();Mining->SetInputContextEnabled(false);
        // Echo owns restart/fall recovery. A high PIE spawn (e.g. on the desk)
        // must not become mining's fall-reset target when excavation is enabled
        // again at the low passage exit. Keep ordinary mining maps unchanged.
        if(GetWorld() && TActorIterator<AEchoPassageStage>(GetWorld()))
        {
            OldFallResetDistance=Mining->FallResetDistance;
            Mining->FallResetDistance=0.f;bFallResetOverridden=true;
        }
        Mining->PushEncounterBlock();bMiningBlocked=true;
    }
    // Flight and heat remain owned by the shared player progression.
    if(auto* Character=Cast<ACharacter>(Pawn))Character->StopJumping();
    TArray<ULightComponent*> Found;Pawn->GetComponents(Found);
    for(auto* Light:Found)
    {
        // Turn off the miner's lamps for authored black/warm room lighting only.
        if(Light->GetFName()!=TEXT("MiningLight") && Light->GetFName()!=TEXT("MiningFillLight"))continue;
        Lights.Emplace(Light,Light->IsVisible());Light->SetVisibility(false);
    }
}
void UMiningEncounterPlayerPolicy::Release()
{
    if(Mining.IsValid())
    {
        Mining->CancelMining();if(bMiningBlocked)Mining->PopEncounterBlock();
        if(bFallResetOverridden)Mining->FallResetDistance=OldFallResetDistance;
        Mining->SetInputContextEnabled(bOldInputContextEnabled);
    }
    bMiningBlocked=false;bFallResetOverridden=false;
    if(Movement.IsValid()) Movement->ResetFlightInput();
    for(auto& Light:Lights)if(Light.Key.IsValid())Light.Key->SetVisibility(Light.Value);
    Lights.Reset();Movement.Reset();Mining.Reset();AppliedPawn.Reset();
}
void UMiningEncounterPlayerPolicy::EndPlay(const EEndPlayReason::Type Reason) {Release();Super::EndPlay(Reason);}
AMiningEncounterPlayerController::AMiningEncounterPlayerController()
{
    static ConstructorHelpers::FClassFinder<ABrainMiningPlayerController> Controller(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController"));
    if(Controller.Succeeded())UseInputDefaultsFrom(Controller.Class->GetDefaultObject<ABrainMiningPlayerController>());
    EchoInteraction=CreateDefaultSubobject<UEchoPassageInteractionComponent>(TEXT("EchoInteraction"));
    EchoInteraction->bUseSharedPickaxe=true;
    CardInteraction=CreateDefaultSubobject<UCardRoomInteractionComponent>(TEXT("CardInteraction"));
    EncounterPolicy=CreateDefaultSubobject<UMiningEncounterPlayerPolicy>(TEXT("EncounterPolicy"));
}
void AMiningEncounterPlayerController::BeginPlay()
{
    Super::BeginPlay();SetMiningGUIEnabled(false);EncounterPolicy->Apply(GetPawn());
    EchoInteraction->OnKnock.AddWeakLambda(this,[this]()
    {if(GetPawn()) if(auto* Effects=GetPawn()->FindComponentByClass<UMiningToolEffectsComponent>()) Effects->PlayEncounterSwing();});
}
void AMiningEncounterPlayerController::OnPossess(APawn* InPawn) {Super::OnPossess(InPawn);EncounterPolicy->Apply(InPawn);}
void AMiningEncounterPlayerController::OnUnPossess() {EncounterPolicy->Release();Super::OnUnPossess();}
void AMiningEncounterPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    EchoInteraction->BindInput(InputComponent);CardInteraction->BindInput(InputComponent);
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AMiningEncounterPlayerController::MinePressed).bConsumeInput=false;
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Released,this,&AMiningEncounterPlayerController::MineReleased).bConsumeInput=false;
}
AMiningEncounterGameMode::AMiningEncounterGameMode()
{
    static ConstructorHelpers::FClassFinder<APawn> Pawn(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
    if(Pawn.Succeeded())DefaultPawnClass=Pawn.Class;
    PlayerControllerClass=AMiningEncounterPlayerController::StaticClass();
    HUDClass=AHUD::StaticClass();
}

void AMiningEncounterGameMode::StartPlay()
{
    // Runtime assembly keeps the authored study and unrelated encounter maps intact.
    if(GetWorld()->GetMapName().EndsWith(TEXT("Lvl_EchoMazeToCards_P1")) && !TActorIterator<AMiningRoomEscape>(GetWorld()))
    {
        ACardRoomStage* Cards=nullptr;
        AMiningGiantRoomEntrance* Door=nullptr;
        for(TActorIterator<ACardRoomStage> It(GetWorld());It;++It){Cards=*It;break;}
        for(TActorIterator<AMiningGiantRoomEntrance> It(GetWorld());It;++It){Door=*It;break;}
        if(Cards && Door)
        {
            auto* Escape=GetWorld()->SpawnActorDeferred<AMiningRoomEscape>(AMiningRoomEscape::StaticClass(),FTransform::Identity);
            Escape->CardStage=Cards;Escape->Entrance=Door;
            Escape->FinishSpawning(FTransform::Identity);
        }
    }
    Super::StartPlay();
}

void UMiningEncounterPlayerPolicy::SetMiningAllowed(bool bAllowed)
{
    if(!Mining.IsValid() || bAllowed==!bMiningBlocked)return;
    Mining->CancelMining();
    if(bAllowed)Mining->PopEncounterBlock();else Mining->PushEncounterBlock();
    bMiningBlocked=!bAllowed;
}
void AMiningEncounterPlayerController::MinePressed()
{
    if(GetPawn() && EncounterPolicy->IsMiningAllowed() && !IsMoveInputIgnored())
        if(auto* Mining=GetPawn()->FindComponentByClass<UMiningComponent>())Mining->BeginMiningInput();
}
void AMiningEncounterPlayerController::MineReleased()
{
    if(GetPawn())if(auto* Mining=GetPawn()->FindComponentByClass<UMiningComponent>())Mining->EndMiningInput();
}
