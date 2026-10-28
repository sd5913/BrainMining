#include "CardRoomRun.h"
void FCardRoomRun::SetPhase(ECardRoomPhase NewPhase) { Phase = NewPhase; PhaseTime = 0; }
bool FCardRoomRun::Seat()
{
    if (Phase != ECardRoomPhase::Exploring) return false;
    SetPhase(ECardRoomPhase::Seated); Message = TEXT("请坐。今晚的对手，稍后就到。"); return true;
}
bool FCardRoomRun::Match()
{
    if (Phase != ECardRoomPhase::Seated) return false;
    SetPhase(ECardRoomPhase::Matching); Message = TEXT("正在匹配对手……"); return true;
}
bool FCardRoomRun::Stage(int32 Index)
{
    if (Phase != ECardRoomPhase::Choosing || !Hand.IsValidIndex(Index) || Staged.Contains(Index)) return false;
    Staged.Add(Index); return true;
}
bool FCardRoomRun::Unstage(int32 Index)
{
    if (Phase != ECardRoomPhase::Choosing) return false;
    return Staged.Remove(Index) > 0;
}
bool FCardRoomRun::CanConfirm() const { return Phase == ECardRoomPhase::Choosing && !Hand.IsEmpty() && Staged.Num() == Hand.Num(); }
bool FCardRoomRun::Confirm()
{
    if (!CanConfirm()) { if (Phase == ECardRoomPhase::Choosing) Message = TEXT("点击选中本回合的牌，再按空格打出。"); return false; }
    SetPhase(ECardRoomPhase::Resolving); bPlayerResolved = bRivalResolved = false;
    const TCHAR* Names[] = { TEXT(""), TEXT("激光弹！"), TEXT("这张牌……味道有点大。"), TEXT("火 + 激光弹！点燃还未散去的毒气。"), TEXT("17 张牌，最后一搏！") };
    Message = Names[Round]; return true;
}
void FCardRoomRun::Deal(int32 NewRound)
{
    Round = NewRound; Hand.Reset(); Staged.Reset(); SetPhase(ECardRoomPhase::Choosing);
    if (Round == 1) { Hand.Add(ERoomCard::Laser); Message = TEXT("第一回合 · 先来一发激光弹。"); }
    if (Round == 2) { Hand.Add(ERoomCard::Poison); Message = TEXT("第二回合 · 对手看起来毫无防备。"); }
    if (Round == 3) { Hand = { ERoomCard::Fire, ERoomCard::Laser }; Message = TEXT("第三回合 · 把火和激光弹一起打出，触发组合技。"); }
    if (Round == 4) { for (int32 i=0;i<static_cast<int32>(ERoomCard::Count);++i) Hand.Add(static_cast<ERoomCard>(i)); Message = TEXT("只剩 1 点生命！把 17 张牌全部混合，破釜沉舟。"); }
}
bool FCardRoomRun::Collect()
{
    if (Phase != ECardRoomPhase::Victory) return false;
    SetPhase(ECardRoomPhase::Collected); Message = TEXT("已拾取钻机。"); return true;
}
void FCardRoomRun::Restart() { *this = FCardRoomRun(); Seat(); }
void FCardRoomRun::Tick(float DeltaSeconds)
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
    PhaseTime += DeltaSeconds;
    if (Phase == ECardRoomPhase::Matching && PhaseTime >= 2.2f)
    {
        SetPhase(ECardRoomPhase::Arrival); Message = TEXT("匹配成功！即将安排超级人机对战！"); return;
    }
    if (Phase == ECardRoomPhase::Arrival && PhaseTime >= 1.4f) { Deal(1); return; }
    if (Phase != ECardRoomPhase::Resolving) return;
    if (!bPlayerResolved && PresentationTime() >= 1.0f)
    {
        bPlayerResolved = true;
        if (Round == 1) { RivalHP = 84; Message = TEXT("命中！对手捏起了一张「火」……"); }
        if (Round == 2) { RivalHP = 76; bPoisoned = true; Message = TEXT("噗——！对手：这也算魔法？！"); }
        if (Round == 3) { RivalHP = 28; bPoisoned = false; Message = TEXT("毒气 + 火激光 = 爆燃！对手生气了。"); }
        if (Round == 4) { RivalHP = 0; Message = TEXT("彗尾法球！超级人机，退场！"); }
    }
    if (!bRivalResolved && PresentationTime() >= 2.35f)
    {
        bRivalResolved = true;
        if (Round == 1) { PlayerHP = 80; Message = TEXT("对手打出「火」：小心火球！  -20"); }
        if (Round == 2) Message = TEXT("对手被臭晕了，本回合无法行动。毒气仍在。");
        if (Round == 3) { PlayerHP = 1; Message = TEXT("愤怒重击！你还剩 1 点生命……但还有最后一手。"); }
    }
    if (PresentationTime() >= (Round == 4 ? 4.1f : 4.6f))
    {
        if (Round < 4) Deal(Round + 1);
        else { SetPhase(ECardRoomPhase::Victory); Hand.Reset(); Staged.Reset(); Message = TEXT("恭喜你获得胜利！拾取钻机以获得钻机能力。"); }
    }
}
FString FCardRoomRun::CardName(ERoomCard Card)
{
    return FText::FromStringTable(TEXT("BrainMining.Game"),CardKey(Card)).ToString();
}
FString FCardRoomRun::CardDescription(ERoomCard Card)
{
    switch(Card) { case ERoomCard::Laser: return TEXT("一束精准的能量"); case ERoomCard::Poison: return TEXT("让对手无法行动"); default: return TEXT("与激光、毒气共鸣"); }
}

const TCHAR* FCardRoomRun::CardKey(ERoomCard Card)
{
    static const TCHAR* Keys[]={TEXT("Card.Laser"),TEXT("Card.Poison"),TEXT("Card.Fire"),TEXT("Card.Lightning"),TEXT("Card.Ice"),TEXT("Card.Grass"),TEXT("Card.Wind"),TEXT("Card.Water"),TEXT("Card.Earth"),TEXT("Card.Metal"),TEXT("Card.Light"),TEXT("Card.Shadow"),TEXT("Card.Star"),TEXT("Card.Gravity"),TEXT("Card.Time"),TEXT("Card.Echo"),TEXT("Card.Dragon")};
    return Keys[FMath::Clamp(int32(Card),0,UE_ARRAY_COUNT(Keys)-1)];
}

FLinearColor FCardRoomRun::CardColor(ERoomCard Card)
{
    static const FLinearColor Colors[]={{.1f,.8f,1},{.45f,.8f,.1f},{1,.23f,.06f},{1,.8f,.12f},{.5f,.9f,1},{.2f,.7f,.3f},{.55f,1,.8f},{.12f,.4f,1},{.65f,.4f,.18f},{.65f,.7f,.8f},{1,.95f,.6f},{.55f,.3f,.8f},{1,.5f,.8f},{.6f,.4f,1},{.3f,.8f,.7f},{.65f,.85f,1},{1,.35f,.2f}};
    return Colors[FMath::Clamp(int32(Card),0,UE_ARRAY_COUNT(Colors)-1)];
}
