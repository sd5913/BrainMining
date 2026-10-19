#include "MiningHapticMixer.h"

FMiningHapticPattern FMiningHapticMixer::Pattern(EMiningHapticCue Cue)
{
    FMiningHapticPattern Result;
    const auto Set = [&](float Low, float High, float Duration, int32 Pulses, int32 Priority, int32 Group, float Cooldown = .15f)
    {
        Result.Low = Low; Result.High = High; Result.Duration = Duration;
        Result.Pulses = Pulses; Result.Priority = Priority; Result.Group = Group; Result.Cooldown = Cooldown;
    };
    switch (Cue)
    {
    case EMiningHapticCue::PickaxeContact: Set(.3f, .38f, .075f, 1, 1, 1, .06f); break;
    case EMiningHapticCue::PickaxeBreak: Set(.55f, .4f, .15f, 1, 2, 1, .09f); break;
    case EMiningHapticCue::BlockedContact: Set(.12f, .22f, .05f, 1, 1, 1, .2f); break;
    case EMiningHapticCue::DrillStart: Set(.16f, .22f, .12f, 1, 1, 0); break;
    case EMiningHapticCue::DrillLow: Set(.15f, .23f, .22f, 2, 3, 0, 1.f); break;
    case EMiningHapticCue::DrillEmpty: Set(.2f, .15f, .09f, 1, 3, 0, .5f); break;
    case EMiningHapticCue::JetStart: Set(.23f, .12f, .18f, 1, 1, 0, .25f); break;
    case EMiningHapticCue::JetOverheat: Set(.38f, .27f, .28f, 2, 3, 0, 1.f); break;
    case EMiningHapticCue::Landing: Set(.5f, .18f, .14f, 1, 2, 3, .18f); break;
    case EMiningHapticCue::BombEquip: Set(.08f, .12f, .045f, 1, 1, 5, .2f); Result.bOptional = true; break;
    case EMiningHapticCue::BombThrow: Set(.2f, .23f, .085f, 1, 2, 5); break;
    case EMiningHapticCue::BombStick: Set(.12f, .19f, .055f, 1, 1, 0); break;
    case EMiningHapticCue::Explosion: Set(.7f, .45f, .32f, 1, 4, 2); break;
    case EMiningHapticCue::UpgradeSuccess: Set(.28f, .23f, .2f, 2, 2, 4); break;
    case EMiningHapticCue::UpgradeRejected: Set(.1f, .17f, .055f, 1, 1, 0, .35f); break;
    case EMiningHapticCue::Equipment: Set(.27f, .2f, .22f, 2, 2, 4); break;
    case EMiningHapticCue::Collectible: Set(.18f, .24f, .16f, 2, 2, 4); break;
    case EMiningHapticCue::SkillUnlocked: Set(.32f, .24f, .25f, 2, 2, 4); break;
    case EMiningHapticCue::EchoKnock: Set(.24f, .28f, .075f, 1, 2, 1, .2f); break;
    case EMiningHapticCue::EchoCollapse: Set(.45f, .3f, .24f, 1, 3, 0, .5f); break;
    case EMiningHapticCue::EchoLanding: Set(.5f, .28f, .16f, 1, 3, 3, .18f); break;
    case EMiningHapticCue::PuzzleSwap: Set(.15f, .21f, .15f, 2, 1, 0); break;
    case EMiningHapticCue::PuzzleRejected: Set(.1f, .14f, .045f, 1, 1, 0, .35f); Result.bOptional = true; break;
    case EMiningHapticCue::PuzzleComplete: Set(.32f, .25f, .25f, 2, 2, 4, .5f); break;
    case EMiningHapticCue::OfficeWarning: Set(.18f, .22f, .5f, 3, 2, 0, 2.f); Result.bOptional = true; break;
    case EMiningHapticCue::DoorBreak: Set(.65f, .4f, .32f, 1, 4, 2); break;
    case EMiningHapticCue::Seal: Set(.35f, .2f, .22f, 1, 3, 2, .3f); break;
    case EMiningHapticCue::Collapse: Set(.5f, .3f, .35f, 1, 4, 2, .3f); break;
    case EMiningHapticCue::TitleLanded: Set(.08f, .11f, .055f, 1, 1, 0, 1.f); Result.bOptional = true; break;
    case EMiningHapticCue::FinaleHit: Set(.32f, .25f, .11f, 1, 2, 1, .14f); break;
    default: break;
    }
    return Result;
}

