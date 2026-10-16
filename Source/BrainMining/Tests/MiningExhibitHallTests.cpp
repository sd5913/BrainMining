#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Showcase/MiningExhibitHall.h"
#include "Showcase/MiningTreasure.h"
#include "MiningCollection.h"
#include "MiningComponent.h"
#include "BrainMiningCharacter.h"
#include "MiningToolEffectsComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "MiningCellUniverse.h"
#include "MiningRoomEscape.h"
#include "MiningWalletSubsystem.h"
#include "MiningProgressionComponent.h"
#include "MiningExperienceSubsystem.h"
#include "GeneratedMiningRock.h"
#include "MiningGUIWidgets.h"
#include "CardRoomStage.h"
#include "PopupClose/PopupCloseStage.h"
#include "PopupClose/PopupOfficeDevice.h"
#include "WenSi/WenSiPuzzle.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

namespace ExhibitHallTests
{
    template<typename ActorType> ActorType* First(UWorld* World)
    {
        for(TActorIterator<ActorType> Actor(World); Actor; ++Actor) return *Actor;
        return nullptr;
    }
    class FStart : public IAutomationLatentCommand
    {
        bool Update() override
        {
            FRequestPlaySessionParams Parameters;
            auto* Settings=DuplicateObject<ULevelEditorPlaySettings>(GetMutableDefault<ULevelEditorPlaySettings>(),GetTransientPackage());
            Settings->NewWindowWidth=1440; Settings->NewWindowHeight=900;
            Parameters.EditorPlaySettings=Settings;
            Parameters.DestinationSlateViewport=nullptr;
            GEditor->RequestPlaySession(Parameters);
            return true;
        }
    };
    class FPresentation : public IAutomationLatentCommand
    {
    public:
        explicit FPresentation(FAutomationTestBase* InTest) : Test(InTest) {}
        bool Update() override
        {
            if(FPlatformTime::Seconds()-Started>150) { Test->AddError(TEXT("Shared character presentation timed out")); return true; }
            UWorld* World=GEditor?GEditor->PlayWorld:nullptr;
            if(!World || !World->HasBegunPlay() || World->GetTimeSeconds()<2.5 || FPlatformTime::Seconds()-Changed<1.2) return false;
            auto* Hall=AMiningExhibitHall::Find(World);
            if(!Hall || !Hall->bReady) return false;
            auto* Controller=Cast<AMiningExhibitController>(World->GetFirstPlayerController());
            auto* Pawn=Controller?Cast<ABrainMiningCharacter>(Controller->GetPawn()):nullptr;
            if(!Test->TestNotNull(TEXT("Shared mining character"),Pawn)) return true;
            auto* Mining=Pawn->FindComponentByClass<UMiningComponent>();
            auto* Effects=Pawn->FindComponentByClass<UMiningToolEffectsComponent>();
            auto* Headlamp=FindObject<USpotLightComponent>(Pawn,TEXT("MiningLight"));
            auto* Fill=FindObject<UPointLightComponent>(Pawn,TEXT("MiningFillLight"));
            if(!Test->TestNotNull(TEXT("Tool effects"),Effects) || !Test->TestNotNull(TEXT("Headlamp"),Headlamp) || !Test->TestNotNull(TEXT("Fill light"),Fill)) return true;
            auto Key=[&](FKey Input,EInputEvent Event){Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Input,Event,Event==IE_Pressed?1:0));};
            auto Shot=[&](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("SharedTools")/(FString(Name)+TEXT(".png")),true,false);};
            auto CheckLighting=[&]()
            {
                Test->TestTrue(TEXT("Headlamp and near-player fill are both restored"),Headlamp->IsVisible() && Fill->IsVisible() && Headlamp->Intensity>0 && Fill->Intensity>0);
                Test->TestFalse(TEXT("Headlamp avoids inverse-square hotspot at close mining range"),Headlamp->bUseInverseSquaredFalloff);
                Test->TestFalse(TEXT("Fill uses bounded falloff"),Fill->bUseInverseSquaredFalloff);
                Test->TestTrue(TEXT("Headlamp concentrates on the central view"),Headlamp->OuterConeAngle>=25 && Headlamp->OuterConeAngle<=35 && Headlamp->GetRelativeLocation().X<=0);
            };
            auto CheckFraming=[&](UStaticMeshComponent* Model,bool bDrill)
            {
                if(!Test->TestNotNull(TEXT("Tool mesh component"),Model) || !Test->TestNotNull(TEXT("Tool mesh asset"),Model->GetStaticMesh().Get())) return;
                Test->TestTrue(TEXT("Equipped tool is visible to its owner"),Model->IsVisible() && !Model->bHiddenInGame && Model->bOnlyOwnerSee);
                Test->TestTrue(TEXT("Tool participates in camera first-person depth scaling"),Model->FirstPersonPrimitiveType==EFirstPersonPrimitiveType::FirstPerson);
                const auto Bounds=Model->GetStaticMesh()->GetBounds();
                const FVector Head=Bounds.Origin+(bDrill?FVector(Bounds.BoxExtent.X,0,0):FVector(0,0,Bounds.BoxExtent.Z*.75));
                auto* Camera=Pawn->GetFirstPersonCameraComponent();
                const FVector View=Camera->GetComponentTransform().InverseTransformPosition(Model->GetComponentTransform().TransformPosition(Head));
                const float FieldOfView=Camera->bEnableFirstPersonFieldOfView?Camera->FirstPersonFieldOfView:Camera->FieldOfView;
                const double HalfWidth=View.X*FMath::Tan(FMath::DegreesToRadians(FieldOfView*.5));
                for(const double Aspect : {4.0/3.0,16.0/10.0,16.0/9.0})
                    Test->TestTrue(*FString::Printf(TEXT("%s head stays inside %.2f aspect viewport"),*Model->GetName(),Aspect),View.X>0 && FMath::Abs(View.Y)<HalfWidth*.95 && FMath::Abs(View.Z)<HalfWidth/Aspect*.95);
            };
            if(Step==0)
            {
                InitialPawn=Pawn;
                if(auto Window=World->GetGameViewport()->GetWindow()) Window->Resize(FVector2D(1440,900));
                Test->TestTrue(TEXT("Hall selects its authored shared character"),Pawn->GetClass()->GetPathName().Contains(TEXT("BP_SharedMiningCharacter")));
                Hall->EnterArea(0);
            }
            else if(Step==1)
            {
                CheckLighting(); CheckFraming(Effects->PickaxeMesh,false); Shot(TEXT("fixed_pickaxe"));
            }
            else if(Step==2)
            {
                Key(EKeys::V,IE_Pressed); Key(EKeys::V,IE_Released);
            }
            else if(Step==3)
            {
                Test->TestTrue(TEXT("V enables drilling"),Mining->bDrillMode);
                CheckFraming(Effects->DrillMesh,true); CheckFraming(Effects->LeftDrillMesh,true);
                Test->TestFalse(TEXT("Pickaxe is stowed while drilling"),Effects->PickaxeMesh->IsVisible());
                Spin=Effects->DrillSpinDegrees;
                Key(EKeys::LeftMouseButton,IE_Pressed);
            }
            else if(Step==4)
            {
                Test->TestTrue(TEXT("Holding left mouse rotates visible drill models"),Effects->bDrillRunning && !FMath::IsNearlyEqual(Spin,Effects->DrillSpinDegrees));
                Shot(TEXT("fixed_drilling")); Key(EKeys::LeftMouseButton,IE_Released);
            }
            else if(Step==5) Hall->EnterArea(2);
            else if(Step==6)
            {
                Test->TestFalse(TEXT("Journey intentionally disables headlamp"),Headlamp->IsVisible());
                Test->TestFalse(TEXT("Journey intentionally disables fill"),Fill->IsVisible());
                Hall->EnterArea(0);
            }
            else if(Step==7)
            {
                CheckLighting();
                if(Mining->bDrillMode) return false;
                CheckFraming(Effects->PickaxeMesh,false);
                Test->TestFalse(TEXT("Right drill stows after use expires"),Effects->DrillMesh->IsVisible());
                Test->TestFalse(TEXT("Left drill stows after use expires"),Effects->LeftDrillMesh->IsVisible());
                Shot(TEXT("fixed_return_from_journey"));
            }
            else if(Step>=8 && Step<=11) Hall->EnterArea(Step==8?1:Step-6);
            else if(Step==12) Hall->ReturnToLobby();
            else if(Step==13)
            {
                CheckLighting(); CheckFraming(Effects->PickaxeMesh,false);
                Test->TestTrue(TEXT("All area visits retain the same character"),InitialPawn.Get()==Pawn);
                return true;
            }
            ++Step; Changed=FPlatformTime::Seconds(); return false;
        }
    private:
        FAutomationTestBase* Test;
        int32 Step=0;
        float Spin=0;
        double Started=FPlatformTime::Seconds(),Changed=0;
        TWeakObjectPtr<APawn> InitialPawn;
    };

    class FVisit : public IAutomationLatentCommand
    {
    public:
        explicit FVisit(FAutomationTestBase* InTest):Test(InTest){}
        bool Update() override
        {
            if(FPlatformTime::Seconds()-Started>300) { Test->AddError(FString::Printf(TEXT("Six-area integration timed out at step %d"),Step)); return true; }
            UWorld* World=GEditor?GEditor->PlayWorld:nullptr;
            if(!World || !World->HasBegunPlay() || World->GetTimeSeconds()<2.5 || FPlatformTime::Seconds()-Changed<1.2) return false;
            auto* Hall=AMiningExhibitHall::Find(World);
            if(!Hall || !Hall->bReady) return false;
            auto* Controller=Cast<AMiningExhibitController>(World->GetFirstPlayerController());
            auto* Pawn=Controller?Cast<ACharacter>(Controller->GetPawn()):nullptr;
            if(!Pawn || !Controller->MiningGUI) return false;
            auto* Mining=Pawn->FindComponentByClass<UMiningComponent>();
            auto* Wallet=World->GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
            auto Key=[&](FKey Input,EInputEvent Event){Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Input,Event,Event==IE_Pressed?1:0));};
            auto Shot=[&](FString Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("SixAreasPreview")/(Name+TEXT(".png")),true,false);};
            auto Aim=[&](FVector Target){Pawn->SetActorLocation(Target+FVector(-230,0,40),false,nullptr,ETeleportType::TeleportPhysics);Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);Controller->SetControlRotation((Target-Pawn->GetPawnViewLocation()).Rotation());};
            if(Step==0)
            {
                InitialWorld=World; InitialPawn=Pawn;
                if(auto Window=World->GetGameViewport()->GetWindow()) Window->Resize(FVector2D(1440,900));
                Test->TestEqual(TEXT("Exactly six areas"),Hall->Areas.Num(),6);
                Test->TestEqual(TEXT("No streamed sublevels"),World->GetStreamingLevels().Num(),0);
                for(TActorIterator<AActor> Actor(World); Actor; ++Actor) Test->TestTrue(TEXT("Every actor belongs to persistent level"),Actor->GetLevel()==World->PersistentLevel);
                Test->TestEqual(TEXT("Lobby at startup"),Hall->ActiveArea,INDEX_NONE);
                Test->TestTrue(TEXT("Latest escape actors preserved"),Hall->Journey && Hall->Journey->CardStage && Hall->Journey->Entrance && Hall->Journey->BreachWall && Hall->Journey->TelevisionScreen);
                Test->TestEqual(TEXT("Ending gate references preserved"),Hall->Ending->Gates.Num(),11);
                for(ADiggableRock* Gate:Hall->Ending->Gates) Test->TestTrue(TEXT("Ending gates reference this world"),IsValid(Gate)&&Gate->GetWorld()==World);
                Test->TestTrue(TEXT("Latest museum circuit preserved"),First<AWenSiPuzzle>(World)->bCircuitPuzzle);
                int32 Devices=0; for(TActorIterator<APopupOfficeDevice> Device(World); Device; ++Device) ++Devices;
                Test->TestEqual(TEXT("Four office devices"),Devices,4);
                Test->TestFalse(TEXT("Insufficient coins reject upgrade"),Hall->PurchaseUpgrade(0,1));
                Shot(TEXT("00_Lobby"));
            }
            else if(Step==1) { Hall->EnterArea(0); }
            else if(Step==2)
            {
                int32 Meshes=0;
                for(TActorIterator<AGeneratedMiningRock> Rock(World); Rock; ++Rock) if(Rock->ActorHasTag(TEXT("BM_Mining")))
                { UE::Geometry::FDynamicMesh3 Mesh; Rock->CopyMiningMesh(Mesh); Test->TestTrue(TEXT("Shen generated geometry survives import"),Mesh.TriangleCount()>0); ++Meshes; }
                Test->TestTrue(TEXT("Shen generated mine present"),Meshes>0);
                Shot(TEXT("01_Mining"));
            }
            else if(Step==3) Hall->EnterArea(1);
            else if(Step==4)
            {
                Test->TestEqual(TEXT("Collection uses I"),Hall->Collection->CatalogueKey,EKeys::I);
                for(AMiningCollectible* Item:Hall->Collection->Items) Test->TestTrue(TEXT("Collection props translated into their area"),FVector::Dist(Item->GetActorLocation(),Hall->Areas[1].Origin)<5000);
                Shot(TEXT("02_Collection"));
                Tutorial=Hall->Collection->Items.Last(); Aim(Tutorial->GetActorLocation());
            }
            else if(Step==5) { Key(EKeys::E,IE_Pressed); }
            else if(Step==6)
            {
                Key(EKeys::E,IE_Released);
                Test->TestTrue(TEXT("E collects tutorial after translation"),Hall->Collection->Inventory[2]>0);
                Key(EKeys::I,IE_Pressed);
                Test->TestTrue(TEXT("I opens catalogue"),Hall->Collection->bCatalogueOpen);
                Shot(TEXT("03_Catalogue"));
            }
            else if(Step==7)
            {
                FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::I,FModifierKeysState(),0,false,0,0));
                Test->TestFalse(TEXT("I closes catalogue"),Hall->Collection->bCatalogueOpen);
                Key(EKeys::Tab,IE_Pressed);
            }
            else if(Step==8)
            {
                Test->TestTrue(TEXT("Tab opens upgrade panel"),Controller->bUpgradeMenuHeld);
                Test->TestFalse(TEXT("Tab does not open catalogue"),Hall->Collection->bCatalogueOpen);
                Shot(TEXT("04_Upgrades")); Key(EKeys::Tab,IE_Released);
                Treasure=Hall->Treasure->Items[0];
                for(AMiningTreasure* Item:Hall->Treasure->Items) if(Item->Value>Treasure->Value) Treasure=Item;
                for(ADiggableRock* Rock:Treasure->CoveringRocks) Rock->MineSphere(Treasure->GetActorLocation(),350);
                Aim(Treasure->GetActorLocation());
            }
            else if(Step==9) { Key(EKeys::E,IE_Pressed); }
            else if(Step==10)
            {
                Key(EKeys::E,IE_Released);
                Test->TestTrue(TEXT("Real exposed mineral collected"),Treasure->bCollected);
                Test->TestEqual(TEXT("Mineral credits shared wallet once"),Wallet->Balance,int64(Treasure->Value));
                Balance=Wallet->Balance;
                Test->TestFalse(TEXT("Duplicate mineral rejected"),Hall->Treasure->CollectTarget(Controller));
                Test->TestEqual(TEXT("Duplicate grants no coins"),Wallet->Balance,Balance);
                Test->TestTrue(TEXT("Affordable upgrade purchased"),Hall->PurchaseUpgrade(0,1));
                Balance-=Controller->GetPawn()->FindComponentByClass<UMiningProgressionComponent>()->Tracks[0].Steps[0].Price; Radius=Mining->Radius;
                Test->TestEqual(TEXT("Upgrade deducts configured price"),Wallet->Balance,Balance);
                Test->TestFalse(TEXT("Duplicate upgrade rejected"),Hall->PurchaseUpgrade(0,1));
                Test->TestFalse(TEXT("Skipping a skill level cannot charge"),Hall->PurchaseUpgrade(1,2));
                Test->TestEqual(TEXT("Invalid purchases do not charge"),Wallet->Balance,Balance);
                Hall->ReturnToLobby();
            }
            else if(Step>=11 && Step<=22)
            {
                const int32 Index=(Step-11)/2;
                if((Step-11)%2==0) Hall->EnterArea(Index);
                else
                {
                    Test->TestTrue(TEXT("Same world and pawn across areas"),World==InitialWorld.Get() && Pawn==InitialPawn.Get());
                    Test->TestEqual(TEXT("Selected area active"),Hall->ActiveArea,Index);
                    Test->TestEqual(TEXT("Coins persist across areas"),Wallet->Balance,Balance);
                    Test->TestEqual(TEXT("Upgrade persists across areas"),Mining->Radius,Radius);
                    Test->TestFalse(TEXT("Area entry leaves no input locks"),Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored() || Controller->IsPaused());
                    Test->TestTrue(TEXT("Pawn is inside selected area"),FVector::Dist(Pawn->GetActorLocation(),Hall->Areas[Index].Origin)<70000);
                    Shot(FString::Printf(TEXT("Area_%d"),Index));
                    if(Index==3)
                    {
                        First<APopupCloseStage>(World)->StartOfficeRound();
                        for(TActorIterator<APopupOfficeDevice> Device(World); Device; ++Device)
                        { Device->AddProblem(); Test->TestTrue(TEXT("Office devices accept work"),Device->Pending()>0); if(Device->IsPrinter()) Test->TestTrue(TEXT("Printer processes paper"),Device->ProcessPaper()); }
                    }
                    if(Index==4) Test->TestTrue(TEXT("Museum validates after translation"),First<AWenSiPuzzle>(World)->ValidateCircuitPuzzle().StartsWith(TEXT("PASS")));
                }
            }
            else if(Step==23)
            {
                Key(EKeys::I,IE_Pressed);
                Test->TestTrue(TEXT("Catalogue opens outside collection zone"),Hall->Collection->bCatalogueOpen);
                Test->TestFalse(TEXT("Catalogue capture and its lights remain visible outside collection zone"),Hall->Collection->IsHidden());
                Shot(TEXT("05_GlobalCatalogue"));
            }
            else if(Step==24)
            {
                Hall->ReturnToLobby();
                Test->TestFalse(TEXT("Returning from catalogue clears input"),Hall->Collection->bCatalogueOpen || Controller->IsMoveInputIgnored() || Controller->IsPaused());
                FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::F4,FModifierKeysState(),0,false,0,0));
                Test->TestFalse(TEXT("F4 no longer opens the removed area menu"),World->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->IsMenuOpen());
                Test->TestFalse(TEXT("F4 preserves gameplay input"),Controller->IsPaused() || Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored() || Controller->bShowMouseCursor);
            }
            else if(Step==25)
            {
                Hall->EnterArea(2);
                Test->TestFalse(TEXT("Direct area travel preserves gameplay input"),Controller->IsPaused() || Controller->IsMoveInputIgnored());
                Hall->Journey->CardStage->OnCollectibleClaimed.Broadcast(Hall->Journey->CardStage->RoomId);
            }
            else if(Step==26)
            {
                Test->TestTrue(TEXT("Card reward starts escape in same world"),Hall->Journey->Phase!=EMiningEscapePhase::Idle);
                FrozenElapsed=Hall->Journey->Elapsed; Hall->EnterArea(3);
            }
            else if(Step==27)
            {
                Test->TestEqual(TEXT("Inactive escape timer is suspended"),Hall->Journey->Elapsed,FrozenElapsed);
                Key(EKeys::F6,IE_Pressed);
                Test->TestEqual(TEXT("F6 returns same pawn to lobby"),Hall->ActiveArea,INDEX_NONE);
                Test->TestTrue(TEXT("World identity preserved through all actions"),World==InitialWorld.Get()&&Pawn==InitialPawn.Get());
                Test->TestEqual(TEXT("All coins retained"),Wallet->Balance,Balance);
                Hall->EnterArea(5);
            }
            else if(Step>=28 && Step<=49)
            {
                const int32 GateIndex=(Step-28)/2;
                auto* Ending=Hall->Ending.Get();
                auto* Gate=Ending->Gates[GateIndex].Get();
                if(!Test->TestNotNull(TEXT("Translated ending boundary"),Gate)) return true;
                if((Step-28)%2==0)
                {
                    Test->TestEqual(TEXT("Ending advances in original order"),Ending->CurrentStage,GateIndex);
                    Test->TestTrue(TEXT("Translated ending accepts actual cuts"),Gate->MineSphere(Gate->GetActorLocation(),180));
                    Pawn->SetActorLocation(Ending->GetActorTransform().TransformPosition(FVector(Ending->GetStageEnd(GateIndex)+120,0,110)),false,nullptr,ETeleportType::TeleportPhysics);
                    Pawn->GetCharacterMovement()->StopMovementImmediately();
                }
                else Test->TestTrue(TEXT("Excavation and passage advance ending"),GateIndex==10?Ending->bComplete:Ending->CurrentStage==GateIndex+1);
            }
            else if(Step==50)
            {
                Test->TestTrue(TEXT("All eleven translated stages complete"),Hall->Ending->bComplete && Hall->Ending->MembraneCuts>0);
                Hall->ReturnToLobby();
                Test->TestTrue(TEXT("Ending completion keeps original pawn"),Pawn==InitialPawn.Get());
                return true;
            }
            ++Step; Changed=FPlatformTime::Seconds(); return false;
        }
    private:
        FAutomationTestBase* Test;
        int32 Step=0;
        double Started=FPlatformTime::Seconds(),Changed=0;
        TWeakObjectPtr<UWorld> InitialWorld;
        TWeakObjectPtr<APawn> InitialPawn;
        TWeakObjectPtr<AMiningCollectible> Tutorial;
        TWeakObjectPtr<AMiningTreasure> Treasure;
        int64 Balance=0;
        float Radius=0,FrozenElapsed=0;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSixAreasIntegrationTest,"BrainMining.SixAreas.PersistentWorldAndEconomy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSixAreasIntegrationTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Showcase/Maps/Lvl_MainExperiences"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ExhibitHallTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ExhibitHallTests::FVisit>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSharedCharacterPresentationTest,"BrainMining.SixAreas.SharedCharacterPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSharedCharacterPresentationTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Showcase/Maps/Lvl_MainExperiences"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ExhibitHallTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ExhibitHallTests::FPresentation>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImportedMineGeometryTest,"BrainMining.SixAreas.ShenMiningGeometry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FImportedMineGeometryTest::RunTest(const FString&)
{
    UWorld* Source=LoadObject<UWorld>(nullptr,TEXT("/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo.Lvl_ExternalMining_Demo"));
    UWorld* Destination=LoadObject<UWorld>(nullptr,TEXT("/Game/Showcase/Maps/Lvl_MainExperiences.Lvl_MainExperiences"));
    if(!TestNotNull(TEXT("Source mine"),Source)||!TestNotNull(TEXT("Destination hall"),Destination)) return false;
    int32 Compared=0;
    TSet<AActor*> Matched;
    for(AActor* Actor:Source->PersistentLevel->Actors)
    {
        auto* Original=Cast<AGeneratedMiningRock>(Actor); if(!Original) continue;
        AGeneratedMiningRock* Copy=nullptr;
        UE::Geometry::FDynamicMesh3 OriginalMesh;
        Original->CopyMiningMesh(OriginalMesh);
        for(AActor* Candidate:Destination->PersistentLevel->Actors)
        {
            auto* Rock=Cast<AGeneratedMiningRock>(Candidate);
            if(!Rock || Matched.Contains(Rock) || !Rock->ActorHasTag(TEXT("BM_Mining")) || !Rock->GetActorLocation().Equals(Original->GetActorLocation()+FVector(-250000,0,0),.1)) continue;
            UE::Geometry::FDynamicMesh3 CopyMesh; Rock->CopyMiningMesh(CopyMesh);
            if(OriginalMesh.IsSameAs(CopyMesh,UE::Geometry::FDynamicMesh3::FSameAsOptions())) { Copy=Rock; Matched.Add(Rock); break; }
        }
        if(!TestNotNull(*(Original->GetActorLabel()+TEXT(" retains exact local-space topology")),Copy)) continue;
        TestTrue(TEXT("Imported mine has only intended translation"),Copy->GetActorLocation().Equals(Original->GetActorLocation()+FVector(-250000,0,0),.1));
        ++Compared;
    }
    TestTrue(TEXT("Compared actual Shen generated rock actors"),Compared>0);
    int32 DestinationCount=0;
    for(AActor* Actor:Destination->PersistentLevel->Actors) if(IsValid(Actor) && Actor->IsA<AGeneratedMiningRock>() && Actor->ActorHasTag(TEXT("BM_Mining"))) ++DestinationCount;
    TestEqual(TEXT("Every imported mine has a unique source counterpart"),Compared,DestinationCount);
    return true;
}
#endif
