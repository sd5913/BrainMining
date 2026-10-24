#include "MiningCollection.h"
#include "DiggableRock.h"
#include "Components/BoxComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "EchoPassageStage.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "SMiningCollectionView.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollectionScrollTest,"BrainMining.Collection.HorizontalCatalogueInput",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCollectionScrollTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Field=World->SpawnActor<AMiningCollectionField>();Field->bDeepField=true;Field->bExpandedCollection=true;Field->bCatalogueOpen=true;
    auto View=SNew(SMiningCollectionView).Owner(Field);
    const FGeometry Geometry=FGeometry::MakeRoot(FVector2D(1600,900),FSlateLayoutTransform());
    auto Pointer=[](FVector2D P,FKey Button,float Wheel=0.f){return FPointerEvent(0,P,P,TSet<FKey>(),Button,Wheel,FModifierKeysState());};
    View->OnMouseWheel(Geometry,Pointer(FVector2D(800,820),EKeys::Invalid,-30));
    View->OnMouseButtonDown(Geometry,Pointer(FVector2D(1450,820),EKeys::LeftMouseButton));
    View->OnMouseButtonUp(Geometry,Pointer(FVector2D(1450,820),EKeys::LeftMouseButton));
    TestEqual(TEXT("Wheel scrolling reaches the last locked collectible"),Field->SelectedId,11);
    View->OnMouseButtonDown(Geometry,Pointer(FVector2D(200,820),EKeys::LeftMouseButton));
    View->OnMouseMove(Geometry,Pointer(FVector2D(1500,820),EKeys::Invalid));
    View->OnMouseButtonUp(Geometry,Pointer(FVector2D(1500,820),EKeys::LeftMouseButton));
    TestEqual(TEXT("Dragging the strip does not accidentally select a card"),Field->SelectedId,11);
    View->OnMouseWheel(Geometry,Pointer(FVector2D(800,820),EKeys::Invalid,30));
    View->OnMouseButtonDown(Geometry,Pointer(FVector2D(100,820),EKeys::LeftMouseButton));
    View->OnMouseButtonUp(Geometry,Pointer(FVector2D(100,820),EKeys::LeftMouseButton));
    TestEqual(TEXT("Scrolling back reaches the first card"),Field->SelectedId,0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollectionExposureTest,"BrainMining.Collection.ExposureAndPhysicsGate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCollectionExposureTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Rock=World->SpawnActor<ADiggableRock>();
    Rock->RockSize=FVector(500); Rock->ResetRock();
    auto* Bird=World->SpawnActor<AMiningCollectible>(FVector(600,0,0),FRotator::ZeroRotator);
    Bird->BuildModel(3);Bird->HostRock=Rock;Bird->Tags.Add(TEXT("BM_ExcavationReward"));Bird->Exposure=1;
    TestFalse(TEXT("Soil wall reward waits for excavation even when its model protrudes"),Bird->CanRelease(.7f));
    Bird->Release();TestFalse(TEXT("Direct release cannot bypass intact soil gate"),Bird->bReleased);
    Rock->SuccessfulCuts=1;
    TestTrue(TEXT("One successful cut enables normal exposure release"),Bird->CanRelease(.7f));
    Bird->Release();
    const FTransform BirdPose=Bird->GetActorTransform();
    TestEqual(TEXT("Resident pigeon has two separate feathered wings"),Bird->EmergenceParts.Num(),2);
    if(Bird->EmergenceParts.Num()==2)
    {
        Bird->AnimateEmergence(.08f);
        const FRotator Up=Bird->EmergenceParts[0]->GetRelativeRotation();
        const FVector Shoulder=Bird->EmergenceParts[0]->GetRelativeLocation();
        Bird->AnimateEmergence(.27f);
        TestFalse(TEXT("Released pigeon beats its wings"),Up.Equals(Bird->EmergenceParts[0]->GetRelativeRotation()));
        TestTrue(TEXT("Wing shoulder stays attached during flapping"),Shoulder.Equals(Bird->EmergenceParts[0]->GetRelativeLocation()));
        TestTrue(TEXT("Left and right wing beats are mirrored"),FMath::IsNearlyZero(Bird->EmergenceParts[0]->GetRelativeRotation().Roll+Bird->EmergenceParts[1]->GetRelativeRotation().Roll));
        Bird->SetHighlighted(true);
        for(auto Part:Bird->EmergenceParts)
        {
            auto* Wing=Cast<UDynamicMeshComponent>(Part.Get());
            TestTrue(TEXT("Hover outline includes moving feathers"),Wing && Wing->bRenderCustomDepth);
            TestTrue(TEXT("Wing visuals never block interaction"),Wing && Wing->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
        }
    }
    TestTrue(TEXT("Hovering keeps the saved pickup transform stable"),BirdPose.Equals(Bird->GetActorTransform()));
    Bird->Destroy();Rock->SuccessfulCuts=0;
    for(int Id=0;Id<4;++Id)
    {
        auto* Item=World->SpawnActor<AMiningCollectible>(); Item->BuildModel(Id); Item->HostRock=Rock;
        TestTrue(TEXT("A fully buried model has zero exposure"),Item->MeasureExposure()<.01f);
        Item->Exposure=.69f; TestFalse(TEXT("Below 70% remains buried"),Item->CanRelease(.65f));
        Item->Exposure=.70f; TestTrue(TEXT("70% exposure triggers release"),Item->CanRelease(.65f));
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollectionExpansionTest,"BrainMining.Collection.ExpandedModelsAndCatalogue",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCollectionExpansionTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Field=World->SpawnActor<AMiningCollectionField>();
    TestEqual(TEXT("Existing fields retain four catalogue entries"),Field->CatalogueIds().Num(),4);
    Field->bDeepField=true; Field->bExpandedCollection=true;
    TestEqual(TEXT("Expanded test room shows all twelve entries"),Field->CatalogueIds().Num(),12);
    TSet<FString> Names;
    for(int Id=0;Id<AMiningCollectible::ItemTypeCount;++Id)
    {
        auto* Item=World->SpawnActor<AMiningCollectible>(); Item->BuildModel(Id);
        Names.Add(AMiningCollectible::ItemName(Id));
        TestEqual(TEXT("Stable item ID survives model building"),Item->ItemId,Id);
        TestTrue(TEXT("Every entry has inspectable geometry"),Item->Model->GetDynamicMesh()->GetMeshRef().TriangleCount()>100);
        TestFalse(TEXT("Every entry has a description"),AMiningCollectible::Description(Id).IsEmpty());
        for(int V:Item->Model->GetDynamicMesh()->GetMeshRef().VertexIndicesItr())
        {
            const FVector P=Item->Model->GetDynamicMesh()->GetMeshRef().GetVertex(V);
            const FVector E=Item->Body->GetUnscaledBoxExtent();
            if(Id>=4 && (FMath::Abs(P.X)>E.X+.01 || FMath::Abs(P.Y)>E.Y+.01 || FMath::Abs(P.Z)>E.Z+.01))
            { AddError(FString::Printf(TEXT("Item %d geometry exceeds interaction / release body"),Id)); break; }
        }
        const FTransform ActorPose=Item->GetActorTransform();
        Item->AnimateEmergence(.2f);const FTransform VisualA=Item->Model->GetRelativeTransform();
        TArray<FTransform> PartsA;for(auto Part:Item->EmergenceParts)PartsA.Add(Part->GetRelativeTransform());
        Item->AnimateEmergence(1.7f);bool Changed=!VisualA.Equals(Item->Model->GetRelativeTransform());
        for(int I=0;I<PartsA.Num();++I)Changed|=!PartsA[I].Equals(Item->EmergenceParts[I]->GetRelativeTransform());
        TestTrue(FString::Printf(TEXT("Item %d has visual motion"),Id),Changed);
        TestTrue(TEXT("Animation never moves interaction body or saved actor"),ActorPose.Equals(Item->GetActorTransform()));
        Item->Destroy();
    }
    TestEqual(TEXT("All twenty-three names are distinct"),Names.Num(),AMiningCollectible::ItemTypeCount);
    for(int Id:{5,6,7}) TestTrue(TEXT("Direct displays have no excavation clue"),AMiningCollectible::Clue(Id).IsEmpty());
    Field->bUseAuthoredPlacements=true;
    FMiningCollectionPlacement Placement; Placement.ItemId=11; Field->AuthoredPlacements.Add(Placement); Field->AuthoredPlacements.Add(Placement);
    TestEqual(TEXT("Authored catalogue deduplicates types"),Field->CatalogueIds().Num(),1);
    TestEqual(TEXT("Authored catalogue supports nonzero IDs"),Field->CatalogueIds()[0],11);
    auto* Campaign=World->SpawnActor<AMiningCollectionField>();Campaign->bCatalogueOnly=true;
    const auto Order=Campaign->CatalogueIds();TSet<int32> UniqueIds;for(int Id:Order)UniqueIds.Add(Id);
    TestEqual(TEXT("Campaign order keeps all stable inventory IDs"),UniqueIds.Num(),AMiningCollectible::ItemTypeCount);
    TestTrue(TEXT("Copper and iron rewards lead the catalogue"),Order[0]==12 && Order[1]==13);
    for(int Id:{1,2,4,5,16,17})TestTrue(TEXT("First passage and office precede the malachite reward"),Order.IndexOfByKey(Id)<Order.IndexOfByKey(14));
    TestEqual(TEXT("Diamond follows malachite"),Order.IndexOfByKey(15),Order.IndexOfByKey(14)+1);
    for(int Id:{3,9,6,7,11,18,19})TestTrue(TEXT("Second passage and its rooms follow diamond and precede the third passage"),Order.IndexOfByKey(Id)>Order.IndexOfByKey(15) && Order.IndexOfByKey(Id)<Order.IndexOfByKey(20));

    auto* Soil=World->SpawnActor<ADiggableRock>(FVector(0,0,130),FRotator::ZeroRotator);
    Soil->RockSize=FVector(600,600,260); Soil->ResetRock();
    auto* Flower=World->SpawnActor<AMiningCollectible>(FVector(0,0,175),FRotator::ZeroRotator);
    Flower->BuildModel(9); Flower->HostRock=Soil;
    const float Exposure=Flower->MeasureExposure();
    TestTrue(TEXT("A buried root crown prevents immediate release even with visible flowers"),Exposure<.65f);
    Flower->Exposure=1; Flower->Release(); TestFalse(TEXT("Roots cannot release through intact soil"),Flower->bReleased);
    Soil->MineSphere(Flower->GetActorLocation()-FVector(0,0,20),65);
    Flower->Exposure=Flower->MeasureExposure();Flower->Release();
    TestFalse(TEXT("A shallow crown cut cannot release the deep roots"),Flower->bReleased);
    TestTrue(TEXT("Excavation clears the actual root volume"),Soil->MineSphere(Flower->GetActorLocation()-FVector(0,0,90),300));
    Flower->Exposure=Flower->MeasureExposure(); Flower->Release();
    TestTrue(TEXT("Fully excavated root system can be collected"),Flower->bReleased);
    auto* Calculator=World->SpawnActor<AMiningCollectible>(FVector(0,0,150),FRotator(0,35,0));
    Calculator->BuildModel(4);Calculator->HostRock=Soil;
    const FTransform Pose=Calculator->GetActorTransform();
    Calculator->Exposure=Calculator->MeasureExposure();Calculator->Release();
    TestTrue(TEXT("Excavated calculator releases for pickup"),Calculator->bReleased);
    TestFalse(TEXT("Calculator cannot fall through freshly rebuilt terrain"),Calculator->Body->IsSimulatingPhysics());
    TestTrue(TEXT("Calculator retains its readable orientation after release"),Calculator->GetActorQuat().Equals(Pose.GetRotation()));
    const FVector Pop=Calculator->GetActorLocation()-Pose.GetLocation();
    TestTrue(TEXT("Calculator only pops a short distance towards the excavated opening"),
        Pop.Size()<=252.01f && FVector::DotProduct(Pop,-Pose.GetRotation().GetForwardVector())>=0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollectionEchoTest,"BrainMining.Collection.AuthoredEchoSharesRoomPulse",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCollectionEchoTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Stage=World->SpawnActor<AEchoPassageStage>();Stage->OnConstruction(Stage->GetActorTransform());
    UMaterialInterface* Surface=nullptr;UMaterialInterface* Contours=nullptr;
    TArray<UInstancedStaticMeshComponent*> Components;Stage->GetComponents(Components);
    for(auto* C:Components)
    {
        if(C->GetFName()==TEXT("CollisionRock"))Surface=C->GetMaterial(0);
        if(C->GetFName()==TEXT("EchoContours"))Contours=C->GetMaterial(0);
    }
    TestNotNull(TEXT("Room surface pulse exists"),Surface);TestNotNull(TEXT("Room contour pulse exists"),Contours);
    for(int Id:{6,7})
    {
        auto* Item=World->SpawnActor<AMiningCollectible>();Item->BuildModel(Id);Item->ApplyEchoPresentation(Surface,Contours);
        TestTrue(TEXT("Collectible uses the room's actual shared pulse material"),Item->Outline->GetMaterial(0)==Contours && Item->Model->GetMaterial(0)==Surface);
        TestTrue(TEXT("Model contours are real geometry"),Item->Outline->GetDynamicMesh()->GetMeshRef().TriangleCount()>100);
        Item->SetHighlighted(true);
        TestTrue(TEXT("Aimed echo collectible uses the standard selection silhouette"),Item->Model->bRenderCustomDepth);
        Item->SetHighlighted(false);
        TestTrue(TEXT("Pickup highlighting preserves echo-controlled contours"),Item->Outline->IsVisible());
        TestFalse(TEXT("Pickup highlighting never creates permanent visibility in dark maze"),Item->Model->bRenderCustomDepth);
    }
    return true;
}
#endif
