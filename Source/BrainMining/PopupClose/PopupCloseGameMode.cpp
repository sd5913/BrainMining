#include "PopupClose/PopupCloseGameMode.h"

#include "PopupClose/PopupCloseStage.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "UObject/ConstructorHelpers.h"

APopupCloseGameMode::APopupCloseGameMode()
{
	static ConstructorHelpers::FClassFinder<APawn> PawnClass(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	if (PawnClass.Succeeded())
	{
		DefaultPawnClass = PawnClass.Class;
	}

	static ConstructorHelpers::FClassFinder<APlayerController> ControllerClass(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
	if (ControllerClass.Succeeded())
	{
		PlayerControllerClass = ControllerClass.Class;
	}
}

void APopupCloseGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	UWorld* World = GetWorld();
	bool bHasPlayerStart = false;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		bHasPlayerStart = true;
		break;
	}

	if (!bHasPlayerStart)
	{
		for (TActorIterator<APopupCloseStage> It(World); It; ++It)
		{
			const FTransform Spawn = It->GetPlayerSpawnTransform();
			World->SpawnActor<APlayerStart>(Spawn.GetLocation(), Spawn.Rotator());
			break;
		}
	}

	Super::InitGame(MapName, Options, ErrorMessage);
}
