#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Showcase/MiningTreasure.h"
#include "Showcase/MiningExhibitHall.h"
#include "MiningWalletSubsystem.h"
#include "DiggableRock.h"
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
                Test->TestTrue(TEXT("Real chunked host initially buries the whole gem"),Item->MeasureExposure()<.01f);
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
            Test->TestTrue(TEXT("Crystal clues use a simple eight-face shard"),Mesh && Mesh->GetRenderData()->LODResources[0].GetNumTriangles()==8);
            H->UnlockedArea=6;
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
                const float Radius=FVector::Dist(Item->GetActorLocation(),P.Surface.GetLocation())+70;
                bool Cut=false;for(ADiggableRock* Rock:Item->CoveringRocks)Cut|=Rock->MineSphere(Item->GetActorLocation(),Radius);
                Test->TestTrue(TEXT("Excavate actual white/guide terrain"),Cut);
                Test->TestTrue(TEXT("Real excavation exposes mineral"),Item->MeasureExposure()>.5f);Aim();
            }
            else if(Phase==2)
            {
                Aim();Test->TestTrue(TEXT("Excavation removes unsupported crystal clues"),Item->CrystalClues->GetInstanceCount()<Fragments);
                for(int32 F=0;F<Item->CrystalClues->GetInstanceCount();++F)
                {
                    FTransform T;Item->CrystalClues->GetInstanceTransform(F,T,true);
                    Test->TestTrue(TEXT("Remaining clues do not follow the falling gem"),P.Fragments.ContainsByPredicate([&](const auto& Original){return Original.Equals(T,.1);}));
                }
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("CampaignMineralPreview/%d_Exposed.png"),Sample),true,false);
            }
            else if(Phase==3){Aim();PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1));}
            else if(Phase==4)
            {
                PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0));
                Test->TestTrue(TEXT("Real E collects the gem"),Item->bCollected && !Item->Model->IsVisible());
                Test->TestEqual(TEXT("Shared wallet receives exactly its integer value"),Wallet->Balance,Balance+Item->Value);
                Test->TestFalse(TEXT("A collected gem cannot pay twice"),Field->CollectTarget(PC));
                Test->TestTrue(TEXT("Collecting the gem does not hide supported clues"),!Item->IsHidden() && Item->CrystalClues->IsVisible());
                for(ADiggableRock* Rock:Item->CoveringRocks)Rock->MineSphere(P.Transform.GetLocation(),550);
                Field->ResetExhibit();Test->TestTrue(TEXT("Reset cannot respawn campaign rewards"),Field->Items[I]==Item && Item->bCollected);
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("CampaignMineralPreview/%d_Collected.png"),Sample),true,false);
            }
            else
            {
                Test->TestEqual(TEXT("Clues also disappear when support is cut AFTER collection"),Item->CrystalClues->GetInstanceCount(),0);
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
