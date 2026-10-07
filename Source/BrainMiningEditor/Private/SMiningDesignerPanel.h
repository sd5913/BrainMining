#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "MiningDesignerLibrary.h"

class IDetailsView;
class SEditableTextBox;
class AMiningTunnelGenerator;
struct FDesignerActorItem
{
	TWeakObjectPtr<AActor> Actor;
};

class SMiningDesignerPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMiningDesignerPanel) {}
	SLATE_END_ARGS()
	void Construct(const FArguments& Arguments);
	virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;
private:
	TSharedPtr<IDetailsView> Details;
	TSharedPtr<SComboBox<TSharedPtr<FDesignerActorItem>>> ActorPicker;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> PresetPicker;
	TSharedPtr<SEditableTextBox> PresetName;
	TArray<TSharedPtr<FDesignerActorItem>> ActorOptions;
	TArray<TSharedPtr<FString>> PresetOptions;
	TArray<TWeakObjectPtr<AActor>> KnownActors;
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<AActor> LastSelection;
	EMiningDesignerSection Section = EMiningDesignerSection::Gameplay;
	FString Message;
	double NextRefresh = 0;
	bool bAdvancedUnlocked = false;
	bool bWasPlaying = false;
	bool bUpdatingPicker = false;
	bool Editable() const;
	void RefreshActors();
	void RefreshDetails();
	void RefreshPresets();
	void SelectTarget(AActor* Actor, bool bSelectInLevel);
	AMiningTunnelGenerator* Generator() const;
	TSharedRef<SWidget> Button(const FString& Label, TFunction<FReply()> Action, TFunction<bool()> Enabled = {});
	FReply SwitchSection(EMiningDesignerSection NewSection);
	FReply Rebuild();
	FReply CopyPreset();
	FReply PastePreset();
	FReply SavePreset();
	FReply LoadPreset();
	FReply ResetDefaults();
	FReply FocusTarget();
	FReply EditPath();
	FReply StraightBranch();
	FReply FinishOperation(bool bSuccess, const FString& Error, const FString& Success);
};
