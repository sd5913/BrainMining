#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MiningComponent.generated.h"

class ADiggableRock;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMiningFieldReset);

UCLASS(ClassGroup=(Mining), meta=(BlueprintSpawnableComponent))
class BRAINMINING_API UMiningComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMiningComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void BindInput(UEnhancedInputComponent* Input);
    void SetInputContextEnabled(bool bEnabled);
    bool IsInputContextEnabled() const {return bInputContextEnabled;}
    void BeginMiningInput() {StartMining();}
    void PushEncounterBlock();
    void PopEncounterBlock();
    bool IsEncounterBlocked() const {return EncounterBlockCount>0;}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining", meta=(ClampMin="50.0", ClampMax="1000.0"))
	float Reach = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining", meta=(ClampMin="25.0", ClampMax="300.0"))
	float Radius = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining", meta=(ClampMin="0.1", ClampMax="2.0"))
	float Interval = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining", meta=(ClampMin="0.0", ClampMax="0.9"))
	float InsetFraction = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining", meta=(ClampMin="0.0", ClampMax="150.0"))
	float DownwardOffset = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Safety", meta=(ClampMin="0.0", ToolTip="Reset after falling this far below spawn. Zero disables this safety reset."))
	float FallResetDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining")
	bool bShowPreview = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	FString Status = TEXT("Aim at the rock");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<ADiggableRock> TargetRock;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<ADiggableRock> LastMinedRock;

	UFUNCTION(BlueprintCallable, Category="Mining")
	bool TryMine();

	// Used by modal interfaces to prevent a held dig from resuming after closing.
	UFUNCTION(BlueprintCallable, Category="Mining")
	void CancelMining() { StopMining(); }

	UFUNCTION(BlueprintCallable, Category="Mining")
	void ResetField();

	// Optional level presentation can reset alongside the existing R/fall recovery.
	UPROPERTY(BlueprintAssignable, Category="Mining")
	FMiningFieldReset OnFieldReset;

private:
    int32 EncounterBlockCount=0;
    bool bInputContextEnabled=true;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MiningContext;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MineAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ResetAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PreviewAction;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
	FTransform InitialTransform;
	FHitResult TargetHit;
	double NextMineTime = 0.0;
	bool bMiningHeld = false;
	bool bHasInitialTransform = false;
	void RefreshTarget();
	FVector GetCutCenter() const;
	void StartMining();
	void StopMining();
	void TogglePreview();
};
