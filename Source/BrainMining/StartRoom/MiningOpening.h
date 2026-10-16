#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "MiningOpening.generated.h"

class AMiningOpening;
class UTexture2D;
class UWidgetComponent;
class ACameraActor;

UCLASS()
class BRAINMINING_API UMiningOpeningTitle : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UTexture2D> Texture;
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
    void Prepare(APlayerController* Controller);
    void Release();
    UPROPERTY(Transient) TObjectPtr<UMiningOpeningMenu> Menu;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
    TWeakObjectPtr<APlayerController> Player;
    FVector InitialTitle;
    FVector StandingEye;
    FVector SeatPawnLocation;
    FQuat InitialRotation;
    float Elapsed = 0;
    bool bPrepared = false;
    bool bInputLocked = false;
    bool bOldCollision = true;
    bool bOldHUD = true;
};
