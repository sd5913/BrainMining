#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MiningGUIMotion.h"
#include "MiningGUIWidgets.generated.h"

class UCanvasPanel;
class UImage;
class UButton;
class UTextBlock;
class UProgressBar;
class UTexture2D;
class UMaterialInstanceDynamic;

/** Transient arc gauge driven by actual flight heat; presentation never controls flight. */
UCLASS()
class BRAINMINING_API UMiningGUIJetpackHeatWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Jetpack Heat", meta=(ClampMin="0", ClampMax="1")) float PreviewHeat = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Jetpack Heat") bool bPreviewOverheated = false;
	UPROPERTY(BlueprintReadOnly, Category="GUI|Jetpack Heat") float DisplayedHeat = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="GUI|Jetpack Heat") bool bDisplayedOverheated = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Jetpack Heat", meta=(ClampMin="0.1", ClampMax="0.95")) float WarningThreshold = .75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Jetpack Heat", meta=(ClampMin="0.01")) float GaugeFadeInSeconds = .16f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Jetpack Heat", meta=(ClampMin="0.01")) float GaugeFadeOutSeconds = .35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Jetpack Heat", meta=(ClampMin="0")) float HideDelaySeconds = .25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Jetpack Heat", meta=(ClampMin="0", ClampMax="1")) float WarningMaxOpacity = .55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Jetpack Heat", meta=(ClampMin="0.01")) float WarningFadeInSeconds = .25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Jetpack Heat", meta=(ClampMin="0.01")) float WarningFadeOutSeconds = .45f;
	UPROPERTY(BlueprintReadOnly, Category="GUI|Jetpack Heat") float GaugeOpacity = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="GUI|Jetpack Heat") float WarningOpacity = 0.f;
	UFUNCTION(BlueprintCallable, Category="GUI|Jetpack Heat") void SetHeatState(float Heat, bool bOverheated, bool bThrustActive = false);
	/** Called once by the owning HUD, including while fully transparent. */
	UFUNCTION(BlueprintCallable, Category="GUI|Jetpack Heat") void AdvancePresentation(float DeltaSeconds);
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UImage> HeatBar;
protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> HeatMaterial;
	bool bDisplayedThrustActive = false;
	float GaugeFade = 0.f;
	float IdleSeconds = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMiningGUIUpgradeRequest, int32, ItemIndex, int32, RequestedLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMiningGUILevelChanged, int32, ItemIndex, int32, Level);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMiningGUIAbilityRequest, FName, Ability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMiningGUIPanelChanged, bool, bOpen);

/** Visual state only. Designer children, textures and motion settings remain editable. */
UCLASS()
class BRAINMINING_API UMiningGUIAbilityWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Ability") FName Ability = TEXT("G");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Ability") TObjectPtr<UTexture2D> NormalTexture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Ability") TObjectPtr<UTexture2D> UsedTexture;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GUI|Ability") bool bAvailable = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GUI|Ability") bool bUsed = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Motion", meta=(ClampMin="1")) float SpringStiffness = 420.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Motion", meta=(ClampMin="1")) float SpringDamping = 19.f;
	UFUNCTION(BlueprintCallable, Category="GUI|Ability") void SetUsed(bool bNewUsed);
	UFUNCTION(BlueprintCallable, Category="GUI|Ability") void SetAvailable(bool bNewAvailable);
	/** Gameplay owns the timer; this widget only displays it over the red icon. */
	UFUNCTION(BlueprintCallable, Category="GUI|Ability") void SetCooldownRemaining(float Seconds);
	UFUNCTION(BlueprintCallable, Category="GUI|Ability") void SetRechargeState(bool bRecharging, float Progress);
	UPROPERTY(BlueprintReadOnly, Transient, Category="GUI|Ability") TObjectPtr<UProgressBar> RechargeBar;
	UPROPERTY(BlueprintReadOnly, Category="GUI|Ability") float CooldownRemaining = 0;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> CooldownText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UImage> AbilityIcon;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UCanvasPanel> AbilityContent;
protected:
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	FMiningGUISpring Pulse;
};

