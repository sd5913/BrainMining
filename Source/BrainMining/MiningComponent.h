#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "Components/ActorComponent.h"
#include "MiningComponent.generated.h"

class ADiggableRock;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMiningAttemptEvent, float, MiningInterval);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMiningHitEvent, const FHitResult&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMiningResetEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMiningToolModeEvent, bool, bDrillMode);

UCLASS(ClassGroup=(Mining), meta=(BlueprintSpawnableComponent))
class BRAINMINING_API UMiningComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMiningComponent();
	UPROPERTY(BlueprintAssignable, Category="Mining|Effects")
	FMiningAttemptEvent OnMiningAttempt;
	UPROPERTY(BlueprintAssignable, Category="Mining|Effects")
	FMiningHitEvent OnMiningHit;
	UPROPERTY(BlueprintAssignable, Category="Mining|Effects")
	FMiningResetEvent OnMiningReset;
	UPROPERTY(BlueprintAssignable, Category="Mining|Tool")
	FMiningToolModeEvent OnToolModeChanged;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void BindInput(UEnhancedInputComponent* Input);
    void SetInputContextEnabled(bool bEnabled);
    bool IsInputContextEnabled() const { return bInputContextEnabled; }
    void BeginMiningInput() { StartMining(); }
    UPROPERTY(BlueprintAssignable, Category="Mining") FMiningResetEvent OnFieldReset;
    void PushEncounterBlock();
    void PopEncounterBlock();
    bool IsEncounterBlocked() const {return EncounterBlockCount>0;}
	/** Suspend held mouse mining while the upgrade UI owns the pointer. */
	void SetInputSuspended(bool bSuspended);
    bool IsInputSuspended() const { return bInputSuspended; }
	/** V starts a timed drill session. Active sessions and cooldown cannot be restarted. */
	UFUNCTION(BlueprintCallable, Category="Mining|Drill")
	bool ActivateDrill();
	/** Stow an active drill without clearing its existing use/cooldown deadlines. */
	UFUNCTION(BlueprintCallable, Category="Mining|Tool") void SelectPickaxe();
	UFUNCTION(BlueprintPure, Category="Mining|Drill")
	float GetDrillUseRemaining() const;
	UFUNCTION(BlueprintPure, Category="Mining|Drill")
	float GetDrillCooldownRemaining() const;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill", meta=(ClampMin="0.1"))
	float DrillUseDuration = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill", meta=(ClampMin="0.1"))
	float DrillCooldownDuration = 10.f;
	UFUNCTION(BlueprintPure, Category="Mining|Tool")
	float GetMiningInterval() const;
	UFUNCTION(BlueprintPure, Category="Mining|Tool")
	float GetMiningRadius() const;
	UFUNCTION(BlueprintPure, Category="Mining|Tool")
	bool IsMiningActive() const { return bMiningHeld && !bInputSuspended; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Tool")
	bool bDrillMode = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill", meta=(ClampMin="0.1", ClampMax="1"))
	float DrillIntervalMultiplier = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill", meta=(ClampMin="1", ClampMax="2"))
	float DrillRadiusMultiplier = 1.2f;

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
    bool bScopedReset = false;
    FBox ResetBounds = FBox(ForceInit);
    FName ResetAreaTag;

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

private:
    int32 EncounterBlockCount=0;
    bool bInputContextEnabled = true;
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
	bool bInputSuspended = false;
	bool bHasInitialTransform = false;
	double DrillUseEnds = 0;
	double DrillCooldownEnds = 0;
	void SetDrillMode(bool bNewDrillMode);
	void UpdateDrillSession();
	void RefreshTarget();
	FVector GetCutCenter() const;
	void StartMining();
	void StopMining();
	void TogglePreview();
};
