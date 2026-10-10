#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PopupCloseGameMode.generated.h"

UCLASS()
class BRAINMINING_API APopupCloseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APopupCloseGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
};
