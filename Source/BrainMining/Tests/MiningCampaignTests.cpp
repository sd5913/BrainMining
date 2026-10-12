#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Showcase/MiningExhibitHall.h"
#include "MiningCollection.h"
#include "MiningComponent.h"
#include "MiningExternalRegion.h"
#include "GeneratedMiningRock.h"
#include "MiningRoomEscape.h"
#include "MiningCellUniverse.h"
#include "PopupClose/PopupCloseStage.h"
#include "PopupClose/PopupOfficeDevice.h"
#include "MiningBombToolComponent.h"
#include "MiningBombActor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "WenSi/WenSiPuzzle.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

namespace CampaignTests
{
template<typename T> T* First(UWorld* World)
{
    for(TActorIterator<T> It(World);It;++It) return *It;
    return nullptr;
}
class FStart : public IAutomationLatentCommand
{
    bool Update() override
    {
        FRequestPlaySessionParams P;
        auto* Settings=DuplicateObject<ULevelEditorPlaySettings>(GetMutableDefault<ULevelEditorPlaySettings>(),GetTransientPackage());
        Settings->NewWindowWidth=1440; Settings->NewWindowHeight=900;
        P.EditorPlaySettings=Settings;
        GEditor->RequestPlaySession(P);
        return true;
    }
};
class FRun : public IAutomationLatentCommand
{
public:
    explicit FRun(FAutomationTestBase* InTest):Test(InTest) {}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>300) { Test->AddError(FString::Printf(TEXT("Campaign timeout: step %d"),Step)); return true; }
        UWorld* World=GEditor?GEditor->PlayWorld:nullptr;
        auto* Hall=AMiningExhibitHall::Find(World);
        if(!World || !World->HasBegunPlay() || World->GetTimeSeconds()<2.5 || !Hall || !Hall->bReady || FPlatformTime::Seconds()-Changed<.6) return false;
        auto* PC=Cast<AMiningExhibitController>(World->GetFirstPlayerController());
        auto* Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
        if(!Pawn) return false;
        auto* Mining=Pawn->FindComponentByClass<UMiningComponent>();
        auto* Collection=Hall->Collection.Get();
        auto Key=[&](FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,E,E==IE_Pressed?1:0));};
        auto Place=[&](FVector P,FRotator R=FRotator::ZeroRotator)
        {
            Pawn->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
            PC->SetControlRotation(R);PC->PlayerCameraManager->UpdateCamera(.01f);
        };
        auto Aim=[&](AMiningCollectible* Item)
        {
            const FVector Forward=Item->GetActorForwardVector();
            const FVector Eye=Item->GetActorLocation()-Forward*300+FVector(0,0,30);
            FVector OldEye;FRotator OldRotation;PC->GetPlayerViewPoint(OldEye,OldRotation);
            const FVector EyeOffset=OldEye-Pawn->GetActorLocation();
            Place(Eye-EyeOffset,(Item->GetActorLocation()-Eye).Rotation());
        };
        auto Shot=[&](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("CampaignPreview")/(FString(Name)+TEXT(".png")),true,false);};
        auto Exit=[&](int32 I){Place(Hall->Areas[I].Exit+FVector(Hall->bContinuousCampaign?40:0,0,0));};
        auto Excavate=[&](AMiningCollectible* Item)
        {
            if(Collection->bBuriedWithClues)
            {
                const auto* P=Collection->AuthoredPlacements.FindByPredicate([&](const auto& V){return V.ItemId==Item->ItemId;});
                if(!P || !Item->HostRock) { Test->AddError(TEXT("Buried cache lacks host soil"));return; }
                const double Depth=P->ClueTransform.InverseTransformPosition(Item->BuriedTransform.GetLocation()).X;
                for(int X=-50;X<=Depth+120;X+=110) Item->HostRock->MineSphere(P->ClueTransform.TransformPosition(FVector(X,0,-50)),185);
            }
            else for(TActorIterator<ADiggableRock> It(World);It;++It) if(It->GetActorLocation().Equals(Item->GetActorLocation(),1)) It->MineSphere(Item->GetActorLocation(),240);
        };
        if(Step==0)
        {
            InitialPawn=Pawn;
            Test->TestEqual(TEXT("Seven linear sections"),Hall->Areas.Num(),7);
            Test->TestEqual(TEXT("Starts directly in the first mine"),Hall->ActiveArea,0);
            Test->TestEqual(TEXT("Four source rooms loaded seamlessly"),World->GetStreamingLevels().Num(),4);
            for(int32 RoomIndex:{1,3,5,6})
                Test->TestTrue(TEXT("Room holds a loaded source level reference"),Hall->Areas[RoomIndex].SourceLevel && Hall->Areas[RoomIndex].SourceLevel->GetLoadedLevel());
            for(AActor* Room:{static_cast<AActor*>(First<APopupCloseStage>(World)),static_cast<AActor*>(Hall->Journey.Get()),
                             static_cast<AActor*>(First<AWenSiPuzzle>(World)),static_cast<AActor*>(Hall->Ending.Get())})
                Test->TestTrue(TEXT("Gameplay runs from source sublevel, never a stale persistent copy"),Room && Room->GetLevel()!=World->PersistentLevel);
            Test->TestEqual(TEXT("Exactly four collectibles, no tutorial duplicate"),Collection->Items.Num(),4);
            Test->TestFalse(TEXT("Unreached room cannot be selected"),Hall->EnterArea(6));
            int32 Regions=0;
            for(TActorIterator<AMiningExternalRegion> It(World);It;++It)
            {
                ++Regions;
                Test->TestEqual(TEXT("Every mine preserves 20 blockers"),It->GeneratedGuideLayers.Num(),20);
                Test->TestTrue(TEXT("Mine references stay in persistent level"),It->GeneratedRock && It->GeneratedRock->GetLevel()==World->PersistentLevel && It->GeneratedRock->GetOwner()==*It);
            }
            Test->TestEqual(TEXT("Three independent source mine copies"),Regions,3);
            if(!Hall->bContinuousCampaign) Test->TestFalse(TEXT("Entrance does not trigger mine exit"),Hall->CanAdvanceCampaign());
            Test->TestFalse(TEXT("Mining entrance has clear player capsule"),World->OverlapBlockingTestByChannel(Pawn->GetActorLocation(),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius()-2,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-2),FCollisionQueryParams(NAME_None,false,Pawn)));
            Shot(TEXT("01_MineEntrance"));
        }
        else if(Step>=1 && Step<=8)
        {
            const int32 ItemIndex=(Step-1)/4;
            auto* Item=Collection->Items[ItemIndex].Get();
            const int32 Action=(Step-1)%4;
            if(Action==0)
            {
                Aim(Item);
                Collection->CollectTarget();
                Test->TestFalse(TEXT("Covered collectible cannot be collected through rock"),Item->bCollected);
                if(Collection->bBuriedWithClues)
                {
                    Test->TestFalse(TEXT("Buried collectible is not released in advance"),Item->bReleased);
                    Test->TestTrue(TEXT("Collectible is enclosed in actual soil"),Item->MeasureExposure()<.1f);
                    Excavate(Item);
                }
                else
                {
                FHitResult Hit;FVector Eye;FRotator R;PC->GetPlayerViewPoint(Eye,R);
                World->LineTraceSingleByChannel(Hit,Eye,Item->GetActorLocation(),ECC_Visibility,FCollisionQueryParams(NAME_None,true,Pawn));
                auto* Rock=Cast<ADiggableRock>(Hit.GetActor());
                if(!Test->TestNotNull(TEXT("Cache cover lies on actual collection ray"),Rock)) return true;
                Test->TestTrue(TEXT("Real excavation opens cache"),Rock->MineSphere(Item->GetActorLocation(),240));
                }
            }
            else if(Action==1) { Aim(Item);Key(EKeys::E,IE_Pressed); }
            else if(Action==2)
            {
                Key(EKeys::E,IE_Released);
                Test->TestTrue(TEXT("Real E collects excavated cache"),Item->bCollected);
                Collection->CollectTarget();
                Test->TestEqual(TEXT("Collected only once"),Collection->Inventory[Item->ItemId],1);
            }
            else if(ItemIndex==1) { Shot(TEXT("02_CacheExcavated")); Exit(0); }
        }
        else if(Step==9)
        {
            if(Hall->ActiveArea!=1) return false;
            Test->TestEqual(TEXT("Two mine-one collectibles persist into office"),Collection->UniqueCount(),2);
            Test->TestFalse(TEXT("Office cannot advance before completion"),Hall->CanAdvanceCampaign());
            Shot(TEXT("03_Office"));
            Test->TestTrue(TEXT("Office starts on entry without a direct start call"),First<APopupCloseStage>(World)->GetSessionStatus().StartsWith(TEXT("phase=1")));
        }
        else if(Step==10)
        {
            if(Hall->ActiveArea==1)
            {
                auto* Office=First<APopupCloseStage>(World);
                auto* Bomb=Pawn->FindComponentByClass<UMiningBombToolComponent>();
                if(!Office || !Bomb)return false;
                if(OfficeBombAction==0)
                {
                    if(!Office->GetExitStatus().Contains(TEXT("Exit phase=2")))return false;
                    UStaticMeshComponent* Pickup=nullptr;TArray<UStaticMeshComponent*> OfficeMeshes;Office->GetComponents(OfficeMeshes);
                    for(auto Mesh:OfficeMeshes)if(Mesh->GetName()==TEXT("OfficeBombSkillPickup")){Pickup=Mesh;break;}
                    if(!Pickup || !Pickup->GetAttachParent())return false;
                    const FVector PickupTarget=Pickup->GetAttachParent()->GetComponentLocation();
                    FVector Local=Office->GetActorTransform().InverseTransformPosition(PickupTarget);Local.X-=110.f;Local.Z=98;
                    Place(Office->GetActorTransform().TransformPosition(Local));
                    FVector Eye;FRotator R;PC->GetPlayerViewPoint(Eye,R);
                    PC->SetControlRotation((PickupTarget-Eye).Rotation());
                    PC->PlayerCameraManager->UpdateCamera(.01f);Key(EKeys::E,IE_Pressed);
                }
                else if(OfficeBombAction==1)
                {
                    Key(EKeys::E,IE_Released);if(!Test->TestTrue(TEXT("Office pickup unlocks the campaign bomb skill"),Bomb->IsSkillUnlocked()))return true;
                    PC->SetControlRotation(Office->GetActorRotation());Key(EKeys::G,IE_Pressed);
                }
                else if(OfficeBombAction==2){Key(EKeys::G,IE_Released);Key(EKeys::LeftMouseButton,IE_Pressed);}
                else if(OfficeBombAction==3)
                {
                    Key(EKeys::LeftMouseButton,IE_Released);
                    if(!Test->TestTrue(TEXT("Campaign throws a real ability bomb"),Bomb->LastThrownBomb.IsValid()))return true;
                    Bomb->LastThrownBomb->SetActorLocation(Office->GetActorTransform().TransformPosition(FVector(968,0,85)),false,nullptr,ETeleportType::TeleportPhysics);
                    Bomb->LastThrownBomb->Collision->SetPhysicsLinearVelocity(FVector::ZeroVector);
                }
                else
                {
                    if(Office->GetCompletedSessions()==0)return false;
                    if(Hall->bContinuousCampaign)Exit(1);
                    else if(!Office->IsOfficeEscaped())Place(Office->GetActorTransform().TransformPosition(FVector(1135,0,98)));
                    return false;
                }
                ++OfficeBombAction;Changed=FPlatformTime::Seconds();return false;
            }
            if(Hall->bContinuousCampaign && Hall->ActiveArea==1 && Hall->CanAdvanceCampaign()) { Exit(1);return false; }
            // Exercise the office's real timer/eruption completion (no fake success flag).
            if(Hall->ActiveArea!=2) return false;
            Test->TestEqual(TEXT("Office completion enters second mine"),Hall->UnlockedArea,2);
            auto* Item=Collection->Items[2].Get();Aim(Item);
            Excavate(Item);
        }
        else if(Step==11) { Aim(Collection->Items[2]);Key(EKeys::E,IE_Pressed); }
        else if(Step==12)
        {
            Key(EKeys::E,IE_Released);
            Test->TestEqual(TEXT("Third collectible in second mine"),Collection->UniqueCount(),3);
            if(!bCatalogueCaptured)
            {
                Key(EKeys::I,IE_Pressed);
                Test->TestTrue(TEXT("Catalogue opens"),Collection->bCatalogueOpen);
                Shot(TEXT("04_Catalogue"));
                bCatalogueCaptured=true;Changed=FPlatformTime::Seconds();return false;
            }
            Collection->ToggleCatalogue();
            Exit(2);
        }
        else if(Step==13)
        {
            if(Hall->ActiveArea!=3) return false;
            Test->TestFalse(TEXT("Entering echo does not skip card room"),Hall->CanAdvanceCampaign());
            Test->TestFalse(TEXT("Collection E is inactive in encounter"),Collection->IsAreaActive());
            if(!bEchoCaptured)
            {
                Shot(TEXT("05_Echo"));bEchoCaptured=true;Changed=FPlatformTime::Seconds();return false;
            }
            // Integration contract: the encounter's existing end signal advances the campaign.
            // Full card/TV mechanics have their separate real-play automation test.
            Hall->Journey->Phase=EMiningEscapePhase::Escaped;
        }
        else if(Step==14)
        {
            if(Hall->bContinuousCampaign && Hall->ActiveArea==3 && Hall->CanAdvanceCampaign()) { Exit(3);return false; }
            if(Hall->ActiveArea!=4) return false;
            Test->TestFalse(TEXT("Mining restored after encounter"),Mining->IsInputSuspended() || PC->IsMoveInputIgnored());
            auto* Item=Collection->Items[3].Get();Aim(Item);
            Excavate(Item);
        }
        else if(Step==15) { Aim(Collection->Items[3]);Key(EKeys::E,IE_Pressed); }
        else if(Step==16)
        {
            Key(EKeys::E,IE_Released);
            Test->TestEqual(TEXT("All four types collected across 2+1+1 mines"),Collection->UniqueCount(),4);
            Collection->ResetCollection();
            Test->TestEqual(TEXT("Legacy reset cannot erase campaign inventory"),Collection->UniqueCount(),4);
            Exit(4);
        }
        else if(Step==17)
        {
            if(Hall->ActiveArea!=5) return false;
            auto* Museum=First<AWenSiPuzzle>(World);
            Test->TestTrue(TEXT("Translated museum validates both real puzzle solutions"),Museum->ValidateCircuitPuzzle().StartsWith(TEXT("PASS")));
            Shot(TEXT("06_Museum"));
            Museum->bComplete=true; // End-signal routing; solutions were exercised above.
        }
        else if(Step==18)
        {
            if(Hall->bContinuousCampaign && Hall->ActiveArea==5 && Hall->CanAdvanceCampaign()) { Exit(5);return false; }
            if(Hall->ActiveArea!=6) return false;
            Test->TestTrue(TEXT("Same pawn throughout"),InitialPawn.Get()==Pawn);
            Test->TestEqual(TEXT("All collection retained at ending"),Collection->UniqueCount(),4);
            Shot(TEXT("07_Ending"));
        }
        else if(Step>=19 && Step<=40)
        {
            const int32 Gate=(Step-19)/2;
            if((Step-19)%2==0)
            {
                Test->TestEqual(TEXT("Ending stage in order"),Hall->Ending->CurrentStage,Gate);
                auto* Rock=Hall->Ending->Gates[Gate].Get();
                Test->TestTrue(TEXT("Ending actual membrane cut"),Rock->MineSphere(Rock->GetActorLocation(),180));
                Place(Hall->Ending->GetActorTransform().TransformPosition(FVector(Hall->Ending->GetStageEnd(Gate)+120,0,110)));
            }
            else Test->TestTrue(TEXT("Ending advances after excavation and traversal"),Gate==10?Hall->Ending->bComplete:Hall->Ending->CurrentStage==Gate+1);
        }
        else if(Step==41)
        {
            if(!Hall->bCampaignComplete) return false;
            Test->TestTrue(TEXT("Completed campaign can revisit mine one"),Hall->EnterArea(0));
            Key(EKeys::F6,IE_Pressed);
            Test->TestEqual(TEXT("F6 rescues within current section"),Hall->ActiveArea,0);
            Test->TestEqual(TEXT("Rescue keeps all four collectibles"),Collection->UniqueCount(),4);
            Test->TestTrue(TEXT("Completed run still uses original pawn"),InitialPawn.Get()==Pawn);
            return true;
        }
        ++Step;Changed=FPlatformTime::Seconds();return false;
    }
