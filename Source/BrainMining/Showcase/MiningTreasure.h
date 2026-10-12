#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningTreasure.generated.h"

class ADiggableRock;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class APlayerController;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UInstancedStaticMeshComponent;

USTRUCT(BlueprintType)
struct FMiningTreasureDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UStaticMesh> Model;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Scale = FVector(.7);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Color = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor HintColor = FLinearColor(.2f,.8f,.5f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bBuriedHint = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="40", ClampMax="240")) float HintRadius = 120;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="3")) float HintStrength = .8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="4")) float EmissiveStrength = .45f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float Roughness = .4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float Metallic = .1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SurfaceStyle = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 MinValue = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 MaxValue = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> ValueTiers;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bScaleWithValue = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float Weight = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> Regions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bNormalLayer = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bGuideLayer = true;
    bool IsEligible(FName Region, bool bGuide) const;
};

USTRUCT(BlueprintType)
struct FMiningTreasurePlacement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TreasureId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Value = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AreaTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RouteProgress = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bGuideLayer = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform Transform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform Surface;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<ADiggableRock>> Hosts;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTransform> Fragments;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector> FragmentSupports;
};

UCLASS()
class BRAINMINING_API AMiningTreasure : public AActor
{
    GENERATED_BODY()
public:
    AMiningTreasure();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Model;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> GlowLight;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> HintMaterials;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName TreasureId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FText TreasureName;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Value = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Exposure = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bCollected = false;
    UPROPERTY() TArray<TObjectPtr<ADiggableRock>> CoveringRocks;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> CrystalClues;
    UPROPERTY() TArray<FVector> CrystalSupports;
    TArray<FTransform> CrystalTransforms;
    void ConfigureCrystalClues(const FMiningTreasurePlacement& Placement);
    static void MarkCoveredPoints(const TArray<TObjectPtr<ADiggableRock>>& Rocks, const TArray<FVector>& Points, TArray<bool>& Covered);
    static float ValueScale(const FMiningTreasureDefinition& Definition, int32 InValue);
    bool Configure(const FMiningTreasureDefinition& Definition, int32 InValue, UMaterialInterface* Material);
    float MeasureExposure() const;
    static bool IsExposureEligible(float Ratio) { return FMath::IsFinite(Ratio) && Ratio > .5f; }
    void SetHighlighted(bool bEnabled);
    void AttachBuriedHint(const FMiningTreasureDefinition& Definition);
    void UpdateGlow(float DeltaSeconds, bool bAllowLight);
    float GetHintAmount() const { return HintAmount; }
    int32 GetSurfaceSampleCount() const { return SurfaceSamples.Num(); }
private:
    TArray<FVector> SurfaceSamples;
    FLinearColor BaseColor = FLinearColor::White;
    float HintAmount = 1;
};

UCLASS(Blueprintable)
class BRAINMINING_API AMiningTreasureField : public AActor
{
    GENERATED_BODY()
public:
    AMiningTreasureField();
    UPROPERTY(EditAnywhere, Category="Treasure") bool bUseSharedWallet = false;
    UPROPERTY(EditAnywhere, Category="Treasure|Campaign") bool bUseAuthoredPlacements = false;
    UPROPERTY(EditAnywhere, Category="Treasure|Campaign") TArray<FMiningTreasurePlacement> AuthoredPlacements;
    FName ActiveAreaTag;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Treasure") TArray<FMiningTreasureDefinition> Definitions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Treasure") FName RegionId = TEXT("TreasureShowcase");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Treasure") int32 RandomSeed = 2718;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Treasure", meta=(ClampMin="1", ClampMax="9")) int32 SlotsPerLayer = 9;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Treasure") TObjectPtr<UMaterialInterface> TreasureMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Treasure") TObjectPtr<UMaterialInterface> RockMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Treasure") TObjectPtr<UMaterialInterface> GuideMaterial;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Treasure") int64 Coins = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Treasure") int32 CollectedCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Treasure") TArray<TObjectPtr<AMiningTreasure>> Items;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Treasure") TArray<TObjectPtr<ADiggableRock>> Rocks;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Treasure") FString GenerationStatus;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Treasure") TObjectPtr<AMiningTreasure> Target;
    bool IsAreaActive() const { return bAreaActive; }
    void SetAreaActive(bool bActive);
    UFUNCTION(BlueprintCallable, Category="Treasure") void ResetExhibit();
    bool CollectTarget(APlayerController* Controller);
    FString GetPrompt() const;
    static int32 ChooseDefinition(const TArray<FMiningTreasureDefinition>& Catalogue, FName Region, bool bGuide, FRandomStream& Random);
private:
    bool bAreaActive = false;
    float ToastRemaining = 0;
    FString Toast;
    TArray<int32> ObservedCuts;
    bool bAuthoredGenerated = false;
    void SpawnAuthored();
    void RefreshTarget(APlayerController* Controller);
    void ClearGenerated();
};