/** An upgrade card has four independent green overlays and a separate lock layer. */
UCLASS()
class BRAINMINING_API UMiningGUIUpgradeWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Item") int32 ItemIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GUI|Item", meta=(ClampMin="0", ClampMax="4")) int32 UpgradeLevel = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GUI|Item") bool bUnlocked = true;
	/** Turn off when gameplay owns upgrade decisions; requests still broadcast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Prototype") bool bPreviewUpgrades = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Motion", meta=(ClampMin="1")) float SpringStiffness = 420.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Motion", meta=(ClampMin="1")) float SpringDamping = 19.f;
	UPROPERTY(BlueprintAssignable, Category="GUI|Events") FMiningGUIUpgradeRequest OnUpgradeRequested;
	UPROPERTY(BlueprintAssignable, Category="GUI|Events") FMiningGUILevelChanged OnLevelChanged;
	UFUNCTION(BlueprintCallable, Category="GUI|Item") void SetUpgradeLevel(int32 NewLevel, bool bAnimate = true);
	UFUNCTION(BlueprintCallable, Category="GUI|Item") void SetUnlocked(bool bNewUnlocked);
	UFUNCTION(BlueprintCallable, Category="GUI|Item") void SetPriceDisplay(FText NewText);
	/** Same handler as the coin button: no money checks, no deductions, no gameplay. */
	UFUNCTION(BlueprintCallable, Category="GUI|Prototype") void RequestUpgrade();
	void SetInteractionAllowed(bool bAllowed);
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> CoinButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> PriceText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UCanvasPanel> PriceContent;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UCanvasPanel> RingContent;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UImage> RingGrey;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UImage> ItemIcon;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UImage> LockedRing;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UImage> LockIcon;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UImage> GreenSegment1;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UImage> GreenSegment2;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UImage> GreenSegment3;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> GreenSegment4;
protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	UFUNCTION() void PressCoin();
	UFUNCTION() void ReleaseCoin();
	void RefreshVisuals();
	FMiningGUISpring ClickSpring;
	FMiningGUISpring RingSpring;
	bool bInteractionAllowed = true;
};

/** Mounted once by the local player controller. All visual state is public to Blueprint. */
UCLASS()
class BRAINMINING_API UMiningGUIHUDWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Demo mode marks G used. Drill V is driven by the mining component's cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Prototype") bool bPreviewAbilities = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Motion", meta=(ClampMin="1")) float PanelSpringStiffness = 360.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GUI|Motion", meta=(ClampMin="1")) float PanelSpringDamping = 18.f;
	UPROPERTY(BlueprintReadOnly, Category="GUI|State") bool bUpgradePanelOpen = false;
	UPROPERTY(BlueprintAssignable, Category="GUI|Events") FMiningGUIUpgradeRequest OnUpgradeRequested;
	UPROPERTY(BlueprintAssignable, Category="GUI|Events") FMiningGUILevelChanged OnLevelChanged;
	UPROPERTY(BlueprintAssignable, Category="GUI|Events") FMiningGUIAbilityRequest OnAbilityRequested;
	UPROPERTY(BlueprintAssignable, Category="GUI|Events") FMiningGUIPanelChanged OnPanelChanged;
	UFUNCTION(BlueprintCallable, Category="GUI|Panel") void ToggleUpgradePanel();
	UFUNCTION(BlueprintCallable, Category="GUI|Panel") void SetUpgradePanelOpen(bool bOpen);
	UFUNCTION(BlueprintCallable, Category="GUI|Display") void SetCoinDisplay(FText NewText);
	UFUNCTION(BlueprintCallable, Category="GUI|Display") void SetItemLevel(int32 ItemIndex, int32 Level, bool bAnimate = true);
	UFUNCTION(BlueprintCallable, Category="GUI|Display") void SetItemUnlocked(int32 ItemIndex, bool bUnlocked);
	void EnableProgressionPanel();
	void RefreshMineralProgression();
    void QueueCollectionPickup(class AMiningCollectionField* Field,int32 Id);
    int32 ActivePickupId=INDEX_NONE;
    int32 CompletedPickupFlights=0;
    float PickupAge=0;
	UPROPERTY(BlueprintReadOnly, Category="GUI|State") bool bMineralProgression = false;
	UMiningGUIUpgradeWidget* GetUpgradeCard(int32 Index) const { return Item(Index); }
	void SetUpgradeDetails(int32 Index, FText Name, FText Description, int64 Price, bool bAffordable);
	UFUNCTION(BlueprintCallable, Category="GUI|Ability") void TriggerAbility(FName Ability);
	UFUNCTION(BlueprintCallable, Category="GUI|Ability") void SetAbilityUsed(FName Ability, bool bUsed);
	UFUNCTION(BlueprintCallable, Category="GUI|Ability") void SetAbilityAvailable(FName Ability, bool bAvailable);
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> PlusButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UCanvasPanel> PlusContent;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> CoinText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UCanvasPanel> UpgradePanel;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UMiningGUIUpgradeWidget> PickaxeUpgrade;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UMiningGUIUpgradeWidget> VortexUpgrade;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UMiningGUIUpgradeWidget> BlastUpgrade;
	UPROPERTY(Transient, BlueprintReadOnly) TObjectPtr<UMiningGUIUpgradeWidget> DrillUpgrade;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UMiningGUIAbilityWidget> AbilityG;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UMiningGUIAbilityWidget> AbilityV;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UMiningGUIJetpackHeatWidget> JetpackHeatBar;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> JetpackHeatWarning;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> DrillEnergyBar;
protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	UFUNCTION() void PressPlus();
	UFUNCTION() void ReleasePlus();
	UFUNCTION() void ForwardUpgrade(int32 ItemIndex, int32 RequestedLevel);
	UFUNCTION() void ForwardLevel(int32 ItemIndex, int32 Level);
	UMiningGUIUpgradeWidget* Item(int32 Index) const;
	UMiningGUIAbilityWidget* AbilityWidget(FName Name) const;
	FMiningGUISpring PanelSpring;
	FMiningGUISpring PlusSpring;
	float DrillEnergyFade = 0.f;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> UpgradeNames;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> UpgradeDescriptions;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> MineralPanel;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> LeftHUDStack;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> CollectionShortcutPanel;
	UPROPERTY(Transient) TObjectPtr<UImage> MineralBackplate;
	UPROPERTY(Transient) TObjectPtr<UImage> PickaxeBackplate;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> MineralCounts;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PickaxeLevelLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PickaxeRequirements;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> PickaxeRequirementRows;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> PickaxeRequirementCounts;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> PickaxeRequirementIcons;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PickaxeUpgradeHint;
    void RefreshTutorial(float DeltaSeconds);
    void LayoutLeftHUD(float TaskHeight);
    void LayoutTutorialRow(int32 Index, bool Completed, float& NextY);
    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> TutorialPanel;
    UPROPERTY(Transient) TObjectPtr<UImage> TutorialBackplate;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TutorialRows;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TutorialMarkers;
    float TutorialAge=0;
    float UpgradeAttentionAge=2.4f;
    bool bTutorialUpgradeRevealed=false;
    bool bTutorialSoilPresented=false;
    float TutorialSoilDoneAge=0;
    float TutorialDoneAge[6]={0,0,0,0,0,0};
    bool bMuseumCircleTaskPresented=false;
    int32 DisplayedPrototype=1;
    int32 DisplayedUpgrade=1;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CollectionShortcut;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CompletionToast;
    UPROPERTY(Transient) TObjectPtr<class USoundBase> CompletionSound;
    float CompletionToastAge=3.f;
    void TickCollectionPickup(const FGeometry& Geometry,float Dt);
    TArray<TPair<TWeakObjectPtr<class AMiningCollectionField>,int32>> PickupQueue;
    TWeakObjectPtr<class AMiningCollectionField> PickupField;
    UPROPERTY(Transient) TObjectPtr<UImage> PickupImage;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PickupLabel;
    float PickupCaptureAge=0,CollectionArrivalAge=1;
};