private:
    FAutomationTestBase* Test;
    int32 Step=0,OfficeBombAction=0;
    double Started=FPlatformTime::Seconds(),Changed=0;
    TWeakObjectPtr<APawn> InitialPawn;
    bool bCatalogueCaptured=false,bEchoCaptured=false;
};

class FWalkEntry : public IAutomationLatentCommand
{
public:
    explicit FWalkEntry(FAutomationTestBase* T):Test(T) {}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>120) {Test->AddError(FString::Printf(TEXT("Walk entry timeout: room %d step %d"),Room,Step));return true;}
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;
        auto* H=AMiningExhibitHall::Find(W);
        if(!W || W->GetTimeSeconds()<3 || !H || !H->bReady) return false;
        auto* PC=W->GetFirstPlayerController();auto* P=Cast<ACharacter>(PC->GetPawn());
        auto* Office=First<APopupCloseStage>(W);auto* Museum=First<AWenSiPuzzle>(W);
        const int Previous[]={0,2,4,5};
        if(Room>=4)return true;
        auto Key=[&](EInputEvent E){PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,E,E==IE_Pressed?1:0));};
        auto Place=[&](FVector Position,FRotator Rotation=FRotator::ZeroRotator)
        {P->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);PC->SetControlRotation(Rotation);PC->PlayerCameraManager->UpdateCamera(.01f);};
        if(Step==0)
        {
            const FVector Spawn=H->Areas[0].Entry.GetLocation();
            for(FVector Direction:{FVector(1,0,0),FVector(-1,0,0),FVector(0,1,0),FVector(0,-1,0),FVector(0,0,1),FVector(0,0,-1)})
            {
                FHitResult Hit;W->LineTraceSingleByChannel(Hit,Spawn,Spawn+Direction*1800,ECC_Visibility,FCollisionQueryParams(NAME_None,true,P));
                auto* Soil=Cast<ADiggableRock>(Hit.GetActor());
                if(Test->TestNotNull(FString::Printf(TEXT("Start enclosure is diggable in direction %s (hit %s)"),*Direction.ToString(),*GetNameSafe(Hit.GetActor())),Soil))
                {Test->TestTrue(TEXT("Start enclosure supports real excavation"),Soil->MineSphere(Hit.ImpactPoint-Hit.ImpactNormal*20,100));Soil->ResetRock();}
            }
            for(TActorIterator<AActor> It(W);It;++It)
                Test->TestFalse(TEXT("No legacy static blue mine supports"),It->GetActorLabel().StartsWith(TEXT("Campaign_Bedrock")) || It->GetActorLabel().StartsWith(TEXT("Campaign_Mouth")) || It->GetActorLabel().StartsWith(TEXT("Campaign_Connection")) || It->GetActorLabel().StartsWith(TEXT("Campaign_BankFoundation")));
            Test->TestTrue(TEXT("Office remains idle before arrival"),Office->GetSessionStatus().StartsWith(TEXT("phase=0")));
        }
        else if(Step==1)
        {
            H->UnlockedArea=6;H->EnterArea(Previous[Room]);
            if(Room==3)Museum->bComplete=true; // Puzzle completion is covered by FlowCollectionAndEnding.
            Place(H->Areas[Previous[Room]].Exit+FVector(-280,Room==3?150:0,-4));
        }
        else if(Step==2)
        {
            const double Target=H->Areas[Previous[Room]].Exit.X+300;
            if(P->GetActorLocation().X<Target)
            {P->AddMovementInput(FVector::ForwardVector);return false;}
            P->GetCharacterMovement()->StopMovementImmediately();
            Test->TestEqual(TEXT("Walking through doorway activates the next room"),H->ActiveArea,Previous[Room]+1);
            if(Room==0)Test->TestTrue(TEXT("Walking into office starts its real timer"),Office->bAreaActive && Office->IsActorTickEnabled() && Office->GetSessionStatus().StartsWith(TEXT("phase=1")));
            if(Room==1)Test->TestTrue(TEXT("Echo/card journey tick and gameplay are active"),H->Journey->bAreaActive && H->Journey->IsActorTickEnabled());
            if(Room==2)Test->TestTrue(TEXT("Museum tick and interaction are active"),Museum->bAreaActive && Museum->IsActorTickEnabled());
            if(Room==3)Test->TestTrue(TEXT("Universe tick and excavation progression are active"),H->Ending->bAreaActive && H->Ending->IsActorTickEnabled());
        }
        else if(Step==3)
        {
            if(Now-Changed<1)return false;
            if(Room==1 || Room==3){++Room;Step=1;return false;}
            FVector Aim;
            if(Room==0)
            {
                for(TActorIterator<APopupOfficeDevice> It(W);It;++It)if(It->DeviceIndex==0){Device=*It;break;}
                if(!Test->TestNotNull(TEXT("Office device exists"),Device.Get()))return true;
                Aim=Device->InteractionPoint();
                Place(Device->GetActorLocation()-Device->GetActorForwardVector()*215+FVector(0,0,96));
            }
            else
            {
                for(auto O:Museum->Objects)if(Museum->CanThinkAbout(O)){Object=O;break;}
                if(!Test->TestNotNull(TEXT("Museum has an available exhibit"),Object.Get()))return true;
                Aim=Object->GetActorLocation();
                Place(Aim+FVector(-210,0,65));
                P->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
            }
            FVector Eye;FRotator R;PC->GetPlayerViewPoint(Eye,R);PC->SetControlRotation((Aim-Eye).Rotation());PC->PlayerCameraManager->UpdateCamera(.01f);
        }
        else if(Step==4)
        {
            if(Now-Changed<1)return false;
            if(Room==0)Test->TestTrue(TEXT("Actual sight line reaches computer"),Device->CanInteract(PC));
            else Test->TestTrue(TEXT("Actual sight line reaches museum exhibit"),Museum->Aim!=INDEX_NONE);
            Key(IE_Pressed);
        }
        else if(Step==5)
        {
            if(Now-Changed<1.5)return false;
            Key(IE_Released);
            if(Room==0)
            {
                Test->TestTrue(TEXT("Real E opens computer screen"),Office->IsSessionActive() && PC->GetViewTarget()==Device.Get());
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("CampaignPreview/Office_RealE.png"),true,false);
            }
            else {Test->TestTrue(TEXT("Real E creates museum thought card"),Museum->Thoughts[0].IsValid() || Museum->Thoughts[1].IsValid());++Room;Step=1;return false;}
        }
        else if(Step==6){if(Now-Changed<.5)return false;Key(IE_Pressed);}
        else if(Step==7)
        {
            if(Now-Changed<1.5)return false;Key(IE_Released);
            Test->TestFalse(TEXT("E exits computer and restores movement"),Office->IsSessionActive() || PC->IsMoveInputIgnored());
            ++Room;Step=1;return false;
        }
        ++Step;Changed=Now;return false;
    }
