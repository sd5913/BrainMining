#include "MiningProgressionComponent.h"
#include "MiningLocalizationSubsystem.h"
#include "MiningAchievementSubsystem.h"
#include "Subtitles/SubtitleLibrary.h"
#include "BrainMiningCharacter.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningWalletSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"

bool FMiningUpgradeStep::IsValid() const
{
    return Price > 0 && FMath::IsFinite(Value) && Value > 0 && FMath::IsFinite(SecondaryValue) && SecondaryValue > 0 &&
        (Effect != EMiningUpgradeEffect::MineralDoubleChance || Value <= 1);
}

UMiningProgressionComponent::UMiningProgressionComponent()
{
    const TCHAR* Names[] = {TEXT("UI.Tool.Pickaxe"), TEXT("UI.Tool.Jetpack"), TEXT("UI.Tool.Bomb"), TEXT("UI.Tool.Drill")};
    for (const TCHAR* Name : Names) Tracks.AddDefaulted_GetRef().Name = MiningText::Get(Name);
    const auto Add = [this](int32 Track, int64 Price, EMiningUpgradeEffect Effect, float Value, const FText& Description, float Secondary = 1)
    {
        auto& Step = Tracks[Track].Steps.AddDefaulted_GetRef();
        Step.Price = Price; Step.Effect = Effect; Step.Value = Value; Step.SecondaryValue = Secondary;
        Step.Description = Description;
    };
    const auto PickaxeLevels = UMiningPickaxeSettings::MakeRecommendedLevels();
    for (int32 Level = 1; Level <= 4; ++Level)
        Add(0, PickaxeLevels[Level].Price, EMiningUpgradeEffect::PickaxeProfile, Level, PickaxeLevels[Level].GetDescription());
    Add(1, 250, EMiningUpgradeEffect::CoolingDelay, .5f, MiningText::Get(TEXT("UI.Upgrade.CoolingDelay")));
    Add(1, 7500, EMiningUpgradeEffect::CoolingRate, 2, MiningText::Get(TEXT("UI.Upgrade.CoolingRate")));
    Add(1, 25000, EMiningUpgradeEffect::HeatCapacity, 2, MiningText::Get(TEXT("UI.Upgrade.HeatCapacity")));
    Add(1, 125000, EMiningUpgradeEffect::CreativeFlight, 1, MiningText::Get(TEXT("UI.Upgrade.CreativeFlight")));
    Add(2, 1500, EMiningUpgradeEffect::BombRecharge, .8f, MiningText::Get(TEXT("UI.Upgrade.BombRecharge")));
    Add(2, 10000, EMiningUpgradeEffect::BombCapacity, 2, MiningText::Get(TEXT("UI.Upgrade.BombCapacity")));
    Add(2, 40000, EMiningUpgradeEffect::BombRadius, 2, MiningText::Get(TEXT("UI.Upgrade.BombRadius")));
    Add(2, 160000, EMiningUpgradeEffect::BombCapacityAndFuse, 2, MiningText::Get(TEXT("UI.Upgrade.BombCapacityAndFuse")), .5f);
    Add(3, 20000, EMiningUpgradeEffect::DrillRecovery, 1.5f, MiningText::Get(TEXT("UI.Upgrade.DrillRecovery")));
    Add(3, 100000, EMiningUpgradeEffect::DrillCapacity, 1.5f, MiningText::Get(TEXT("UI.Upgrade.DrillCapacity")));
    Add(3, 500000, EMiningUpgradeEffect::DrillConsumption, .75f, MiningText::Get(TEXT("UI.Upgrade.DrillConsumption")));
    Add(3, 2000000, EMiningUpgradeEffect::InfiniteDrill, 1, MiningText::Get(TEXT("UI.Upgrade.InfiniteDrill")));
}

