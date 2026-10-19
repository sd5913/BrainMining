#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "Blueprint/UserWidget.h"
#include "WenSiMirrorPuzzle.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
class AWenSiMirrorPuzzle;
class SHorizontalBox;
class STextBlock;
class APlayerController;

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
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UPROPERTY(EditAnywhere, Category="WenSi") int32 Kind=0;
 UPROPERTY(EditAnywhere, Category="WenSi") EWenSiMirrorWord Word=EWenSiMirrorWord::Ordinary;
 UPROPERTY(EditAnywhere, Category="WenSi") FString DisplayNoun;
 UPROPERTY(EditAnywhere, Category="WenSi|Art") TObjectPtr<UStaticMesh> ArtModel;
 UPROPERTY(EditAnywhere, Category="WenSi|Art") FRotator ArtRotation=FRotator(0,90,0);
 UPROPERTY(EditAnywhere, Category="WenSi|Art") bool bUniformArtScale=true;
 UPROPERTY(VisibleAnywhere, Category="WenSi|Art") TObjectPtr<UStaticMeshComponent> ArtVisual;
 UPROPERTY(VisibleAnywhere, Category="WenSi|VFX") TObjectPtr<UNiagaraComponent> ConductiveFX;
 UFUNCTION(BlueprintCallable,CallInEditor,Category="WenSi|Art") void RefreshArtModel();
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
 UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
 UPROPERTY() TObjectPtr<UTextureRenderTarget2D> MirrorTarget;
 UPROPERTY(Transient) TObjectPtr<AActor> MirrorPresence;
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> PresenceParts;
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> ConductiveArcs;
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
 float ConductiveClock=0;
 float RestBoundsMinZ=0.f;
 bool bRestBoundsInitialized=false;
 void UpdateMirror();
 void UpdateConductive(float Dt);
 void BuildMirrorPresence();
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
 virtual void NativeTick(const FGeometry& MyGeometry,float InDeltaTime) override;
 void Refresh();
private:
 UPROPERTY(Transient) TObjectPtr<class UTexture2D> ThoughtTexture;
 FSlateBrush ThoughtBrushes[2];
 TSharedPtr<SHorizontalBox> Rows[2];
 TSharedPtr<SWidget> Cards[2];
 TSharedPtr<SWidget> WordWidgets[2];
 TWeakObjectPtr<UObject> PresentedThoughts[2];
 float FlightAge[2]={-1.f,-1.f};
 TSharedPtr<STextBlock> Hint;
 FString Signature;
};

UCLASS()
class BRAINMINING_API AWenSiMirrorPuzzle : public AActor
{
 GENERATED_BODY()
public:
 AWenSiMirrorPuzzle();
 UPROPERTY(EditAnywhere, Category="WenSi|Campaign") bool bAreaManaged=false;
 bool bAreaActive=true;
 FTransform AreaPlayerStart;
 void SetAreaActive(bool Active);
	bool HandleInteractionInput(APlayerController* PC);
 EWenSiMirrorWord GetPlayerWord() const;
 bool SetPlayerWord(EWenSiMirrorWord Word);
 FVector RoomPoint(FVector Local) const { return Local+GetActorLocation(); }

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
 UPROPERTY(Transient) TObjectPtr<class AMouseMuseumPortal> MousePortal;
 bool bComplete=false;
 bool bSecondRoom=false;
 void Think();
 void CloseThought(int32 Index);
 void CloseThoughts();
 void SwapThoughts();
 void AdvanceSwap(float DeltaTime);
 bool IsSwapping() const { return SwapAge>=0.f; }
 float GetSwapProgress() const { return FMath::Clamp(SwapAge/SwapTravelSeconds,0.f,1.f); }
 float GetSwapOpacity() const { return 1.f-FMath::SmoothStep(SwapTravelSeconds+SwapHoldSeconds,SwapTotalSeconds,SwapAge); }
 bool HasSwapCommitted() const { return bSwapCommitted; }
 const FString& GetSwapWord(int32 Index) const { return SwapWordsBefore[Index]; }
 static constexpr float SwapTravelSeconds=.7f;
 static constexpr float SwapHoldSeconds=.3f;
 static constexpr float SwapTotalSeconds=1.2f;
 void ToggleEdit();
 bool SwapWords(int32 From,int32 To);
 void RefreshSelfThoughts();
 AWenSiMirrorExhibit* Find(int32 Kind) const;
 UFUNCTION(BlueprintCallable, Category="WenSi") void ResetPuzzle();
 UFUNCTION(BlueprintCallable, Category="WenSi|Developer") bool SkipToReward();
 UFUNCTION(BlueprintCallable, Category="WenSi|Validation") FString ValidateMirrorPuzzle();
 UFUNCTION(BlueprintCallable, Category="WenSi|Validation") FString MirrorStatus();
#if WITH_EDITOR
 static void CreateMirrorLevel();
#endif
private:
 TArray<FTransform> InitialTransforms;
 TArray<EWenSiMirrorWord> InitialWords;
 FTransform StartTransform;
 FVector PlayerBaseScale=FVector::OneVector;
 float PlayerBaseSpeed=300;
 float PlayerBaseRadius=32,PlayerBaseHalfHeight=88,PlayerBaseStepHeight=12;
 ECollisionResponse PlayerBaseCollision=ECR_Block;
 EWenSiMirrorWord SharedPlayerWord=EWenSiMirrorWord::Ordinary;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LampMeshes;
 UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Lamps;
 UPROPERTY() TObjectPtr<AActor> RewardShutter;
 UPROPERTY() TObjectPtr<AActor> Reward;
 UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> CableMaterials;
 TArray<bool> CablePressure;
 void UpdateMechanism();
 void CancelSwap();
 float SwapAge=-1.f;
 bool bSwapCommitted=false;
 TWeakObjectPtr<AWenSiMirrorExhibit> SwapTargets[2];
 EWenSiMirrorWord SwapInitialWords[2]={EWenSiMirrorWord::Ordinary,EWenSiMirrorWord::Ordinary};
 FString SwapWordsBefore[2];
};
