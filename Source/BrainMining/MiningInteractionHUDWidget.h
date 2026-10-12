#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MiningInteractionHUDWidget.generated.h"

class UMiningWalletSubsystem;

struct FMiningCoinFeedback
{
    int64 Amount = 0;
    float Age = 2.f;
    static constexpr float Duration = 1.2f;
    static constexpr float MergeWindow = .35f;

    void Add(int64 Value)
    {
        if (Value <= 0) return;
        Amount = Age <= MergeWindow ? Amount + FMath::Min(Value, MAX_int64 - Amount) : Value;
        Age = 0.f;
    }
    void Advance(float DeltaSeconds) { Age = FMath::Min(Duration, Age + FMath::Max(0.f, DeltaSeconds)); }
    bool IsVisible() const { return Amount > 0 && Age < Duration; }
};

UCLASS()
class BRAINMINING_API UMiningInteractionHUDWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    static bool IsActiveFor(const APlayerController* Controller);
    UFUNCTION(BlueprintCallable, Category="GUI|Feedback") void ShowCoinGain(int64 Amount);
    UPROPERTY(BlueprintReadOnly, Category="GUI|Feedback") FString InteractionAction;
    UPROPERTY(BlueprintReadOnly, Category="GUI|Feedback") bool bExplorationVisible = false;
    UPROPERTY(BlueprintReadOnly, Category="GUI|Feedback") int64 DisplayedCoinGain = 0;
    UPROPERTY(BlueprintReadOnly, Category="GUI|Feedback") bool bCoinGainVisible = false;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;
private:
    FString ResolveInteractionAction() const;
    FMiningCoinFeedback CoinFeedback;
    TWeakObjectPtr<UMiningWalletSubsystem> Wallet;
    FDelegateHandle CoinsAddedHandle;
};
