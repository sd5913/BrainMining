#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MiningCollectionWidget.generated.h"

class AMiningCollectionField;

/** Layout lives in WBP_CollectionInspect. Native code only supplies data and input. */
UCLASS()
class BRAINMINING_API UMiningCollectionWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Collection") void SetCollection(AMiningCollectionField* Field);
    UFUNCTION(BlueprintCallable, Category="Collection") void RefreshCollection();
    UFUNCTION(BlueprintCallable, Category="Collection") void SelectCollectionItem(int32 Id);
    UFUNCTION(BlueprintCallable, Category="Collection") void TurnCollectionPage(int32 Direction);
    UFUNCTION(BlueprintCallable, Category="Collection") void CloseCollection();
protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseMove(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseWheel(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnKeyDown(const FGeometry&,const FKeyEvent&) override;
    virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent&) override;
private:
    UPROPERTY(Transient) TObjectPtr<AMiningCollectionField> Collection;
    bool bDragging=false;
    FVector2D LastMouse;
    bool InPreview(const FVector2D& ScreenPosition) const;
    UFUNCTION() void Select0();
    UFUNCTION() void Select1();
    UFUNCTION() void Select2();
    UFUNCTION() void Select3();
    UFUNCTION() void Previous();
    UFUNCTION() void Next();
};
