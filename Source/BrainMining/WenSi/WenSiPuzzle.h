#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "Blueprint/UserWidget.h"
#include "WenSiPuzzle.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class SHorizontalBox;
class STextBlock;
class AWenSiPuzzle;
class AWenSiCircuitMechanism;
class AWenSiResetButton;

UENUM()
enum class EWenSiRule : uint8 { Heavy, Light, Intangible, Conductive };

UCLASS()
class BRAINMINING_API AWenSiObject : public AActor
{
 GENERATED_BODY()
public:
 AWenSiObject();
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 UPROPERTY(EditAnywhere, Category="WenSi") int32 ObjectKind = 0;
 UPROPERTY(EditAnywhere, Category="WenSi") EWenSiRule Rule = EWenSiRule::Heavy;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
 UPROPERTY(EditAnywhere, Category="WenSi|Museum") TObjectPtr<UMaterialInterface> HighlightMaterial;
 void Configure(int32 Kind);
 void ApplyRule();
 void SetInteractionHighlight(bool bAimed, bool bEnabled=true);
 FString Noun() const;
 FString Adjective() const;
 FVector Rest;
private:
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
 UPROPERTY() TObjectPtr<UMaterialInterface> SolidMaterial;
 UPROPERTY() TObjectPtr<UMaterialInterface> GhostMaterial;
};

UCLASS()
class BRAINMINING_API AWenSiPawn : public ACharacter
{
 GENERATED_BODY()
public:
 AWenSiPawn();
 virtual void Tick(float DeltaSeconds) override;
 bool bFlightNeedsRelease = false;
};

UCLASS()
class BRAINMINING_API AWenSiGameMode : public AGameModeBase
{
 GENERATED_BODY()
public: AWenSiGameMode();
};

UCLASS()
class BRAINMINING_API UWenSiPuzzleWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 TWeakObjectPtr<AWenSiPuzzle> Puzzle;
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
 void Refresh();
private:
 UPROPERTY(Transient) TObjectPtr<class UTexture2D> ThoughtTexture;
 FSlateBrush ThoughtBrushes[2];
 TSharedPtr<SHorizontalBox> Rows[2];
 TSharedPtr<SWidget> Cards[2];
 TSharedPtr<STextBlock> Hint;
 FString Signature;
};

UCLASS()
class BRAINMINING_API AWenSiPuzzle : public AActor
{
 GENERATED_BODY()
public:
 AWenSiPuzzle();
 UPROPERTY(EditAnywhere, Category="WenSi") bool bAreaManaged = false;
 bool bAreaActive = true;
 void SetAreaActive(bool bActive);
 UPROPERTY(EditAnywhere, Category="Integration") FTransform AreaPlayerStart;
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UPROPERTY() TArray<TObjectPtr<AWenSiObject>> Objects;
 UPROPERTY() TObjectPtr<UWenSiPuzzleWidget> UI;
 TWeakObjectPtr<AWenSiObject> Thoughts[2];
 bool bEditing = false;
 bool bComplete = false;
 int32 Aim = INDEX_NONE;
 UPROPERTY(EditAnywhere, Category="WenSi") bool bExtendedPuzzle = false;
 UPROPERTY(EditAnywhere, Category="WenSi") bool bCircuitPuzzle = false;
 UPROPERTY(VisibleAnywhere, Category="WenSi") bool bStageOneSolved = false;
 UPROPERTY(VisibleAnywhere, Category="WenSi") bool bStageTwoUnlocked = false;
 UPROPERTY() TObjectPtr<AWenSiCircuitMechanism> Circuit;
 UPROPERTY() TObjectPtr<AWenSiResetButton> ResetButton;
 bool bAimReset = false;
 bool CanThinkAbout(const AWenSiObject* Object) const;
 bool FirstBarrierClear() const;
 void UpdateCircuitProgress();
 UFUNCTION(BlueprintCallable, Category="WenSi") void ResetPuzzle();
 void ToggleEdit();
 void CloseThought(int32 Index);
 void SwapRules(int32 From, int32 To);
 void Think();
 UFUNCTION(BlueprintCallable, Category="WenSi|Validation") FString ValidatePuzzle();
 UFUNCTION(BlueprintCallable, Category="WenSi|Validation") FString ValidateExtendedPuzzle();
 UFUNCTION(BlueprintCallable, Category="WenSi|Validation") FString ValidateCircuitPuzzle();
#if WITH_EDITOR
 static void CreateLevel();
 static void CreateExtendedLevel();
 static void DressMuseum();
 static void UpgradeMuseumCircuit();
#endif
private:
 TArray<FTransform> InitialObjectTransforms;
 TArray<EWenSiRule> InitialObjectRules;
 FTransform PlayerResetTransform;
};

UCLASS()
class BRAINMINING_API AWenSiCircuitMechanism : public AActor
{
 GENERATED_BODY()
public:
 AWenSiCircuitMechanism();
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 void Configure();
 void SetState(bool bPressure, bool bElectricity, bool bOpen);
 void ResetMechanism();
 bool HasPressure() const;
 bool HasConductiveBridge() const;
 UPROPERTY(VisibleAnywhere, Category="WenSi") bool bPressureActive = false;
 UPROPERTY(VisibleAnywhere, Category="WenSi") bool bCircuitActive = false;
 UPROPERTY(VisibleAnywhere, Category="WenSi") bool bRewardOpen = false;
private:
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
 UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> Palette;
 UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
 UPROPERTY() TObjectPtr<UStaticMesh> SphereMesh;
 UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
 UPROPERTY() TObjectPtr<UMaterialInterface> GlowMaterial;
 UPROPERTY() TObjectPtr<UMaterialInterface> GlassMaterial;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Lamps[2];
 float DoorSlide = 0.f;
};

UCLASS()
class BRAINMINING_API AWenSiResetButton : public AActor
{
 GENERATED_BODY()
public:
 AWenSiResetButton();
 virtual void BeginPlay() override;
 void Configure();
 void SetAimed(bool bAimed);
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Button;
private:
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
 UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
 UPROPERTY() TObjectPtr<UMaterialInterface> GlowMaterial;
};
