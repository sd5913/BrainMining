#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EndingCreditsWidget.generated.h"

/** Layout/text/fonts live in WBP_EndingCredits; this class only controls timing and the CRT overlay. */
UCLASS()
class BRAINMINING_API UEndingCreditsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Credits", meta=(ClampMin="0")) float BlackFadeSeconds=1.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Credits", meta=(ClampMin="0")) float TextFadeSeconds=1.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Credits", meta=(ClampMin="0")) float PromptDelay=8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Credits|CRT", meta=(ClampMin="0",ClampMax=".3")) float CRTStrength=.055f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Credits|CRT", meta=(ClampMin="2",ClampMax="12")) float ScanlineSpacing=4.f;
protected:
    virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
    virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& CullingRect,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const override;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UImage> BlackBackground;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UCanvasPanel> TextGroup;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> Prompt;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class URichTextBlock> Contributors;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> ThankYou;
private:
    float Elapsed=0;
};
