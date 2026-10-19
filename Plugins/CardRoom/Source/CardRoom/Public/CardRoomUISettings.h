#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CardRoomUISettings.generated.h"

USTRUCT(BlueprintType)
struct CARDROOM_API FCardRoomUIBox
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FVector2D Position = FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FVector2D Size = FVector2D(100.f, 50.f);
};

UCLASS(BlueprintType)
class CARDROOM_API UCardRoomUISettings : public UDataAsset
{
    GENERATED_BODY()
public:
    // All positions use the CardRoom virtual canvas (1600 x 900). These are
    // presentation data so designers can tune the screen without recompiling.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Artwork") TObjectPtr<UTexture2D> RoundArt;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Artwork") TObjectPtr<UTexture2D> SpeechBubbleArt;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Artwork") TObjectPtr<UTexture2D> PlayerHealthArt;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Artwork") TObjectPtr<UTexture2D> BossHealthArt;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Artwork") TObjectPtr<UTexture2D> CardLaserArt;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Artwork") TObjectPtr<UTexture2D> CardPoisonArt;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Artwork") TObjectPtr<UTexture2D> CardFireArt;
    // Optional baked selected-state artwork. When set, the card artwork carries
    // its own lower label so the view does not need to draw a second label.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Artwork") TObjectPtr<UTexture2D> CardLaserSelectedArt;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Artwork") TObjectPtr<UTexture2D> CardPoisonSelectedArt;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Artwork") TObjectPtr<UTexture2D> CardFireSelectedArt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FCardRoomUIBox Round;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FCardRoomUIBox PlayerHealth;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FCardRoomUIBox BossHealth;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FCardRoomUIBox Dialogue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FCardRoomUIBox SeatedPanel;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FCardRoomUIBox Prompt;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FCardRoomUIBox Confirm;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FCardRoomUIBox SelectAll;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FCardRoomUIBox Victory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FCardRoomUIBox BottomHint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout") FCardRoomUIBox Poison;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cards") FVector2D CardNormalSize = FVector2D(138.f, 200.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cards") FVector2D CardManySize = FVector2D(64.f, 96.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cards") float CardNormalStep = 160.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cards") float CardManyStep = 75.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cards") float CardTop = 672.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cards") float CardSelectedLift = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cards") float CardHandCenterX = 800.f;
};
