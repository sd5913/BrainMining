#if WITH_DEV_AUTOMATION_TESTS
#include "EndingEmergence.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"

class FEndingEditableSceneryPreview : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(),At=0;
    int Step=0;
    TWeakObjectPtr<AStaticMeshActor> Cell,Building;
    FTransform CellLocal,BuildingLocal,OriginalCellLocal,OriginalBuildingLocal;
public:
    explicit FEndingEditableSceneryPreview(FAutomationTestBase* InTest):Test(InTest){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>120){Test->AddError(TEXT("Editable scenery runtime timed out"));return true;}
        UWorld* World=AutomationCommon::GetAnyGameWorld();if(!World)return false;
        AEndingEmergence* Demo=nullptr;for(TActorIterator<AEndingEmergence> It(World);It;++It){Demo=*It;break;}
        if(!Demo||!Demo->bReady||Demo->Age<2)return false;
        if(Step==0)
        {
            Test->TestTrue(TEXT("Actual ending map has protected authored scenery"),Demo->bEditableScenery);
            int Count=0;bool AllAttached=true;
            for(TActorIterator<AStaticMeshActor> It(World);It;++It)if(It->ActorHasTag(TEXT("EndingEditableScenery")))
            {
                ++Count;AllAttached&=It->GetAttachParentActor()==Demo;
                if(It->ActorHasTag(TEXT("EndingEditableCell"))&&!Cell.IsValid())Cell=*It;
                if(It->ActorHasTag(TEXT("EndingEditableCity"))&&!Building.IsValid())Building=*It;
            }
            Test->TestTrue(TEXT("Scenery actors survive level loading into the game world"),Count>0&&Cell.IsValid()&&Building.IsValid()&&AllAttached);
            if(!Cell.IsValid()||!Building.IsValid()){Test->AddError(TEXT("Missing editable cell/city actors"));return true;}
            CellLocal=Cell->GetRootComponent()->GetRelativeTransform();BuildingLocal=Building->GetRootComponent()->GetRelativeTransform();
            OriginalCellLocal=CellLocal;OriginalBuildingLocal=BuildingLocal;
            Test->TestTrue(TEXT("Cell visible at start"),Cell->GetRootComponent()->IsVisible());
            Test->TestFalse(TEXT("Street hidden at start"),Building->GetRootComponent()->IsVisible());
            // Simulate an artist's offsets: animation must move the anchors, never overwrite these transforms.
            CellLocal.AddToTranslation(FVector(17,-11,9));Cell->GetRootComponent()->SetRelativeTransform(CellLocal);
            BuildingLocal.AddToTranslation(FVector(31,23,0));Building->GetRootComponent()->SetRelativeTransform(BuildingLocal);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Validation/EndingEditable/cell.png"),false,false);
            At=Demo->Age;Step=1;
        }
        else if(Step==1&&Demo->Age-At>.6f)
        {
            Test->TestTrue(TEXT("Cell animation preserves artist offset"),Cell->GetRootComponent()->GetRelativeTransform().Equals(CellLocal));
            Demo->PreviewFilm();Demo->SetShot(1);At=Demo->Age;Step=2;
        }
        else if(Step==2&&Demo->Age-At>.6f)
        {
            Test->TestFalse(TEXT("Cell leaves with its original flow stage"),Cell->GetRootComponent()->IsVisible());
            Test->TestTrue(TEXT("Authored street appears with the city stage"),Building->GetRootComponent()->IsVisible());
            Test->TestTrue(TEXT("City shot preserves artist offset"),Building->GetRootComponent()->GetRelativeTransform().Equals(BuildingLocal));
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Validation/EndingEditable/city.png"),false,false);
            At=Demo->Age;Step=3;
        }
        else if(Step==3&&Demo->Age-At>.5f){Demo->SetShot(3);At=Demo->Age;Step=4;}
        else if(Step==4&&Demo->Age-At>.5f)
        {
            Test->TestFalse(TEXT("Authored street hides during Earth shot"),Building->GetRootComponent()->IsVisible());
            Demo->RestartDemo();
            Test->TestTrue(TEXT("Restart restores cell visibility"),Cell->GetRootComponent()->IsVisible());
            Test->TestFalse(TEXT("Restart hides street"),Building->GetRootComponent()->IsVisible());
            Test->TestTrue(TEXT("Restart preserves cell edits"),Cell->GetRootComponent()->GetRelativeTransform().Equals(CellLocal));
            Test->TestTrue(TEXT("Restart preserves building edits"),Building->GetRootComponent()->GetRelativeTransform().Equals(BuildingLocal));
            Cell->GetRootComponent()->SetRelativeTransform(OriginalCellLocal);
            Building->GetRootComponent()->SetRelativeTransform(OriginalBuildingLocal);
            return true;
        }
        return false;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingEditableSceneryTest,"BrainMining.EndingEmergence.EditableScenery",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FEndingEditableSceneryTest::RunTest(const FString& Parameters)
{
    AutomationOpenMap(TEXT("/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo"));
    ADD_LATENT_AUTOMATION_COMMAND(FEndingEditableSceneryPreview(this));
    return true;
}
#endif
