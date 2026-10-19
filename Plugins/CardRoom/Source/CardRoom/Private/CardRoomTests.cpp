#include "CardRoomRun.h"
#include "Misc/AutomationTest.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "SCardRoomView.h"
#include "Engine/World.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardRoomMouseTest,"CardRoom.Input.MouseAndSpace",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCardRoomMouseTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto* Cards=NewObject<UCardRoomInteractionComponent>();
    Cards->Stage=World->SpawnActor<ACardRoomStage>();
    auto& Run=Cards->Stage->Run;
    Run.Phase=ECardRoomPhase::Choosing;Run.Round=1;Run.Hand={ERoomCard::Laser};
    const auto View=SNew(SCardRoomView).Owner(Cards);
    const FGeometry Geometry=FGeometry::MakeRoot(FVector2D(1600,900),FSlateLayoutTransform());
    auto Click=[&](FVector2D Position)
    {
        View->OnMouseButtonDown(Geometry,FPointerEvent(0,Position,Position,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()));
    };
    Click(FVector2D(800,760));TestTrue(TEXT("Mouse click selects a card"),Run.Staged.Contains(0));
    Click(FVector2D(800,760));TestTrue(TEXT("Second mouse click deselects"),Run.Staged.IsEmpty());
    View->OnKeyDown(Geometry,FKeyEvent(EKeys::Right,FModifierKeysState(),0,false,0,0));
    TestTrue(TEXT("Arrow keys do not select cards"),Run.Staged.IsEmpty());
    Click(FVector2D(800,760));
    View->OnKeyDown(Geometry,FKeyEvent(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0));
    TestTrue(TEXT("Space plays the mouse-selected card"),Run.Phase==ECardRoomPhase::Resolving);
    Run.Phase=ECardRoomPhase::Choosing;Run.Round=4;Run.Staged.Reset();Run.Hand.Init(ERoomCard::Laser,17);
    Click(FVector2D(1100,760));
    TestTrue(TEXT("Overlapping cards select the visually topmost card"),Run.Staged.Num()==1 && Run.Staged.Contains(16));
    View->OnKeyDown(Geometry,FKeyEvent(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0));
    TestTrue(TEXT("First Space selects all seventeen"),Run.Phase==ECardRoomPhase::Choosing && Run.Staged.Num()==17);
    View->OnKeyDown(Geometry,FKeyEvent(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0));
    TestTrue(TEXT("Second Space plays the full hand"),Run.Phase==ECardRoomPhase::Resolving);
    Run.Phase=ECardRoomPhase::Choosing;Run.Round=3;Run.Hand={ERoomCard::Fire,ERoomCard::Laser};Run.Staged.Reset();
    Cards->bGamepadInput=true;
    TestEqual(TEXT("New round focus starts at first card"),Cards->GetFocusedCardIndex(),0);
    Cards->NavigateCard(-1);
    TestEqual(TEXT("Focus wraps to last card"),Cards->GetFocusedCardIndex(),1);
    Click(FVector2D(720,760));
    TestFalse(TEXT("Mouse selection returns to pointer presentation"),Cards->bGamepadInput);
    TestTrue(TEXT("Mouse selects the first card after controller navigation"),Run.Staged.Contains(0));
    TestEqual(TEXT("Mouse selection updates focus for next controller input"),Cards->GetFocusedCardIndex(),0);
    Cards->SelectAllCards();TestEqual(TEXT("All-select fills combination hand"),Run.Staged.Num(),2);
    Cards->ClearCardSelection();TestTrue(TEXT("Clear selection preserves choosing phase"),Run.Staged.IsEmpty() && Run.Phase==ECardRoomPhase::Choosing);
    Cards->NavigateCard(1);
    Run.Round=4;Run.Hand.Init(ERoomCard::Laser,17);
    TestEqual(TEXT("Round transition resets focus"),Cards->GetFocusedCardIndex(),0);
    Cards->NavigateCard(-1);Cards->ToggleCardSelection(Cards->GetFocusedCardIndex());
    TestTrue(TEXT("Seventeenth card can be selected without mouse targeting"),Run.Staged.Contains(16));
    Cards->SelectAllCards();Cards->ConfirmCards();
    Cards->ClearCardSelection();Cards->SelectAllCards();Cards->NavigateCard(1);
    TestTrue(TEXT("Resolution locks selection actions"),Run.Phase==ECardRoomPhase::Resolving && Run.Staged.Num()==17);
    TestEqual(TEXT("No focus outside selection phase"),Cards->GetFocusedCardIndex(),INDEX_NONE);
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardRoomFlowTest,"CardRoom.Rules.FullEncounter",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCardRoomFlowTest::RunTest(const FString& Parameters)
{
    FCardRoomRun R;
    TestFalse(TEXT("Cannot match before seating"),R.Match());
    TestFalse(TEXT("Cannot claim a reward early"),R.Collect());
    TestTrue(TEXT("Can take a seat"),R.Seat()); TestFalse(TEXT("Cannot seat twice"),R.Seat());
    TestTrue(TEXT("Can match"),R.Match()); R.Tick(2.3f); TestTrue(TEXT("Arrival follows matching"),R.Phase==ECardRoomPhase::Arrival);
    R.Tick(1.5f); TestEqual(TEXT("First round starts"),R.Round,1);
    for(int32 Turn=1;Turn<=4;++Turn)
    {
        const int32 Expected=Turn==3?2:Turn==4?17:1;
        TestEqual(TEXT("Correct hand size"),R.Hand.Num(),Expected);
        TestFalse(TEXT("Empty play rejected"),R.Confirm());
        TestFalse(TEXT("Invalid card index rejected"),R.Stage(99));
        if(Turn==3) { TestTrue(TEXT("Poison persists into combo turn"),R.bPoisoned); R.Stage(0); TestFalse(TEXT("Single card cannot trigger combination"),R.Confirm()); }
        for(int32 i=0;i<R.Hand.Num();++i) R.Stage(i);
        TestFalse(TEXT("Same card cannot be staged twice"),R.Stage(0));
        TestTrue(TEXT("Can return staged card"),R.Unstage(0)); TestFalse(TEXT("Partial hand rejected"),R.CanConfirm()); R.Stage(0);
        TestTrue(TEXT("Complete hand accepted"),R.Confirm()); TestFalse(TEXT("Duplicate confirm blocked"),R.Confirm());
        TestFalse(TEXT("Input locked during resolution"),R.Stage(0));
        R.Tick(1.1f);
        if(Turn==2) TestTrue(TEXT("Poison applied"),R.bPoisoned);
        if(Turn==3) TestFalse(TEXT("Fire consumes poison"),R.bPoisoned);
        R.Tick(1.3f);
        if(Turn==1) TestEqual(TEXT("Fireball deals 20"),R.PlayerHP,80);
        if(Turn==2) TestEqual(TEXT("Stunned enemy does not damage player"),R.PlayerHP,80);
        if(Turn==3) TestEqual(TEXT("Counterattack leaves one HP"),R.PlayerHP,1);
        R.Tick(3.f);
    }
    TestTrue(TEXT("Victory after fourth round"),R.Phase==ECardRoomPhase::Victory);
    TestEqual(TEXT("Rival defeated"),R.RivalHP,0); TestTrue(TEXT("Collectible claim works"),R.Collect()); TestFalse(TEXT("Collectible cannot be claimed twice"),R.Collect());
    R.Restart(); TestEqual(TEXT("Replay resets HP"),R.PlayerHP,100); TestEqual(TEXT("Replay resets round"),R.Round,0); TestTrue(TEXT("Replay returns to matching choice"),R.Phase==ECardRoomPhase::Seated);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardRoomTimingTest,"CardRoom.Rules.FrameRatesAndEarlyInput",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCardRoomTimingTest::RunTest(const FString& Parameters)
{
    for(const int32 FPS:{15,30,60,120})
    {
        FCardRoomRun R; R.Seat(); R.Match();
        for(int32 i=0;i<FPS*5;++i) R.Tick(1.f/FPS);
        for(int32 Round=1;Round<=4;++Round)
        {
            for(int32 i=0;i<R.Hand.Num();++i) R.Stage(i);
            TestTrue(TEXT("Turn ready"),R.Confirm());
            for(int32 i=0;i<FPS*5;++i) R.Tick(1.f/FPS);
        }
        TestTrue(TEXT("Victory independent of frame rate"),R.Phase==ECardRoomPhase::Victory); TestEqual(TEXT("Stable final HP"),R.PlayerHP,1);
    }
    return true;
}
#endif
