#include "CardRoomRun.h"
#include "Misc/AutomationTest.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "CardRoomRobot.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SCardRoomView.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardRoomTouchTest,"CardRoom.Input.TouchFullEncounter",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCardRoomTouchTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
    auto* Controller = World->SpawnActor<APlayerController>();
    auto* Pawn = World->SpawnActor<ACharacter>();
    Controller->Possess(Pawn);
    auto* Cards = NewObject<UCardRoomInteractionComponent>(Controller);
    Cards->Stage = World->SpawnActor<ACardRoomStage>();
    int32 RewardCount = 0;
    Cards->Stage->AudioOverride.BindLambda([&RewardCount](FName Cue,float)
    {
        if(Cue == TEXT("S_Reward")) ++RewardCount;
    });
    const auto View = SNew(SCardRoomView).Owner(Cards);
    Cards->View = View;
    for(const FVector2D Size : {FVector2D(1280,720),FVector2D(2400,1080),FVector2D(800,360),FVector2D(1024,768)})
    {
        const FGeometry Geometry = FGeometry::MakeRoot(Size,FSlateLayoutTransform(1.25f,FVector2D(37,21)));
        const float Scale = FMath::Min(Size.X/1600,Size.Y/900);
        const FVector2D Origin = (Size-FVector2D(1600,900)*Scale)*.5;
        auto Touch = [&](FVector2D Position)
        {
            const FVector2D Screen = Geometry.LocalToAbsolute(Origin+Position*Scale);
            return View->OnTouchStarted(Geometry,FPointerEvent(0,1,Screen,Screen,1.f,true));
        };
        auto& Run = Cards->Stage->Run;
        Cards->EntryLocation = FVector(123,456,789);
        Cards->EntryMovementMode = MOVE_Walking;
        Cards->bEntryCollision = true;
        Cards->SeatedPawn = Pawn;
        Cards->bSeated = true;
        Cards->SetRoomActive(true);
        Controller->SetIgnoreMoveInput(true);
        Controller->SetIgnoreLookInput(true);
        Controller->bShowMouseCursor = true;
        Pawn->SetActorEnableCollision(false);
        Pawn->GetCharacterMovement()->DisableMovement();
        Run = FCardRoomRun();
        Run.Seat(); Run.Match(); Run.Tick(2.3f); Run.Tick(1.5f);
        for(int32 Round = 1; Round <= 4; ++Round)
        {
            TestEqual(TEXT("Touch battle reaches each round"),Run.Round,Round);
            Touch(FVector2D(1322,762));
            TestTrue(TEXT("Empty confirm cannot skip combat"),Run.Phase == ECardRoomPhase::Choosing);
            if(Round == 4)
                TestEqual(TEXT("First tap selects all seventeen cards"),Run.Staged.Num(),17);
            else
            {
                const FVector2D FirstCard(Round == 3 ? 720 : 800,760);
                TestTrue(TEXT("Card view handles real touch input"),Touch(FirstCard).IsEventHandled());
                TestTrue(TEXT("Touch selects the first card"),Run.Staged.Contains(0));
                Touch(FirstCard);
                TestTrue(TEXT("Second touch cancels selection"),Run.Staged.IsEmpty());
                Touch(FirstCard);
                if(Round == 3)
                {
                    Touch(FVector2D(1322,762));
                    TestTrue(TEXT("Combination requires both cards"),Run.Phase == ECardRoomPhase::Choosing);
                    Touch(FVector2D(880,760));
                }
            }
            TestTrue(TEXT("Same card surface handles confirm touch"),Touch(FVector2D(1322,762)).IsEventHandled());
            TestTrue(TEXT("Touch alone plays the selected hand"),Run.Phase == ECardRoomPhase::Resolving);
            Run.Tick(.5f);
            Touch(FVector2D(1322,762));
            TestEqual(TEXT("Repeated confirm does not restart resolution"),Run.PhaseTime,.5f);
            Run.Tick(5.f);
        }
        TestTrue(TEXT("Touch-only battle reaches victory"),Run.Phase == ECardRoomPhase::Victory);
        const int32 BeforeReward = RewardCount;
        Touch(FVector2D(800,500));
        TestTrue(TEXT("Victory requires explicit reward pickup"),Run.Phase == ECardRoomPhase::Victory);
        const FReply Pickup = Touch(FVector2D(1322,762));
        TestTrue(TEXT("Victory action handles reward touch"),Pickup.IsEventHandled());
        TestFalse(TEXT("Pickup does not refocus the dismissed modal"),Pickup.GetUserFocusRecepient().IsValid());
        TestTrue(TEXT("Touch claims reward"),Run.Phase == ECardRoomPhase::Collected);
        TestEqual(TEXT("Reward is granted once"),RewardCount,BeforeReward+1);
        TestFalse(TEXT("Pickup deactivates card input"),Cards->bRoomActive || Cards->bSeated);
        TestTrue(TEXT("Pickup removes the card overlay"),View->GetVisibility() == EVisibility::Collapsed);
        TestFalse(TEXT("Pickup restores camera and movement input"),Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored() || Controller->bShowMouseCursor);
        TestTrue(TEXT("Pickup restores entry position and collision"),Pawn->GetActorLocation().Equals(Cards->EntryLocation) && Pawn->GetActorEnableCollision());
        TestEqual(TEXT("Pickup restores walking"),Pawn->GetCharacterMovement()->MovementMode.GetValue(),MOVE_Walking);
        TestFalse(TEXT("Inactive card view ignores touches"),Touch(FVector2D(1322,762)).IsEventHandled());
        TestEqual(TEXT("Repeated touches cannot grant reward twice"),RewardCount,BeforeReward+1);
    }
    Controller->UnPossess();
    World->DestroyWorld(false);
    return true;
}
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardRoomSingedBrowTest,"CardRoom.Presentation.PermanentSingedBrow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCardRoomSingedBrowTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto* Stage=World->SpawnActor<ACardRoomStage>();
    Stage->bUseRobotOpponent=true;Stage->DispatchBeginPlay();
    auto* Child=Stage->FindComponentByClass<UChildActorComponent>();
    auto* Robot=Child?Cast<ACardRoomRobot>(Child->GetChildActor()):nullptr;
    if(!TestNotNull(TEXT("Encounter creates the supplied robot"),Robot)){World->DestroyWorld(false);return false;}
    auto& Run=Stage->Run;
    Run.Seat();Run.Match();Stage->Tick(2.3f);Stage->Tick(1.5f);
    for(int32 Round=1;Round<=3;++Round)
    {
        for(int32 Index=0;Index<Run.Hand.Num();++Index)Run.Stage(Index);
        TestTrue(TEXT("Real card sequence confirms"),Run.Confirm());
        Stage->Tick(.9f);
        if(Round==3)TestFalse(TEXT("Eyebrow is intact before fire hits"),Robot->bEyebrowSinged);
        Stage->Tick(.2f);
        TestEqual(TEXT("Only the fire reaction breaks the eyebrow"),Robot->bEyebrowSinged,Round==3);
        Stage->Tick(4.f);
    }
    for(const FName Expression:{FName(TEXT("Sick")),FName(TEXT("Attack")),FName(TEXT("Fly"))})
    {
        Robot->GetRootComponent()->SetVisibility(true,true);
        Robot->Pose(Expression,2.f,.1f);
        TArray<UStaticMeshComponent*> Parts;Robot->GetComponents(Parts);
        int32 BrokenMeshes=0;
        for(auto* Part:Parts)
        {
            const FName Name=Part->GetFName();
            if(Name==TEXT("BrowL"))TestFalse(TEXT("Expressions never restore the intact brow"),Cast<UStaticMeshComponent>(Part)->IsVisible());
            if(Name==TEXT("BrowR") || Name==TEXT("SingedBrowInner") || Name==TEXT("SingedBrowOuter"))
                TestTrue(TEXT("Other brow and separated brow pieces remain visible"),Cast<UStaticMeshComponent>(Part)->IsVisible());
            if(Name==TEXT("SingedBrowInner") || Name==TEXT("SingedBrowOuter"))
            {
                ++BrokenMeshes;
                TestNotNull(TEXT("The original brow with a central gap is available"),Part->GetStaticMesh().Get());
                TestTrue(TEXT("Broken eyebrow adds no rotation, offset or scaling"),Part->GetRelativeTransform().Equals(FTransform::Identity));
            }
        }
        TestEqual(TEXT("One untransformed cut mesh replaces the eyebrow"),BrokenMeshes,1);
    }
    Stage->RestartEncounter();
    TestFalse(TEXT("A new encounter restores the brow"),Robot->bEyebrowSinged);
    World->DestroyWorld(false);
    return true;
}
#endif