private:
    FAutomationTestBase* Test;int Room=0,Step=0;double Started=FPlatformTime::Seconds(),Changed=0;
    TWeakObjectPtr<APopupOfficeDevice> Device;TWeakObjectPtr<AWenSiObject> Object;
};

class FAssemblyRevision : public IAutomationLatentCommand
{
public:
    explicit FAssemblyRevision(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>120){Test->AddError(TEXT("Terrain/birds/skip test timed out"));return true;}
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* H=AMiningExhibitHall::Find(W);
        if(!W || W->GetTimeSeconds()<3 || !H || !H->bReady || FPlatformTime::Seconds()-Changed<.5)return false;
        auto* C=H->Collection.Get();auto* PC=W->GetFirstPlayerController();auto* P=Cast<ACharacter>(PC->GetPawn());
        const auto& Cache=C->AuthoredPlacements[3];
        if(Step==0)
        {
            H->UnlockedArea=6;H->EnterArea(4);
            for(const auto& A:C->AuthoredPlacements)
                Test->TestTrue(TEXT("Collectible is deeper than seven metres from clue face"),A.ClueTransform.InverseTransformPosition(A.Transform.GetLocation()).X>700);
            for(int X=-50;X<=450;X+=110)Cache.HostRock->MineSphere(Cache.ClueTransform.TransformPosition(FVector(X,0,-50)),185);
            P->SetActorLocation(Cache.ClueTransform.TransformPosition(FVector(-220,0,-124)),false,nullptr,ETeleportType::TeleportPhysics);
            P->GetCharacterMovement()->SetMovementMode(MOVE_Flying);P->GetCharacterMovement()->StopMovementImmediately();PC->SetControlRotation(Cache.ClueTransform.Rotator());
        }
        else if(Step==1)
        {
            Test->TestEqual(TEXT("No flock before a complete excavation path"),C->FlockBursts,0);
            Test->TestFalse(TEXT("Shallow excavation does not release the deeper collectible"),C->Items[3]->bReleased);
            for(int X=450;X<=890;X+=110)Cache.HostRock->MineSphere(Cache.ClueTransform.TransformPosition(FVector(X,0,-50)),185);
        }
        else if(Step==2)
        {
            Test->TestEqual(TEXT("Excavating rotated campaign bird bank triggers one flock"),C->FlockBursts,1);
            for(TActorIterator<AActor> It(W);It;++It)if(It->ActorHasTag(TEXT("CollectionFlyingBird"))){Bird=*It;BirdPosition=It->GetActorLocation();break;}
            Test->TestTrue(TEXT("Visible flying bird actors exist"),Bird.IsValid());
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("CampaignPreview/BirdFlight.png"),true,false);
        }
        else if(Step==3)
        {
            Test->TestTrue(TEXT("Flock animates along excavated flight path"),Bird.IsValid() && !Bird->GetActorLocation().Equals(BirdPosition,5));
            Test->TestEqual(TEXT("Animation leaves resident collectible available"),C->Inventory[3],0);
            const auto Inventory=C->Inventory;
            auto Skip=[&](){PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F7,IE_Pressed,1));PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F7,IE_Released,0));};
            Skip();Test->TestEqual(TEXT("F7 does not skip mining"),H->ActiveArea,4);
            for(int I:{1,3,5})
            {
                H->EnterArea(I);Skip();Test->TestEqual(TEXT("F7 skips current minigame"),H->ActiveArea,I+1);
                Test->TestFalse(TEXT("Skip opens room exit"),H->Areas[I].ProgressGate->GetActorEnableCollision());
                Test->TestFalse(TEXT("Skip restores movement and mining"),PC->IsMoveInputIgnored() || P->FindComponentByClass<UMiningComponent>()->IsInputSuspended());
                Test->TestTrue(TEXT("Skip preserves inventory"),C->Inventory==Inventory);
            }
            Skip();Test->TestEqual(TEXT("F7 does not skip ending"),H->ActiveArea,6);return true;
        }
        ++Step;Changed=FPlatformTime::Seconds();return false;
    }
