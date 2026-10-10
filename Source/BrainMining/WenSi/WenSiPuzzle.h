#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "Blueprint/UserWidget.h"
#include "WenSiPuzzle.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class SHorizontalBox;
class STextBlock;
class AWenSiPuzzle;

UENUM()
enum class EWenSiRule : uint8 { Heavy, Light, Intangible };

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
 void Configure(int32 Kind);
 void ApplyRule();
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
 void ToggleEdit();
 void CloseThought(int32 Index);
 void SwapRules(int32 From, int32 To);
 void Think();
 UFUNCTION(BlueprintCallable, Category="WenSi|Validation") FString ValidatePuzzle();
 UFUNCTION(BlueprintCallable, Category="WenSi|Validation") FString ValidateExtendedPuzzle();
#if WITH_EDITOR
 static void CreateLevel();
 static void CreateExtendedLevel();
#endif
};
