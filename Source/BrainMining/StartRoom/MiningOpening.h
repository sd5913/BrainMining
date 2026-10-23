#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "MiningOpening.generated.h"

class AMiningOpening;
class UTexture2D;
class UWidgetComponent;
class ACameraActor;
class UButton;
class UImage;

UCLASS()
class BRAINMINING_API UMiningOpeningTitle : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UTexture2D> Texture;
    bool bCornerLayout=false;
    virtual TSharedRef<SWidget> RebuildWidget() override;
};

UCLASS()
class BRAINMINING_API UMiningOpeningMenu : public UUserWidget
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<AMiningOpening> Opening;
    virtual TSharedRef<SWidget> RebuildWidget() override;
    UFUNCTION() void StartGame();
    UFUNCTION() void QuitGame();
    UFUNCTION() void ContinueGame();
    UPROPERTY(Transient) TObjectPtr<UButton> ContinueButton;
    UPROPERTY(Transient) TObjectPtr<UButton> StartButton;
    UPROPERTY(Transient) TObjectPtr<UButton> ExitButton;
    UPROPERTY(Transient) TObjectPtr<UImage> TitleImage;
protected:
    virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
    float TitleFade=1.f;
    float StartScale=1.f;
    float ContinueScale=1.f;
    float ExitScale=1.f;
};

/** MainGame opening; supports authored anchors or the shared StartRoom transform. */
UCLASS()
class BRAINMINING_API AMiningOpening : public AActor
{
    GENERATED_BODY()
public:
    AMiningOpening();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(BlueprintCallable) void StartGame();
    UFUNCTION(BlueprintCallable) void ConfirmNewGame(bool bKeepCollection);
    bool IsChoosingCollection() const { return CollectionChoice.IsValid(); }
    UFUNCTION(BlueprintCallable) void ContinueSavedGame();
    void BeginTitleToWall(class AMainGameDoor* Door);
    bool HasTitleLanded() const { return bTitleLanded; }
    static bool IsOpening(UWorld* World);
    static AMiningOpening* EnsureMainGameOpening(class AMiningExhibitHall* Hall);
    UPROPERTY(EditAnywhere, Category="Opening") TObjectPtr<UTexture2D> TitleTexture;
    UPROPERTY(EditAnywhere, Category="Opening") FVector SeatEye;
    UPROPERTY(EditAnywhere, Category="Opening") FVector StandingLocation;
    UPROPERTY(EditAnywhere, Category="Opening") FRotator Facing = FRotator(0,15,0);
    UPROPERTY(EditAnywhere, Category="Opening") FVector DoorTitleLocation;
    UPROPERTY(EditAnywhere, Category="Opening") FRotator DoorTitleRotation = FRotator(0,180,0);
    UPROPERTY(EditAnywhere, Category="Opening", meta=(ClampMin="1")) float Duration = 3.2f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bStarted = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bFinished = false;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> WorldTitle;
private:
    friend class FOpeningTitleProjectionTest;
    void Prepare(APlayerController* Controller);
    void Release();
    void EnsureTitleOnWall();
    UPROPERTY(Transient) TObjectPtr<UMiningOpeningMenu> Menu;
    UPROPERTY(Transient) TObjectPtr<UMiningOpeningTitle> CornerTitle;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
    TWeakObjectPtr<APlayerController> Player;
    FVector InitialTitle;
    FVector StandingEye;
    float StandingFOV=90;
    FVector SeatPawnLocation;
    FQuat InitialRotation;
    float Elapsed = 0;
    bool bPrepared = false;
    TSharedPtr<class SWidget> CollectionChoice;
    bool bInputLocked = false;
    bool bOldCollision = true;
    bool bOldHUD = true;
    bool bTitleLanded=false;
    float InitialTitleScale=.07f;
};
