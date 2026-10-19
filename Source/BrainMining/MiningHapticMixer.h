#pragma once

#include "CoreMinimal.h"

enum class EMiningHapticCue : uint8
{
    PickaxeContact, PickaxeBreak, BlockedContact, DrillStart, DrillLow, DrillEmpty,
    JetStart, JetOverheat, Landing, BombEquip, BombThrow, BombStick, Explosion,
    UpgradeSuccess, UpgradeRejected, Equipment, Collectible, SkillUnlocked,
    EchoKnock, EchoCollapse, EchoLanding, PuzzleSwap, PuzzleRejected, PuzzleComplete,
    OfficeWarning, DoorBreak, Seal, Collapse, TitleLanded, FinaleHit, Count
};

struct FMiningHapticPattern
{
    float Low = 0;
    float High = 0;
    float Duration = .1f;
    float Cooldown = .12f;
    int32 Pulses = 1;
    int32 Priority = 1;
    int32 Group = 0;
    bool bOptional = false;
};

class FMiningHapticMixer
{
public:
    static FMiningHapticPattern Pattern(EMiningHapticCue Cue);
    static float DistanceGain(float Distance, float Radius);
    bool Play(EMiningHapticCue Cue, float Gain = 1.f);
    FVector2D Advance(float DeltaSeconds, FVector2D Continuous = FVector2D::ZeroVector);
    void Reset();
    bool IsActive(EMiningHapticCue Cue) const;
    int32 NumActive() const { return Active.Num(); }
private:
    struct FPulse
    {
        EMiningHapticCue Cue;
        FMiningHapticPattern Pattern;
        double Started;
        float Gain;
    };
    double Time = 0;
    TArray<FPulse> Active;
    TMap<EMiningHapticCue, double> LastPlayed;
};
