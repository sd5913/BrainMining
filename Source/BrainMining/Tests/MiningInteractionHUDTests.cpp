#include "MiningInteractionHUDWidget.h"
#include "MiningWalletSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningCoinFeedbackTest, "BrainMining.HUD.CoinFeedback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningCoinFeedbackTest::RunTest(const FString& Parameters)
{
    auto* Instance = NewObject<UGameInstance>();
    auto* Wallet = NewObject<UMiningWalletSubsystem>(Instance);
    FMiningCoinFeedback Feedback;
    int32 Notifications = 0;
    const auto Handle = Wallet->OnCoinsAdded.AddLambda([&](int64 Amount) { ++Notifications; Feedback.Add(Amount); });
    TestFalse(TEXT("No gain appears when attaching to existing balance"), Feedback.IsVisible());
    Wallet->AddCoins(25);
    TestEqual(TEXT("Gain uses credited amount"), Feedback.Amount, int64(25));
    Feedback.Advance(.2f);
    Wallet->AddCoins(10);
    TestEqual(TEXT("Rapid gains combine"), Feedback.Amount, int64(35));
    Wallet->Spend(5);
    Wallet->AddCoins(0);
    Wallet->AddCoins(-20);
    TestEqual(TEXT("Spending and invalid credits emit no gain"), Notifications, 2);
    TestEqual(TEXT("Combined label is not wallet balance"), Feedback.Amount, int64(35));
    Feedback.Advance(.4f);
    Wallet->AddCoins(7);
    TestEqual(TEXT("Separate reward starts its own label"), Feedback.Amount, int64(7));
    Feedback.Advance(1.3f);
    TestFalse(TEXT("Reward expires"), Feedback.IsVisible());
    Wallet->Balance = MAX_int64 - 2;
    Wallet->AddCoins(20);
    TestEqual(TEXT("Saturated wallet reports only actual credit"), Feedback.Amount, int64(2));
    Wallet->AddCoins(20);
    TestEqual(TEXT("Full wallet emits no phantom reward"), Notifications, 4);
    Feedback.Add(MAX_int64);
    TestEqual(TEXT("Accumulation cannot overflow"), Feedback.Amount, MAX_int64);
    Wallet->OnCoinsAdded.Remove(Handle);
    return true;
}
#endif
