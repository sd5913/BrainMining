#include "MiningPrototypeGameMode.h"
#include "MiningHUD.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

AMiningPrototypeGameMode::AMiningPrototypeGameMode()
{
	static ConstructorHelpers::FClassFinder<APawn> PawnClass(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	static ConstructorHelpers::FClassFinder<APlayerController> ControllerClass(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController"));
	if (PawnClass.Succeeded())
	{
		DefaultPawnClass = PawnClass.Class;
	}
	if (ControllerClass.Succeeded())
	{
		PlayerControllerClass = ControllerClass.Class;
	}
	HUDClass = AMiningHUD::StaticClass();
}
