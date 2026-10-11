#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningPrototypeGameMode.h"
#include "MiningCellUniverse.generated.h"

class ADiggableRock;
class UMiningComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class SCellUniverseView;

USTRUCT(BlueprintType)
struct FCellUniverseStage
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="2600", Units="cm")) float Length=4000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1200", Units="cm")) float Width=2600;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1000", Units="cm")) float Height=1500;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="800", Units="cm")) float BelowPathDepth=1500;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="300", Units="cm")) float OpeningWidth=420;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="300", Units="cm")) float OpeningHeight=400;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="180", ClampMax="650", Units="cm")) float Depth=260;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", ClampMax="1")) float EndScale=.65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="5", Units="s")) float MinimumScaleSeconds=18;
};

/** A standalone ending study. Uses the existing pawn, input, boolean mining and flight. */
UCLASS()
class BRAINMINING_API AMiningCellUniverse : public AActor
{
    GENERATED_BODY()
public:
    AMiningCellUniverse();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    // Authored settings are copied at build time: editing details cannot desync saved collisions.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cell Universe")
    TArray<FCellUniverseStage> Stages;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cell Universe")
    TArray<FCellUniverseStage> BuiltStages;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cell Universe")
    TArray<float> StageStarts;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cell Universe")
    int32 Seed = 73;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cell Universe")
    TArray<TObjectPtr<ADiggableRock>> Gates;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Cell Universe")
    int32 CurrentStage = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Cell Universe")
    bool bComplete = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Cell Universe")
    int32 MembraneCuts = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Cell Universe")
    float ScaleProgress = 0;

    UFUNCTION(CallInEditor, BlueprintCallable, Category="Cell Universe") void BuildDemo();
    UFUNCTION(CallInEditor, BlueprintCallable, Category="Cell Universe") void ApplyExpandedPreset();
    UFUNCTION(BlueprintCallable, Category="Cell Universe") void ResetPresentation();
    UFUNCTION(BlueprintPure, Category="Cell Universe") FVector GetGateApproach(int32 Index) const;
    UFUNCTION(BlueprintPure, Category="Cell Universe") float GetStageStart(int32 Index) const;
    UFUNCTION(BlueprintPure, Category="Cell Universe") float GetStageEnd(int32 Index) const;
    static constexpr int32 StageCount=11;
    static constexpr int32 MembraneStage=8;
    static constexpr int32 GiantBloodStage=9;
    static constexpr int32 MouthStage=10;
    FString StageName() const;
    FString Hint() const;
    FString Subtitle() const;
    UMiningComponent* GetMining() const { return Mining; }

private:
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> Landmarks;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BloodCells;
    UPROPERTY() TArray<FVector> BloodOrigins;
    UPROPERTY() TArray<FVector> BloodScales;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> GateMaterials;
    UPROPERTY(Transient) TObjectPtr<UMiningComponent> Mining;
    UPROPERTY(Transient) TObjectPtr<APlayerController> Player;
    TSharedPtr<SCellUniverseView> View;
    TArray<int32> ObservedCuts;
    float Elapsed = 0;
    float StageAge = 0;
    TArray<float> StageElapsed;
    bool bInitialized = false;
    UStaticMeshComponent* Shape(FString Name, FVector Position, FVector Size, FLinearColor Color,
        const TCHAR* Mesh = TEXT("Cube"), bool Collision = false, USceneComponent* Parent = nullptr,
        const TCHAR* Surface = TEXT("M_CU_Surface"));
    USceneComponent* Group(FString Name, FVector Position);
    void Rod(FString Name, FVector A, FVector B, float Width, FLinearColor Color, USceneComponent* Parent = nullptr);
    void Ring(FString Name, FVector Center, float Radius, FLinearColor Color, USceneComponent* Parent = nullptr);
    void BuildCell(int32 Stage);
    void BuildBlood(int32 Stage);
    void BuildTissue();
    void BuildBug();
    void BuildContinent();
    void BuildPlanet();
    void BuildCosmos();
    void BuildMouth();
    void BuildBelowPath();
    void Galaxy(USceneComponent* Parent, float Radius, int32 RandomSeed, int32 Count);
    bool InitializePlayer();
};

UCLASS()
class BRAINMINING_API AMiningCellUniverseGameMode : public AMiningPrototypeGameMode
{
    GENERATED_BODY()
public:
    AMiningCellUniverseGameMode();
};
