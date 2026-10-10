#include "MiningEncounterPlayer.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "EchoPassagePlayer.h"
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
    if(Mining.IsValid())Mining->PushEncounterBlock();
    if(Movement.IsValid())Movement->PushFlightBlock();
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
    if(Mining.IsValid())Mining->PopEncounterBlock();
    if(Movement.IsValid())Movement->PopFlightBlock();
    for(auto& Light:Lights)if(Light.Key.IsValid())Light.Key->SetVisibility(Light.Value);
    Lights.Reset();Movement.Reset();Mining.Reset();AppliedPawn.Reset();
}
void UMiningEncounterPlayerPolicy::EndPlay(const EEndPlayReason::Type Reason) {Release();Super::EndPlay(Reason);}
AMiningEncounterPlayerController::AMiningEncounterPlayerController()
{
    static ConstructorHelpers::FClassFinder<ABrainMiningPlayerController> Controller(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController"));
    if(Controller.Succeeded())UseInputDefaultsFrom(Controller.Class->GetDefaultObject<ABrainMiningPlayerController>());
    EchoInteraction=CreateDefaultSubobject<UEchoPassageInteractionComponent>(TEXT("EchoInteraction"));
    CardInteraction=CreateDefaultSubobject<UCardRoomInteractionComponent>(TEXT("CardInteraction"));
    EncounterPolicy=CreateDefaultSubobject<UMiningEncounterPlayerPolicy>(TEXT("EncounterPolicy"));
}
void AMiningEncounterPlayerController::BeginPlay() {Super::BeginPlay();EncounterPolicy->Apply(GetPawn());}
void AMiningEncounterPlayerController::OnPossess(APawn* InPawn) {Super::OnPossess(InPawn);EncounterPolicy->Apply(InPawn);}
void AMiningEncounterPlayerController::OnUnPossess() {EncounterPolicy->Release();Super::OnUnPossess();}
void AMiningEncounterPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    EchoInteraction->BindInput(InputComponent);CardInteraction->BindInput(InputComponent);
}
AMiningEncounterGameMode::AMiningEncounterGameMode()
{
    static ConstructorHelpers::FClassFinder<APawn> Pawn(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
    if(Pawn.Succeeded())DefaultPawnClass=Pawn.Class;
    PlayerControllerClass=AMiningEncounterPlayerController::StaticClass();
    HUDClass=AHUD::StaticClass();
}