private:
    FAutomationTestBase* Test;int Step=0;double Started=FPlatformTime::Seconds(),Changed=0;
    TWeakObjectPtr<AActor> Bird;FVector BirdPosition;
};

class FPresentation : public IAutomationLatentCommand
{
public:
    explicit FPresentation(FAutomationTestBase* InTest):Test(InTest) {}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>100) { Test->AddError(TEXT("Campaign presentation timed out"));return true; }
        UWorld* World=GEditor?GEditor->PlayWorld.Get():nullptr;
        auto* Hall=AMiningExhibitHall::Find(World);
        if(!World || World->GetTimeSeconds()<3 || !Hall || !Hall->bReady || FPlatformTime::Seconds()-Changed<3) return false;
        if(Step>=14) return true;
        if(Step>=6)
        {
            const int ItemIndex=(Step-6)/2;
            const auto& Placement=Hall->Collection->AuthoredPlacements[ItemIndex];
            auto* PC=World->GetFirstPlayerController();auto* Pawn=Cast<ACharacter>(PC->GetPawn());
            if(Step%2==0)
            {
                Hall->UnlockedArea=4;Hall->EnterArea(ItemIndex<2?0:(ItemIndex==2?2:4));
                Pawn->SetActorLocation(Placement.ClueTransform.TransformPosition(FVector(-270,0,-124)),false,nullptr,ETeleportType::TeleportPhysics);
                Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
                PC->SetControlRotation((Placement.ClueTransform.TransformPosition(FVector(0,0,ItemIndex==1?-190:-45))-(Pawn->GetActorLocation()+FVector(0,0,64))).Rotation());
            }
            else FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("CampaignPreview")/FString::Printf(TEXT("Clue_%d.png"),ItemIndex),true,false);
            ++Step;Changed=FPlatformTime::Seconds();return false;
        }
        const int32 Mine=Step/2;
        const int32 Area=Mine*2;
        if(Step%2==0)
        {
            Hall->UnlockedArea=4; // Presentation fixture for the three authored entrances.
            Hall->EnterArea(Area);Hall->ReturnToLobby();
        }
        else
        {
            APawn* Pawn=World->GetFirstPlayerController()->GetPawn();
            Test->TestEqual(TEXT("Correct mine is visible"),Hall->ActiveArea,Area);
            Test->TestTrue(TEXT("Landing retains player near its entrance"),FVector::Dist(Pawn->GetActorLocation(),Hall->Areas[Area].Entry.GetLocation())<200);
            Test->TestFalse(TEXT("Spawn does not overlap rock"),World->OverlapBlockingTestByChannel(Pawn->GetActorLocation(),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(30,85),FCollisionQueryParams(NAME_None,false,Pawn)));
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("CampaignPreview")/FString::Printf(TEXT("Final_Mine%d.png"),Mine+1),true,false);
        }
        ++Step;Changed=FPlatformTime::Seconds();
        return false;
    }
