#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "MiningPickaxeSettings.h"

class IDetailsView;

class SMiningPickaxeBalancePanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMiningPickaxeBalancePanel) {}
    SLATE_END_ARGS()
    void Construct(const FArguments& Arguments);

private:
    TStrongObjectPtr<UMiningPickaxeSettings> Draft;
    TSharedPtr<IDetailsView> Details;
    int32 PreviewLevel = 0;
    FString Message;
    FReply Save();
    FReply Reload();
    FReply Recommended();
    FReply SetStages(int32 Count);
    FReply Preview();
    FReply Restore();
    int32 ApplySavedToPIE();
    FText GetSummary() const;
    FText GetValidation() const;
    bool IsDirty() const;
};