bool UMiningProgressionComponent::InitializeProgression()
{
    if (bInitialized) return true;
    auto* Mining = GetOwner()->FindComponentByClass<UMiningComponent>();
    auto* Flight = GetOwner()->FindComponentByClass<UMiningCharacterMovementComponent>();
    auto* Bomb = GetOwner()->FindComponentByClass<UMiningBombToolComponent>();
    if (!Mining || !Flight || !Bomb || !GetOwner()->GetGameInstance()) return false;
    BaseRadius = Mining->Radius; BaseDepth = Mining->DepthMultiplier; BaseInterval = Mining->Interval;
    BaseFlight = Flight->FlightSettings;
    BaseCoolingDelay = Flight->CoolingDelay > 0 ? Flight->CoolingDelay : 1.f;
    BaseBomb = Bomb->Settings; BaseBombCapacity = FMath::Max(1, Bomb->MaxBombs); BaseBombRecharge = FMath::Max(.1f, Bomb->RechargeSeconds);
    BaseDrillCapacity = FMath::Max(1.f, Mining->MaxDrillEnergy);
    BaseDrillRecovery = FMath::Max(0.f, Mining->DrillRecoveryRate);
    BaseDrillConsumption = FMath::Max(0.f, Mining->DrillConsumptionRate);
    Mining->bUseDrillEnergy = true; Mining->DrillEnergy = BaseDrillCapacity;
    Bomb->bUseInventory = true; Bomb->BombCount = BaseBombCapacity;
    bInitialized = true;
    ApplyUpgrades();
    return true;
}

UMiningWalletSubsystem* UMiningProgressionComponent::GetWallet() const
{
    const auto* Instance = GetOwner() ? GetOwner()->GetGameInstance() : nullptr;
    return Instance ? Instance->GetSubsystem<UMiningWalletSubsystem>() : nullptr;
}

int32 UMiningProgressionComponent::GetLevel(int32 TrackIndex) const
{
    if (UsesMineralInventory() && TrackIndex != 0) return 0;
    const auto* Wallet = GetWallet();
    return Wallet ? Wallet->GetUpgradeLevel(TrackIndex) : INDEX_NONE;
}

bool UMiningProgressionComponent::UsesMineralInventory() const
{
    const auto* Wallet = GetWallet();
    return Wallet && Wallet->UsesMineralInventory();
}

bool UMiningProgressionComponent::CanUpgradePickaxe() const
{
    const int32 Level = GetLevel(0);
    return bInitialized && UsesMineralInventory() && Level >= 0 && Level < 4 &&
        GetWallet()->CanSpendMinerals(UMiningPickaxeSettings::ResolveLevel(Level + 1).MaterialCosts);
}