private:
    FAutomationTestBase* Test;
    int32 Step=0;
    double Started=FPlatformTime::Seconds(),Changed=0;
};

class FMapCheck : public IAutomationLatentCommand
{
public:
    explicit FMapCheck(FAutomationTestBase* InTest):Test(InTest) {}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>120) { Test->AddError(TEXT("Map test timed out")); return true; }
        UWorld* World=GEditor?GEditor->PlayWorld.Get():nullptr;
        auto* Hall=AMiningExhibitHall::Find(World);
        if(!World || World->GetTimeSeconds()<3 || !Hall || !Hall->bReady || FPlatformTime::Seconds()-Changed<1) return false;
        auto* PC=World->GetFirstPlayerController();
        auto* Mining=PC->GetPawn()->FindComponentByClass<UMiningComponent>();
        const int32 Areas[]={0,2,4,1};
        if(Step>=12) return true;
        const int32 Index=Areas[Step/3];
        if(Step%3==0)
        {
            Hall->UnlockedArea=4;Hall->EnterArea(Index);Hall->ReturnToLobby();
            if(Index!=1)
            {
                if(!Test->TestTrue(TEXT("Mine has authored navigation route"),Hall->Areas[Index].MapRoute.Num()>20)) return true;
                Test->TestTrue(TEXT("Map ends at actual exit"),Hall->Areas[Index].MapRoute.Last().Equals(Hall->Areas[Index].Exit,1));
            }
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::M,IE_Pressed,1));
            Test->TestTrue(TEXT("M opens map"),Hall->IsMapOpen());
            Test->TestTrue(TEXT("Map pauses gameplay"),PC->IsPaused());
            Test->TestTrue(TEXT("Map suspends mining"),Mining->IsInputSuspended());
            PausedTime=World->GetTimeSeconds();
        }
        else if(Step%3==1)
        {
            Test->TestEqual(TEXT("World clock stays still while reading map"),World->GetTimeSeconds(),PausedTime);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("CampaignPreview")/FString::Printf(TEXT("Map_Area%d.png"),Index),true,false);
        }
        else
        {
            const FKey CloseKey=Index==2?EKeys::Escape:EKeys::M;
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(CloseKey,FModifierKeysState(),0,false,0,0));
            Test->TestFalse(TEXT("M or Escape closes focused map"),Hall->IsMapOpen());
            Test->TestFalse(TEXT("Closing map restores world and movement"),PC->IsPaused()||PC->IsMoveInputIgnored()||PC->IsLookInputIgnored());
            Test->TestFalse(TEXT("Closing map restores mining"),Mining->IsInputSuspended());
        }
        ++Step;Changed=FPlatformTime::Seconds();return false;
    }
