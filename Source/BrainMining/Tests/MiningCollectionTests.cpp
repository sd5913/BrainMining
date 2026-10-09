#include "MiningCollection.h"
#include "DiggableRock.h"
#include "Components/BoxComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollectionExposureTest,"BrainMining.Collection.ExposureAndPhysicsGate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCollectionExposureTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Rock=World->SpawnActor<ADiggableRock>();
    Rock->RockSize=FVector(500); Rock->ResetRock();
    for(int Id=0;Id<4;++Id)
    {
        auto* Item=World->SpawnActor<AMiningCollectible>(); Item->BuildModel(Id); Item->HostRock=Rock;
        TestTrue(TEXT("A fully buried model has zero exposure"),Item->MeasureExposure()<.01f);
        Item->Exposure=.64f; TestFalse(TEXT("Below threshold remains buried"),Item->CanRelease(.65f));
        Item->Exposure=.65f; TestTrue(TEXT("Exact configured threshold qualifies"),Item->CanRelease(.65f));
        Item->Release(); TestFalse(TEXT("No release or physics inside solid rock despite forced ratio"),Item->bReleased);
        TestFalse(TEXT("Buried object never simulates physics"),Item->Body->IsSimulatingPhysics());
        Item->Destroy();
    }
    auto* Item=World->SpawnActor<AMiningCollectible>(); Item->BuildModel(0); Item->HostRock=Rock;
    TestTrue(TEXT("Lowpoly front face normals point outward in UE coordinates"),Item->Model->GetDynamicMesh()->GetMeshRef().GetTriNormal(0).X<-.99);
    TestTrue(TEXT("A real boolean cut opens an enclosed cavity"),Rock->MineSphere(FVector::ZeroVector,150));
    TestTrue(TEXT("Actual mesh determines exposure in a cut cavity"),Item->MeasureExposure()>.99f);
    Item->Exposure=Item->MeasureExposure(); Item->Release();
    TestTrue(TEXT("Clear cavity allows one physics release"),Item->bReleased);
    TestTrue(TEXT("Released collectible simulates rigid body physics"),Item->Body->IsSimulatingPhysics());
    TestFalse(TEXT("Released item cannot release twice"),Item->CanRelease(.65f));
    Rock->ResetRock();
    TestTrue(TEXT("Reset restores rock occupancy"),Item->MeasureExposure()<.01f);
    TestFalse(TEXT("Invalid cutting request does not remove rock"),Rock->MineSphere(FVector::ZeroVector,-1));
    TestTrue(TEXT("Failed cut leaves exposure unchanged"),Item->MeasureExposure()<.01f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollectionReuseTest,"BrainMining.Collection.ReusesMiningPawnAndController",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCollectionReuseTest::RunTest(const FString& Parameters)
{
    auto* Base=GetDefault<AMiningPrototypeGameMode>(); auto* Collection=GetDefault<AMiningCollectionGameMode>();
    TestEqual(TEXT("Original first person pawn is reused"),Collection->DefaultPawnClass,Base->DefaultPawnClass);
    TestEqual(TEXT("Original player controller is reused"),Collection->PlayerControllerClass,Base->PlayerControllerClass);
    return true;
}
#endif