FText UMiningProgressionComponent::GetNextMaterialRequirements() const
{
    const int32 Level = GetLevel(0);
    if (Level < 0 || Level >= 4) return MiningText::Get(TEXT("UI.Upgrade.Max"));
    TArray<FString> Lines;
    for (const auto& Cost : UMiningPickaxeSettings::ResolveLevel(Level + 1).MaterialCosts)
        Lines.Add(FString::Printf(TEXT("%s  %d / %d"), *FMiningMineralCost::Name(Cost.Mineral), GetWallet()->GetMineralCount(Cost.Mineral), Cost.Count));
    return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

int64 UMiningProgressionComponent::GetNextPrice(int32 TrackIndex) const
{
    if (UsesMineralInventory()) return 0;
    const int32 Level = GetLevel(TrackIndex);
    if (TrackIndex == 0) return Level >= 0 && Level < 4 ? UMiningPickaxeSettings::ResolveLevel(Level + 1).Price : 0;
    return Tracks.IsValidIndex(TrackIndex) && Tracks[TrackIndex].Steps.IsValidIndex(Level) && Level < 4 ? Tracks[TrackIndex].Steps[Level].Price : 0;
}

FText UMiningProgressionComponent::GetNextDescription(int32 TrackIndex) const
{
    const int32 Level = GetLevel(TrackIndex);
    if (TrackIndex == 0) return Level >= 0 && Level < 4 ? UMiningPickaxeSettings::ResolveLevel(Level + 1).GetDescription() : MiningText::Get(TEXT("UI.Upgrade.Max"));
    return Tracks.IsValidIndex(TrackIndex) && Tracks[TrackIndex].Steps.IsValidIndex(Level) && Level < 4 ?
        Tracks[TrackIndex].Steps[Level].Description : MiningText::Get(TEXT("UI.Upgrade.Max"));
}

bool UMiningProgressionComponent::Purchase(int32 TrackIndex, int32 RequestedLevel)
{
    if (!bInitialized || !Tracks.IsValidIndex(TrackIndex)) return false;
    if (UsesMineralInventory())
    {
        if (TrackIndex != 0 || RequestedLevel != GetLevel(0) + 1 || !CanUpgradePickaxe()) return false;
        auto* Wallet = GetWallet();
        if (!Wallet->SpendMinerals(UMiningPickaxeSettings::ResolveLevel(RequestedLevel).MaterialCosts)) return false;
        Wallet->UpgradeLevels[0] = RequestedLevel;
        ApplyUpgrades();
        GetOwner()->GetGameInstance()->GetSubsystem<UMiningAchievementSubsystem>()->RecordPickaxe(RequestedLevel);
        USubtitleLibrary::PlayMainGameSubtitle(this,FName(*FString::Printf(TEXT("镐子升级%d"),RequestedLevel)));
        return true;
    }
    if (TrackIndex == 2)
        if (const auto* Bomb = GetOwner()->FindComponentByClass<UMiningBombToolComponent>(); !Bomb || !Bomb->IsSkillUnlocked()) return false;
    auto* Wallet = GetOwner()->GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    const int32 Current = Wallet->GetUpgradeLevel(TrackIndex);
    if (TrackIndex == 0)
    {
        if (Current < 0 || RequestedLevel != Current + 1 || RequestedLevel > 4) return false;
        if (!Wallet->Spend(UMiningPickaxeSettings::ResolveLevel(RequestedLevel).Price)) return false;
        Wallet->UpgradeLevels[0] = RequestedLevel;
        ApplyUpgrades();
        GetOwner()->GetGameInstance()->GetSubsystem<UMiningAchievementSubsystem>()->RecordPickaxe(RequestedLevel);
        USubtitleLibrary::PlayMainGameSubtitle(this,FName(*FString::Printf(TEXT("镐子升级%d"),RequestedLevel)));
        return true;
    }
    const auto& Steps = Tracks[TrackIndex].Steps;
    if (Current < 0 || RequestedLevel != Current + 1 || RequestedLevel > 4 || !Steps.IsValidIndex(Current) || !Steps[Current].IsValid()) return false;
    if (!Wallet->Spend(Steps[Current].Price)) return false;
    Wallet->UpgradeLevels[TrackIndex] = RequestedLevel;
    ApplyUpgrades();
    return true;
}

void UMiningProgressionComponent::RefreshPickaxeTrack()
{
    if (UsesMineralInventory()) Tracks.SetNum(1);
    if (Tracks.IsEmpty()) Tracks.AddDefaulted();
    Tracks[0].Name = MiningText::Get(TEXT("UI.Tool.Pickaxe"));
    Tracks[0].Steps.Reset();
    for (int32 Level = 1; Level <= 4; ++Level)
    {
        const auto Profile = UMiningPickaxeSettings::ResolveLevel(Level);
        auto& Step = Tracks[0].Steps.AddDefaulted_GetRef();
        Step.Price = Profile.Price;
        Step.Description = Profile.GetDescription();
        Step.Effect = EMiningUpgradeEffect::PickaxeProfile;
        Step.Value = Level;
    }
}

void UMiningProgressionComponent::ApplyUpgrades()
{
    if (!bInitialized) return;
    auto* Mining = GetOwner()->FindComponentByClass<UMiningComponent>();
    auto* Flight = GetOwner()->FindComponentByClass<UMiningCharacterMovementComponent>();
    auto* Bomb = GetOwner()->FindComponentByClass<UMiningBombToolComponent>();
    if (!Mining || !Flight || !Bomb) return;
    auto* Wallet = GetOwner()->GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    const float PreviousHeatSeconds = Flight->FlightSettings.HeatUpSeconds;
    Mining->Radius = BaseRadius; Mining->DepthMultiplier = BaseDepth; Mining->Interval = BaseInterval;
    Mining->MaxDrillEnergy = BaseDrillCapacity; Mining->DrillRecoveryRate = BaseDrillRecovery; Mining->DrillConsumptionRate = BaseDrillConsumption;
    Mining->bInfiniteDrillEnergy = false;
    Flight->FlightSettings.HeatUpSeconds = BaseFlight.HeatUpSeconds;
    Flight->FlightSettings.PassiveCoolSeconds = BaseFlight.PassiveCoolSeconds;
    Flight->FlightSettings.OverheatLockSeconds = BaseFlight.OverheatLockSeconds;
    Flight->CoolingDelay = BaseCoolingDelay; Flight->bCreativeFlight = false;
    Bomb->Settings = BaseBomb; Bomb->MaxBombs = BaseBombCapacity; Bomb->RechargeSeconds = BaseBombRecharge;
    Wallet->MineralDoubleChance = 0;
    RefreshPickaxeTrack();
    Mining->ApplyPickaxeBalance(UMiningPickaxeSettings::ResolveLevel(GetLevel(0)));
    for (int32 TrackIndex = 1; TrackIndex < Tracks.Num(); ++TrackIndex)
    {
        const auto& Steps = Tracks[TrackIndex].Steps;
        for (int32 Level = 0; Level < FMath::Min3(GetLevel(TrackIndex), Steps.Num(), 4); ++Level)
        {
            const auto& Step = Steps[Level];
            if (!Step.IsValid()) continue;
            const float Value = Step.Value;
            switch (Step.Effect)
            {
            case EMiningUpgradeEffect::MiningDepth: Mining->DepthMultiplier *= Value; break;
            case EMiningUpgradeEffect::MiningRadius: Mining->Radius *= Value; break;
            case EMiningUpgradeEffect::MiningInterval: Mining->Interval *= Value; break;
            case EMiningUpgradeEffect::MineralDoubleChance: Wallet->MineralDoubleChance = Value; break;
            case EMiningUpgradeEffect::CoolingDelay: Flight->CoolingDelay *= Value; break;
            case EMiningUpgradeEffect::CoolingRate:
                Flight->FlightSettings.PassiveCoolSeconds /= Value; Flight->FlightSettings.OverheatLockSeconds /= Value; break;
            case EMiningUpgradeEffect::HeatCapacity:
                Flight->FlightSettings.HeatUpSeconds *= Value; Flight->FlightSettings.PassiveCoolSeconds *= Value;
                Flight->FlightSettings.OverheatLockSeconds *= Value; break;
            case EMiningUpgradeEffect::CreativeFlight: Flight->bCreativeFlight = true; break;
            case EMiningUpgradeEffect::BombRecharge: Bomb->RechargeSeconds *= Value; break;
            case EMiningUpgradeEffect::BombCapacity: Bomb->MaxBombs = FMath::RoundToInt(Bomb->MaxBombs * Value); break;
            case EMiningUpgradeEffect::BombRadius: Bomb->Settings.ExcavationRadius *= Value; Bomb->Settings.BlastRadius *= Value; break;
            case EMiningUpgradeEffect::BombCapacityAndFuse:
                Bomb->MaxBombs = FMath::RoundToInt(Bomb->MaxBombs * Value); Bomb->Settings.FuseSeconds *= Step.SecondaryValue; break;
            case EMiningUpgradeEffect::DrillRecovery: Mining->DrillRecoveryRate *= Value; break;
            case EMiningUpgradeEffect::DrillCapacity: Mining->MaxDrillEnergy *= Value; break;
            case EMiningUpgradeEffect::DrillConsumption: Mining->DrillConsumptionRate *= Value; break;
            case EMiningUpgradeEffect::InfiniteDrill: Mining->bInfiniteDrillEnergy = true; break;
            }
        }
    }
    Flight->JetpackHeat = FMath::Clamp(Flight->JetpackHeat * PreviousHeatSeconds / FMath::Max(.1f, Flight->FlightSettings.HeatUpSeconds), 0.f, 1.f);
    Mining->DrillEnergy = FMath::Clamp(Mining->DrillEnergy, 0.f, Mining->MaxDrillEnergy);
    Bomb->BombCount = FMath::Clamp(Bomb->BombCount, 0, Bomb->MaxBombs);
    if (auto* Character = Cast<ABrainMiningCharacter>(GetOwner()))
    {
        Character->RefreshPickaxeVisual(GetLevel(0));
    }
}