private:
    FAutomationTestBase* Test;
    int32 Step=0;
    double PausedTime=0;
    double Started=FPlatformTime::Seconds(),Changed=0;
};

class FContinuityCheck : public IAutomationLatentCommand
{
public:
    explicit FContinuityCheck(FAutomationTestBase* InTest):Test(InTest) {}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>120) { Test->AddError(TEXT("Continuity initialization timed out"));return true; }
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* H=AMiningExhibitHall::Find(W);
        if(!W || W->GetTimeSeconds()<3 || !H || !H->bReady) return false;
        Test->TestTrue(TEXT("Campaign uses physical transitions"),H->bContinuousCampaign);
        auto* P=W->GetFirstPlayerController()->GetPawn().Get();
        H->UnlockedArea=6;
        H->EnterArea(1);
        for(float Side:{-1.f,1.f})for(float X:{950.f,1050.f,1150.f,1250.f})
        {
            const FVector Door=H->Areas[1].Origin+FVector(Side*X,0,0);FHitResult FloorHit;
            Test->TestTrue(TEXT("Resized office doors retain continuous floor to campaign tunnels"),
                W->LineTraceSingleByChannel(FloorHit,Door+FVector(0,0,80),Door-FVector(0,0,80),ECC_Visibility,FCollisionQueryParams(NAME_None,false,P)));
        }
        const FCollisionShape Capsule=FCollisionShape::MakeCapsule(30,84);
        for(int I=0;I<6;++I)
        {
            const auto& A=H->Areas[I];H->EnterArea(I);
            // Walk beside the museum reward cabinet toward its doorway.
            FVector Before=A.Exit+FVector(-400,I==5?150:0,-4),After=A.Exit+FVector(750,0,-4);FHitResult Hit;
            if(A.ProgressGate)
            {
                Test->TestTrue(TEXT("Incomplete room has a physical exit gate"),A.ProgressGate->GetActorEnableCollision());
                A.ProgressGate->SetActorEnableCollision(false);
            }
            const bool Blocked=W->SweepSingleByChannel(Hit,Before,After,FQuat::Identity,ECC_Pawn,Capsule,FCollisionQueryParams(NAME_None,false,P));
            Test->TestFalse(FString::Printf(TEXT("Seam %d has capsule clearance (hit %s / %s at %s)"),I,Hit.GetActor()?*Hit.GetActor()->GetActorLabel():TEXT("none"),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString()),Blocked);
            if(A.ProgressGate) A.ProgressGate->SetActorEnableCollision(true);
            P->SetActorLocation(After,false,nullptr,ETeleportType::TeleportPhysics);
            const FVector Saved=P->GetActorLocation();
            H->EnterArea(I+1,false);
            Test->TestTrue(TEXT("Walking activation never moves player"),P->GetActorLocation().Equals(Saved,.01));
        }
        int Regions=0,Clues=0;
        for(TActorIterator<AMiningExternalRegion> It(W);It;++It)
        {++Regions;Test->TestTrue(TEXT("Source mining geometry scaled to 50 percent"),It->GetActorScale3D().Equals(FVector(.5),.001));}
        for(TActorIterator<AActor> It(W);It;++It)if(It->ActorHasTag(TEXT("CampaignExcavationClue")))++Clues;
        Test->TestEqual(TEXT("Three compact mine excerpts"),Regions,3);
        Test->TestTrue(TEXT("Physical clue props are present"),Clues>50);
        Test->TestTrue(TEXT("Ant clue is animated"),H->Collection->AmbientAntCount>=12);
        int SupportedAnts=0;
        for(TActorIterator<AActor> It(W);It;++It)if(It->ActorHasTag(TEXT("CampaignWallAnt")) && !It->IsHidden())
        {
            auto* Surface=Cast<ADiggableRock>(It->GetOwner());FHitResult Hit;
            const FVector Position=It->GetActorLocation(),Up=It->GetActorUpVector();
            const bool Grounded=Surface && Surface->ActorLineTraceSingle(Hit,Position+Up*10,Position-Up*10,ECC_Visibility,FCollisionQueryParams(NAME_None,true)) && FVector::Dist(Position,Hit.ImpactPoint)<5;
            Test->TestTrue(TEXT("Ants follow the real uneven wall surface"),Grounded);SupportedAnts+=Grounded;
        }
        Test->TestTrue(TEXT("Wall ant clue remains visible"),SupportedAnts>=8);
        for(int AreaIndex:{0,2,4})
        {
            bool CutSource=false;
            const auto& Area=H->Areas[AreaIndex];
            for(const FVector& RoutePoint:Area.MapRoute)
            {
                FHitResult Hit;
                if(!W->LineTraceSingleByChannel(Hit,RoutePoint,RoutePoint+FVector(0,450,0),ECC_Visibility,FCollisionQueryParams(NAME_None,true,P))) continue;
                auto* Rock=Cast<AGeneratedMiningRock>(Hit.GetActor());
                if(!Rock || !Rock->ActorHasTag(Area.Tag)) continue;
                CutSource=Rock->MineSphere(Hit.ImpactPoint-Hit.ImpactNormal*30,110);
                break;
            }
            Test->TestTrue(FString::Printf(TEXT("Scaled source terrain %d supports real excavation"),AreaIndex),CutSource);
        }
        for(AMiningCollectible* Item:H->Collection->Items)
        {
            Test->TestNotNull(TEXT("Every collectible has real host soil"),Item->HostRock.Get());
            Test->TestFalse(TEXT("Every collectible starts buried"),Item->bReleased);
            Test->TestTrue(TEXT("Every model starts inside soil volume"),Item->MeasureExposure()<.1);
        }
        H->EnterArea(0);
        auto& Placements=H->Collection->AuthoredPlacements;
        auto* Current=Placements[0].HostRock.Get();auto* Other=Placements[2].HostRock.Get();
        Test->TestTrue(TEXT("Current buried bank is excavatable"),Current->MineSphere(Placements[0].ClueTransform.GetLocation(),100));
        Test->TestTrue(TEXT("Other buried bank is excavatable"),Other->MineSphere(Placements[2].ClueTransform.GetLocation(),100));
        const int OtherCuts=Other->SuccessfulCuts;
        P->FindComponentByClass<UMiningComponent>()->ResetField();
        Test->TestEqual(TEXT("Reset restores current mine"),Current->SuccessfulCuts,0);
        Test->TestEqual(TEXT("Reset preserves other mine excavation"),Other->SuccessfulCuts,OtherCuts);
        return true;
    }