float FMiningHapticMixer::DistanceGain(float Distance, float Radius)
{
    if (!FMath::IsFinite(Distance) || !FMath::IsFinite(Radius) || Radius <= 0) return 0;
    return FMath::Square(1.f - FMath::Clamp(Distance / Radius, 0.f, 1.f));
}

bool FMiningHapticMixer::Play(EMiningHapticCue Cue, float Gain)
{
    if (Cue >= EMiningHapticCue::Count || !FMath::IsFinite(Gain) || Gain <= 0) return false;
    const auto Preset = Pattern(Cue);
    if (const auto* Last = LastPlayed.Find(Cue); Last && Time - *Last < Preset.Cooldown) return false;
    for (auto& Pulse : Active)
    {
        if (Preset.Group == 0 || Pulse.Pattern.Group != Preset.Group || Time - Pulse.Started > .16) continue;
        if (Pulse.Pattern.Priority > Preset.Priority) return false;
        const float PreviousLow = Pulse.Pattern.Low * Pulse.Gain;
        const float PreviousHigh = Pulse.Pattern.High * Pulse.Gain;
        Pulse = {Cue, Preset, Time, FMath::Clamp(Gain, 0.f, 1.f)};
        Pulse.Pattern.Low = FMath::Max(PreviousLow, Preset.Low * Pulse.Gain);
        Pulse.Pattern.High = FMath::Max(PreviousHigh, Preset.High * Pulse.Gain);
        Pulse.Gain = 1;
        LastPlayed.Add(Cue, Time);
        return true;
    }
    Active.Add({Cue, Preset, Time, FMath::Clamp(Gain, 0.f, 1.f)});
    LastPlayed.Add(Cue, Time);
    return true;
}

FVector2D FMiningHapticMixer::Advance(float DeltaSeconds, FVector2D Continuous)
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0) { Reset(); return FVector2D::ZeroVector; }
    Time += DeltaSeconds;
    Active.RemoveAll([&](const FPulse& Pulse) { return Time - Pulse.Started >= Pulse.Pattern.Duration; });
    float Duck = 1;
    for (const auto& Pulse : Active) if (Pulse.Pattern.Priority >= 3) Duck = .2f;
    FVector2D Output = Continuous.ContainsNaN() ? FVector2D::ZeroVector : Continuous * Duck;
    for (const auto& Pulse : Active)
    {
        const float Progress = (Time - Pulse.Started) / Pulse.Pattern.Duration;
        const float Phase = FMath::Fmod(Progress * Pulse.Pattern.Pulses, 1.f);
        const float Envelope = Pulse.Cue == EMiningHapticCue::JetStart ? FMath::Sin(Phase * PI)
            : FMath::Min(Phase * 12.f, 1.f) * FMath::Pow(FMath::Max(0.f, 1.f - Phase / .8f), 1.2f);
        const float Rise = Pulse.Pattern.Pulses > 1 ? FMath::Lerp(.6f, 1.f, Progress) : 1.f;
        Output.X = FMath::Max(Output.X, double(Pulse.Pattern.Low * Pulse.Gain * Envelope * Rise));
        Output.Y = FMath::Max(Output.Y, double(Pulse.Pattern.High * Pulse.Gain * Envelope * Rise));
    }
    return FVector2D(FMath::Clamp(Output.X, 0., 1.), FMath::Clamp(Output.Y, 0., 1.));
}

void FMiningHapticMixer::Reset() { Active.Reset(); LastPlayed.Reset(); Time = 0; }
bool FMiningHapticMixer::IsActive(EMiningHapticCue Cue) const
{
    return Active.ContainsByPredicate([&](const FPulse& Pulse) { return Pulse.Cue == Cue; });
}
