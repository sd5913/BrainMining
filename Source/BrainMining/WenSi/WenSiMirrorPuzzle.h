#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "Blueprint/UserWidget.h"
#include "WenSiMirrorPuzzle.generated.h"

class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class AWenSiMirrorPuzzle;
class SHorizontalBox;
class STextBlock;

UENUM(BlueprintType)
enum class EWenSiMirrorWord : uint8 { Ordinary, Heavy, Floating, Passable, Conductive, Big, Small };

UCLASS()
class BRAINMINING_API AWenSiMirrorExhibit : public AActor
{
 GENERATED_BODY()
public:
 AWenSiMirrorExhibit();
 virtual void BeginPlay() override;
 virtual void Tick(float Dt) override;
 UPROPERTY(EditAnywhere, Category="WenSi") int32 Kind=0;
 UPROPERTY(EditAnywhere, Category="WenSi") EWenSiMirrorWord Word=EWenSiMirrorWord::Ordinary;
 UPROPERTY(EditAnywhere, Category="WenSi") FString DisplayNoun;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
 UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
 UPROPERTY() TObjectPtr<UTextureRenderTarget2D> MirrorTarget;
 void Configure();
 void Apply();
 void Highlight(bool bAimed);
 FString Noun() const;
 FString Adjective() const;
 FVector Rest;
 bool bBallLanded=false;
 bool bThinkingEnabled=true;
private:
 float MirrorClock=0;
 void UpdateMirror();
};

UCLASS()
class BRAINMINING_API AWenSiMirrorPawn : public ACharacter
{
 GENERATED_BODY()
public:
 AWenSiMirrorPawn();
 virtual void Tick(float Dt) override;
 bool SetWord(EWenSiMirrorWord NewWord);
 EWenSiMirrorWord Word=EWenSiMirrorWord::Ordinary;
 bool bRequireFlightRelease=false;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> MinerParts;
};

UCLASS()
class BRAINMINING_API AWenSiMirrorGameMode : public AGameModeBase
{
 GENERATED_BODY()
public: AWenSiMirrorGameMode();
};

UCLASS()
class BRAINMINING_API UWenSiMirrorWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 TWeakObjectPtr<AWenSiMirrorPuzzle> Puzzle;
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual FReply NativeOnKeyDown(const FGeometry&,const FKeyEvent&) override;
 void Refresh();
private:
 TSharedPtr<SHorizontalBox> Rows[2];
 TSharedPtr<SWidget> Cards[2];
 TSharedPtr<STextBlock> Hint;
 FString Signature;
};

UCLASS()
class BRAINMINING_API AWenSiMirrorPuzzle : public AActor
{
 GENERATED_BODY()
public:
 AWenSiMirrorPuzzle();
 virtual void BeginPlay() override;
 virtual void Tick(float Dt) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UPROPERTY() TArray<TObjectPtr<AWenSiMirrorExhibit>> Exhibits;
 UPROPERTY() TObjectPtr<UWenSiMirrorWidget> UI;
 TWeakObjectPtr<AWenSiMirrorExhibit> Thoughts[2];
 TWeakObjectPtr<AWenSiMirrorExhibit> Aim;
 bool bEditing=false;
 bool bAimReset=false;
 bool bPressure=false;
 bool bPower=false;
 bool bRewardUnlocked=false;
 bool bComplete=false;
 bool bSecondRoom=false;
 void Think();
 void CloseThought(int32 Index);
 void ToggleEdit();
 void SwapWords(int32 From,int32 To);
 void RefreshSelfThoughts();
 AWenSiMirrorExhibit* Find(int32 Kind) const;
 UFUNCTION(BlueprintCallable, Category="WenSi") void ResetPuzzle();
 UFUNCTION(BlueprintCallable, Category="WenSi|Validation") FString ValidateMirrorPuzzle();
 UFUNCTION(BlueprintCallable, Category="WenSi|Validation") FString MirrorStatus();
#if WITH_EDITOR
 static void CreateMirrorLevel();
#endif
private:
 TArray<FTransform> InitialTransforms;
 TArray<EWenSiMirrorWord> InitialWords;
 FTransform StartTransform;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LampMeshes;
 UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Lamps;
 UPROPERTY() TObjectPtr<AActor> RewardShutter;
 UPROPERTY() TObjectPtr<AActor> Reward;
 void UpdateMechanism();
};