private:
    FAutomationTestBase* Test;double Started=FPlatformTime::Seconds();
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampaignContinuityTest,"BrainMining.Campaign.ContinuityAndClues",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCampaignContinuityTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Campaign/Maps/Lvl_FullJourney"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CampaignTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CampaignTests::FContinuityCheck>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampaignWalkEntryTest,"BrainMining.Campaign.WalkEntryAndInteraction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCampaignWalkEntryTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Campaign/Maps/Lvl_FullJourney"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CampaignTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CampaignTests::FWalkEntry>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampaignAssemblyTest,"BrainMining.Campaign.BirdsDepthAndSkip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCampaignAssemblyTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Campaign/Maps/Lvl_FullJourney"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CampaignTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CampaignTests::FAssemblyRevision>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampaignMapTest,"BrainMining.Campaign.NavigationMap",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCampaignMapTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Campaign/Maps/Lvl_FullJourney"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CampaignTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CampaignTests::FMapCheck>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampaignFlowTest,"BrainMining.Campaign.FlowCollectionAndEnding",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCampaignFlowTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Campaign/Maps/Lvl_FullJourney"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CampaignTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CampaignTests::FRun>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampaignPresentationTest,"BrainMining.Campaign.MiningEntrancePresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCampaignPresentationTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Campaign/Maps/Lvl_FullJourney"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CampaignTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CampaignTests::FPresentation>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampaignMineCopiesTest,"BrainMining.Campaign.SourceGeometry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCampaignMineCopiesTest::RunTest(const FString&)
{
    auto* Source=LoadObject<UWorld>(nullptr,TEXT("/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo.Lvl_ExternalMining_Demo"));
    auto* Destination=LoadObject<UWorld>(nullptr,TEXT("/Game/Campaign/Maps/Lvl_FullJourney.Lvl_FullJourney"));
    if(!TestNotNull(TEXT("Source"),Source) || !TestNotNull(TEXT("Campaign"),Destination)) return false;
    for(AActor* A:Destination->PersistentLevel->Actors) if(auto* H=Cast<AMiningExhibitHall>(A);H && H->bContinuousCampaign)
    {
        int Count=0;
        for(AActor* B:Destination->PersistentLevel->Actors) if(auto* Region=Cast<AMiningExternalRegion>(B))
        {
            ++Count;TestTrue(TEXT("Compact region keeps source cutter reference"),!Region->Cutters.IsEmpty() && Region->Cutters[0].Mesh!=nullptr);
            TestTrue(TEXT("Mine scale is one half"),Region->GetActorScale3D().Equals(FVector(.5),.001));
            TestEqual(TEXT("Original guide layer references retained"),Region->GeneratedGuideLayers.Num(),20);
        }
        TestEqual(TEXT("Three adapted source regions"),Count,3);return true;
    }
    const FVector Offsets[]={FVector(-250000,0,0),FVector(-150000,0,0),FVector(150000,0,0)};
    for(const FVector Offset:Offsets)
    {
        TSet<AActor*> Matched;
        int32 Count=0;
        for(AActor* Actor:Source->PersistentLevel->Actors)
        {
            auto* Original=Cast<AGeneratedMiningRock>(Actor);if(!Original) continue;
            UE::Geometry::FDynamicMesh3 OriginalMesh;Original->CopyMiningMesh(OriginalMesh);
            AGeneratedMiningRock* Copy=nullptr;
            for(AActor* Candidate:Destination->PersistentLevel->Actors)
            {
                auto* Rock=Cast<AGeneratedMiningRock>(Candidate);
                if(!Rock || Matched.Contains(Rock) || !Rock->GetActorLocation().Equals(Original->GetActorLocation()+Offset,.1)) continue;
                UE::Geometry::FDynamicMesh3 Mesh;Rock->CopyMiningMesh(Mesh);
                if(OriginalMesh.IsSameAs(Mesh,UE::Geometry::FDynamicMesh3::FSameAsOptions())) { Copy=Rock;Matched.Add(Rock);break; }
            }
            TestNotNull(TEXT("Copied mine retains exact source topology"),Copy);
            ++Count;
        }
        TestEqual(TEXT("Each copy contains base rock and twenty blockers"),Count,21);
    }
    return true;
}
#endif
