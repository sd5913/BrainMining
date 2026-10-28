#pragma once
#include "CoreMinimal.h"

enum class ECardRoomPhase : uint8 { Exploring, Seated, Matching, Arrival, Choosing, Resolving, Victory, Collected };
enum class ERoomCard : uint8 { Laser, Poison, Fire, Lightning, Ice, Grass, Wind, Water, Earth, Metal, Light, Shadow, Star, Gravity, Time, Echo, Dragon, Count };

// Engine-independent encounter rules. No timers, actors, input, rewards or mining dependencies.
struct CARDROOM_API FCardRoomRun
{
    ECardRoomPhase Phase = ECardRoomPhase::Exploring;
    int32 Round = 0;
    int32 PlayerHP = 100;
    int32 RivalHP = 100;
    bool bPoisoned = false;
    float PhaseTime = 0;
    TArray<ERoomCard> Hand;
    TSet<int32> Staged;
    FString Message = TEXT("欢迎来到余烬牌室。走近牌桌，按 E 入座。");

    bool Seat();
    bool Match();
    bool Stage(int32 Index);
    bool Unstage(int32 Index);
    bool Confirm();
    bool Collect();
    void Restart();
    void Tick(float DeltaSeconds);
    bool CanConfirm() const;
    int32 Remaining() const { return Hand.Num() - Staged.Num(); }
    static FString CardName(ERoomCard Card);
    static FString CardDescription(ERoomCard Card);
    static const TCHAR* CardKey(ERoomCard Card);
    static FLinearColor CardColor(ERoomCard Card);
    static constexpr float FusionSeconds=2.4f;
    float PresentationTime() const { return PhaseTime-(Phase==ECardRoomPhase::Resolving && Round==4?FusionSeconds:0.f); }
private:
    void Deal(int32 NewRound);
    void SetPhase(ECardRoomPhase NewPhase);
    bool bPlayerResolved = false;
    bool bRivalResolved = false;
};
