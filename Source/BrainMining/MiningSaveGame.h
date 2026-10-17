#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "MiningMineralInventory.h"
#include "MiningSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FMiningSavedActorState
{
	GENERATED_BODY()
	UPROPERTY() FString ActorPath;
	UPROPERTY() FName Id;
	UPROPERTY() int32 IntValue = 0;
	UPROPERTY() float FloatValue = 0.f;
	UPROPERTY() uint8 Flags = 0;
};

USTRUCT(BlueprintType)
struct FMiningSavedCut
{
	GENERATED_BODY()
	UPROPERTY() FString ActorPath;
	// 0=sphere, 1=pickaxe, 2=blast.
	UPROPERTY() uint8 Operation = 0;
	UPROPERTY() FVector Center = FVector::ZeroVector;
	UPROPERTY() float Radius = 0.f;
	UPROPERTY() FTransform WorldFrame;
	UPROPERTY() FVector HalfSize = FVector::ZeroVector;
	UPROPERTY() int32 Seed = 0;
	UPROPERTY() float Roughness = 1.f;
};

UCLASS()
class BRAINMINING_API UMiningSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() int32 SaveVersion = 2;
	UPROPERTY() FString MapPackage;
	UPROPERTY() bool bHasPlayerTransform = false;
	UPROPERTY() FTransform PlayerTransform;
	UPROPERTY() FRotator ControlRotation;
	UPROPERTY() TMap<EMiningMineral, int32> Minerals;
	UPROPERTY() int64 Balance = 0;
	UPROPERTY() TArray<int32> UpgradeLevels;
	UPROPERTY() int32 BombCount = 0;
	UPROPERTY() bool bBombSkillUnlocked = false;
	UPROPERTY() bool bBombSkillRequiresUnlock = false;
	UPROPERTY() TArray<int32> CollectionInventory;
	UPROPERTY() TArray<FMiningSavedActorState> ActorStates;
	UPROPERTY() TArray<FMiningSavedCut> Cuts;
};
