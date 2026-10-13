#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Showcase/MiningTreasure.h"
#include "Showcase/MiningExhibitHall.h"
#include "MiningWalletSubsystem.h"
#include "DiggableRock.h"
#include "GeneratedMiningRock.h"
#include "HAL/PlatformProcess.h"
#include "MiningCharacterMovementComponent.h"
#include "EchoPassagePlayer.h"
#include "MiningToolEffectsComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"

namespace CampaignMineralTests
{
void DrainCut(ADiggableRock* Rock,FAutomationTestBase* Test)
{
    if(auto* Generated=Cast<AGeneratedMiningRock>(Rock))
    {
        const double Deadline=FPlatformTime::Seconds()+3;
        while(Generated->bCutPending && FPlatformTime::Seconds()<Deadline)
        {FPlatformProcess::Sleep(.001f);Generated->Tick(.01f);}
        Test->TestFalse(TEXT("Accepted campaign cut completes"),Generated->bCutPending);
    }
}
class FStart : public IAutomationLatentCommand
{
    bool Update() override
    {
        FRequestPlaySessionParams P;
        auto* Settings=DuplicateObject<ULevelEditorPlaySettings>(GetMutableDefault<ULevelEditorPlaySettings>(),GetTransientPackage());
        Settings->NewWindowWidth=1440;Settings->NewWindowHeight=900;P.EditorPlaySettings=Settings;
        GEditor->RequestPlaySession(P);return true;
    }
};
class FRun : public IAutomationLatentCommand
{
public:
    explicit FRun(FAutomationTestBase* InTest):Test(InTest){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>180){Test->AddError(TEXT("Campaign minerals timed out"));return true;}
        auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* H=AMiningExhibitHall::Find(W);
        if(!H || !H->bReady || W->GetTimeSeconds()<3 || FPlatformTime::Seconds()-Changed<.8)return false;
        auto* Field=H->Treasure.Get();if(!Test->TestNotNull(TEXT("Campaign ore field exists"),Field))return true;
        auto* PC=Cast<AMiningExhibitController>(W->GetFirstPlayerController());auto* Pawn=Cast<ACharacter>(PC->GetPawn());
        auto* Wallet=W->GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
        if(Step==0)
        {
            for(const auto& Placement:Field->AuthoredPlacements) Field->PrepareAuthoredArea(Placement.AreaTag);
            Test->TestEqual(TEXT("Every authored mineral spawns"),Field->Items.Num(),Field->AuthoredPlacements.Num());
            if(Field->Items.Num()!=Field->AuthoredPlacements.Num() || Field->Items.IsEmpty())return true;
            int64 Totals[3]={};int White[3]={},Guide[3]={};
            for(int I=0;I<Field->Items.Num();++I)
            {
                auto* Item=Field->Items[I].Get();const auto& P=Field->AuthoredPlacements[I];
                const int Zone=P.AreaTag==TEXT("BM_Campaign_Mine01")?0:P.AreaTag==TEXT("BM_Campaign_Mine02")?1:2;
                const int Id=FCString::Atoi(*Item->TreasureId.ToString());
                Test->TestTrue(TEXT("Only the three assigned types occur in each mine"),Id>=Zone*3+1 && Id<=Zone*3+3);
                const auto& D=Field->Definitions[Id-1];
                Test->TestTrue(TEXT("Only one of the four discrete values is used"),D.ValuesBySize.Contains(Item->Value));
                Test->TestTrue(FString::Printf(TEXT("Real chunked host initially buries gem %d (%s)"),I,*Item->TreasureId.ToString()),Item->MeasureExposure()<.01f);
                Test->TestTrue(TEXT("Each deposit has a scatter of crystal hints"),Item->CrystalClues->GetInstanceCount()>=15);
                Test->TestTrue(TEXT("Displayed size follows the value"),Item->Model->GetComponentScale().Equals(D.Scale*AMiningTreasure::ValueScale(D,Item->Value),.001));
                Totals[Zone]+=Item->Value;(P.bGuideLayer?Guide[Zone]:White[Zone])++;
            }
            for(int Zone=0;Zone<3;++Zone)
            {
                Test->TestTrue(TEXT("White rock contains ore"),White[Zone]>0);Test->TestTrue(TEXT("Guide layer contains ore"),Guide[Zone]>0);
                Test->TestTrue(TEXT("Deposits fund intended upgrades without minigame rewards"),Totals[Zone]>=(Zone==0?300:Zone==1?25000:8420000));
                for(bool GuideLayer:{false,true})for(int I=0;I<Field->Items.Num();++I)
                    if(Field->AuthoredPlacements[I].AreaTag==FName(*FString::Printf(TEXT("BM_Campaign_Mine%02d"),Zone+1)) && Field->AuthoredPlacements[I].bGuideLayer==GuideLayer)
                    {Samples.Add(I);break;}
            }
            for(const auto& D:Field->Definitions)for(int I=1;I<D.ValuesBySize.Num();++I)
                Test->TestTrue(TEXT("Higher value always means a larger same-type gem"),AMiningTreasure::ValueScale(D,D.ValuesBySize[I])>AMiningTreasure::ValueScale(D,D.ValuesBySize[I-1]));
            if(Samples.Num()!=6){Test->AddError(TEXT("Missing white/guide collection samples"));return true;}
            const auto* Mesh=Field->CrystalClueMesh.Get();
            Test->TestTrue(TEXT("Crystal clues use broad bevelled facets"),Mesh && Mesh->GetRenderData()->LODResources[0].GetNumTriangles()==38);
            H->UnlockedArea=6;
            auto* Movement=Pawn->FindComponentByClass<UMiningCharacterMovementComponent>();
            const auto Flight=Movement->FlightSettings;
            Movement->JetpackHeat=.25f;
            H->EnterArea(3);
            Test->TestTrue(TEXT("Echo/card room preserves shared flight energy limits"),Movement->FlightSettings.bEnableHeat && Movement->FlightSettings.HeatUpSeconds==Flight.HeatUpSeconds);
            Test->TestEqual(TEXT("Entering the room does not refill flight energy"),Movement->JetpackHeat,.25f);
            Movement->UpdateJetpackHeat(Flight.HeatUpSeconds*.75f,true);
            Test->TestTrue(TEXT("Room flight runs out and locks until cooling"),Movement->IsJetpackOverheated());
            Test->TestTrue(TEXT("Echo uses the shared pickaxe"),PC->EchoInteraction->bUseSharedPickaxe);
            auto* Effects=Pawn->FindComponentByClass<UMiningToolEffectsComponent>();
            auto* OriginalMaterial=Effects->PickaxeMesh->GetMaterial(0);
            Effects->UpdateEchoPickaxeStyle();
            Test->TestTrue(TEXT("Shared pickaxe uses black and white lines in the echo passage"),Effects->bEchoPickaxeLines && Effects->PickaxeMesh->GetOverlayMaterial());
            PC->EchoInteraction->SetExplorationActive(false);Effects->UpdateEchoPickaxeStyle();
            Test->TestTrue(TEXT("Doorway restores the original pickaxe material"),!Effects->bEchoPickaxeLines && Effects->PickaxeMesh->GetMaterial(0)==OriginalMaterial);
            PC->EchoInteraction->SetExplorationActive(true);Effects->UpdateEchoPickaxeStyle();
            const int Swings=Effects->SwingCount;
            PC->EchoInteraction->OnKnock.Broadcast();
            Test->TestTrue(TEXT("Echo knock animates the common tool"),Effects->SwingCount>Swings);
            TArray<UStaticMeshComponent*> Models;Pawn->GetComponents(Models);
            for(auto* Model:Models) if(Model->GetMaterial(0))
                Test->TestFalse(TEXT("No room-specific duplicate pickaxe parts"),Model->GetMaterial(0)->GetPathName().Contains(TEXT("M_PickWood")) || Model->GetMaterial(0)->GetPathName().Contains(TEXT("M_PickIron")));
            H->EnterArea(0);
            Effects->UpdateEchoPickaxeStyle();Test->TestFalse(TEXT("Leaving the passage restores normal rendering"),Effects->bEchoPickaxeLines);
            Movement->UpdateJetpackHeat(Flight.OverheatLockSeconds,false);
            auto* Lamp=FindObject<USpotLightComponent>(Pawn,TEXT("MiningLight"));
            Test->TestTrue(TEXT("Soft headlamp has an optical pattern"),Lamp && Lamp->IsVisible() && Lamp->LightFunctionMaterial && !Lamp->CastShadows);

        }
        else
        {
            const int Sample=(Step-1)/6,Phase=(Step-1)%6;
            if(Sample>=Samples.Num())return true;
            const int I=Samples[Sample];auto* Item=Field->Items[I].Get();const auto& P=Field->AuthoredPlacements[I];
            const FVector Normal=P.Surface.GetRotation().GetForwardVector();
            auto Aim=[&]()
            {
                PC->PlayerCameraManager->UpdateCamera(.02f);
                FVector Eye;FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);
                PC->SetControlRotation((Item->GetActorLocation()-Eye).Rotation());PC->PlayerCameraManager->UpdateCamera(.02f);
            };
            if(Phase==0)
            {
                H->EnterArea(1);Test->TestFalse(TEXT("Minigame room disables mineral interaction"),Field->IsAreaActive());
                H->EnterArea((Sample/2)*2);Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
                Pawn->SetActorLocation(P.Surface.GetLocation()+Normal*210-FVector(0,0,64),false,nullptr,ETeleportType::TeleportPhysics);Aim();
                Test->TestFalse(TEXT("Buried gem cannot be collected"),Field->CollectTarget(PC));
                Balance=Wallet->Balance;Fragments=Item->CrystalClues->GetInstanceCount();
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("CampaignMineralPreview/%d_Buried.png"),Sample),true,false);
            }
            else if(Phase==1)
            {
                const float Depth=FVector::Dist(Item->GetActorLocation(),P.Surface.GetLocation());
                bool Cut=false;
                for(float D=0;D<=Depth+80;D+=65)for(ADiggableRock* Rock:Item->CoveringRocks)
                {
                    const int32 Before=Item->CrystalClues->GetInstanceCount();
                    Cut|=Rock->MineSphere(P.Surface.GetLocation()-Normal*D,100);
                    DrainCut(Rock,Test);
                    TArray<bool> Support;AMiningTreasure::MarkCoveredPoints(Item->CoveringRocks,P.FragmentSupports,Support);
                    int32 Expected=0;for(bool Solid:Support)Expected+=Solid;
                    Test->TestEqual(TEXT("Real campaign cut synchronizes clues before any field tick"),Item->CrystalClues->GetInstanceCount(),Expected);
                    Test->TestTrue(TEXT("A mining swing cannot spawn/rebuild a vein"),Item->CrystalClues->GetInstanceCount()<=Before);
                }
                Test->TestTrue(TEXT("Excavate actual white/guide terrain"),Cut);
                Test->TestTrue(TEXT("Real excavation exposes mineral"),Item->MeasureExposure()>.5f);Aim();
            }
            else if(Phase==2)
            {
                Pawn->SetActorLocation(Item->GetActorLocation()+Normal*180-FVector(0,0,64),false,nullptr,ETeleportType::TeleportPhysics);
                Aim();Test->TestTrue(TEXT("Excavation removes unsupported crystal clues"),Item->CrystalClues->GetInstanceCount()<Fragments);
                for(int32 F=0;F<Item->CrystalClues->GetInstanceCount();++F)
                {
                    FTransform T;Item->CrystalClues->GetInstanceTransform(F,T,true);
                    Test->TestTrue(TEXT("Remaining clues do not follow the falling gem"),P.Fragments.ContainsByPredicate([&](const auto& Original){return Original.Equals(T,.1);}));
                }
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("CampaignMineralPreview/%d_Exposed.png"),Sample),true,false);
            }
            else if(Phase==3){Fragments=Item->CrystalClues->GetInstanceCount();Test->TestTrue(TEXT("Untouched rock retains crystal inclusions before pickup"),Fragments>0);Aim();PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1));}
            else if(Phase==4)
            {
                PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0));
                Test->TestTrue(TEXT("Real E collects the gem"),Item->bCollected && !Item->Model->IsVisible());
                Test->TestEqual(TEXT("Shared wallet receives exactly its integer value"),Wallet->Balance,Balance+Item->Value);
                Test->TestFalse(TEXT("A collected gem cannot pay twice"),Field->CollectTarget(PC));
                Test->TestTrue(TEXT("Collecting the gem does not hide supported clues"),!Item->IsHidden() && Item->CrystalClues->IsVisible());
                Test->TestEqual(TEXT("Pickup retains every remaining terrain inclusion"),Item->CrystalClues->GetInstanceCount(),Fragments);
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("CampaignMineralPreview/%d_Collected.png"),Sample),true,false);
                Field->ResetExhibit();Test->TestTrue(TEXT("Reset cannot respawn campaign rewards"),Field->Items[I]==Item && Item->bCollected);
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("CampaignMineralPreview/%d_Collected.png"),Sample),true,false);
            }
            else
            {
                float CleanupRadius=850;
                for(const FVector& Support:P.FragmentSupports)
                    CleanupRadius=FMath::Max(CleanupRadius,float(FVector::Dist(Support,P.Transform.GetLocation()))+100);
                for(ADiggableRock* Rock:Item->CoveringRocks){Rock->MineSphere(P.Transform.GetLocation(),CleanupRadius);DrainCut(Rock,Test);}
                // Check immediately: rendering even one frame of removed inclusions is a bug.
                TArray<bool> RemainingSupport;
                AMiningTreasure::MarkCoveredPoints(Item->CoveringRocks,P.FragmentSupports,RemainingSupport);
                int ExpectedClues=0;for(bool Solid:RemainingSupport)ExpectedClues+=Solid;
                // A deliberately oversized cleanup can be rejected at thin terrain
                // seams. Also test ordinary pickaxe-sized cuts directly on surviving
                // inclusions; retaining uncut rock must never be mistaken for a clue bug.
                int Attempts=0;
                for(int S=0;S<P.FragmentSupports.Num() && ExpectedClues>=Fragments && Attempts<3;++S)
                {
                    if(!RemainingSupport[S])continue;
                    ++Attempts;
                    for(ADiggableRock* Rock:Item->CoveringRocks){Rock->MineSphere(P.FragmentSupports[S]+FVector(.731,1.231,.521),90);DrainCut(Rock,Test);}
                    AMiningTreasure::MarkCoveredPoints(Item->CoveringRocks,P.FragmentSupports,RemainingSupport);
                    ExpectedClues=0;for(bool Solid:RemainingSupport)ExpectedClues+=Solid;
                }
                Test->TestEqual(TEXT("After collection, clues exactly match surviving rock support"),Item->CrystalClues->GetInstanceCount(),ExpectedClues);
                Test->TestTrue(TEXT("Post-pickup excavation removes supported clues"),ExpectedClues<Fragments);
                // The wide cleanup cut can reach the next sample's vein. Restore only terrain;
                // claimed rewards stay claimed, and dropped neighbours are re-buried on the next tick.
                for(ADiggableRock* Rock:Item->CoveringRocks)Rock->ResetRock();
            }
        }
        ++Step;Changed=FPlatformTime::Seconds();return false;
    }
private:
    FAutomationTestBase* Test;int Step=0,Fragments=0;int64 Balance=0;TArray<int> Samples;
    double Started=FPlatformTime::Seconds(),Changed=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampaignMineralsTest,"BrainMining.Campaign.MineralEconomyAndExcavation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCampaignMineralsTest::RunTest(const FString& Parameters)
{
    AutomationOpenMap(TEXT("/Game/Campaign/Maps/Lvl_FullJourney"));
    ADD_LATENT_AUTOMATION_COMMAND(CampaignMineralTests::FStart());
    ADD_LATENT_AUTOMATION_COMMAND(CampaignMineralTests::FRun(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
