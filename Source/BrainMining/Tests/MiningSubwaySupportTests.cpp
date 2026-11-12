#include "MiningCollection.h"
#include "Components/BoxComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSubwaySupportTest,"BrainMining.Collection.SubwaySettlesAfterCrowd",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSubwaySupportTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto Floor=[&](float Height)
    {
        auto* Actor=World->SpawnActor<AActor>();
        auto* Box=NewObject<UBoxComponent>(Actor);Actor->SetRootComponent(Box);Actor->AddInstanceComponent(Box);
        Box->SetBoxExtent(FVector(1000,1000,20));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        Box->RegisterComponent();Actor->SetActorLocation(FVector(0,0,Height-20));
        return Box;
    };
    auto* Ground=Floor(0.f);
    auto* Support=Floor(210.f);
    Support->SetBoxExtent(FVector(12,12,20));Support->SetWorldLocation(FVector(30,-40,190));
    auto* Train=World->SpawnActor<AMiningCollectible>(FVector(30,-40,400),FRotator(0,37,0));
    Train->BuildModel(21);Train->SetActorScale3D(FVector(1.8));Train->BuriedTransform=Train->GetActorTransform();
    const auto Local=Train->Model->GetDynamicMesh()->GetMeshRef().GetBounds();
    auto Bottom=[&](){return FBox(FVector(Local.Min),FVector(Local.Max)).TransformBy(Train->GetActorTransform()).Min.Z;};
    const FVector Initial=Train->GetActorLocation();
    Train->Release();Train->AnimateEmergence(1.f);
    for(int I=0;I<60;++I)Train->UpdateSubwaySupport(1.f/60.f);
    TestTrue(TEXT("Train remains steady before the crowd's three-second presentation finishes"),Train->GetActorLocation().Equals(Initial));
    Train->AnimateEmergence(4.96f);
    for(int I=0;I<150;++I)Train->UpdateSubwaySupport(1.f/60.f);
    TestTrue(TEXT("Train falls onto even a small remaining soil support after the crowd presentation"),Bottom()>=209.99 && Bottom()<=211.);
    const FVector Supported=Train->GetActorLocation();
    for(int I=0;I<60;++I)Train->UpdateSubwaySupport(1.f/60.f);
    TestTrue(TEXT("Intact support holds the carriage at rest"),Train->GetActorLocation().Equals(Supported,.01));

    Support->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Train->UpdateSubwaySupport(1.f/60.f);
    TestTrue(TEXT("Removing support starts a gentle downward fall"),Train->GetActorLocation().Z<Supported.Z && Train->GetActorLocation().Z>Supported.Z-2.);
    for(int I=0;I<180;++I)Train->UpdateSubwaySupport(1.f/60.f);
    const FVector Landed=Train->GetActorLocation();
    TestTrue(TEXT("Downward sweep stops the train on the lower floor without tunneling"),Bottom()>=-.01 && Bottom()<=1.);
    TestTrue(TEXT("No horizontal ejection or drift occurs"),FVector2D(Landed.X,Landed.Y).Equals(FVector2D(Initial.X,Initial.Y),.001));
    TestTrue(TEXT("Carriage keeps its readable orientation"),Train->GetActorRotation().Equals(FRotator(0,37,0),.001));
    TestFalse(TEXT("Settling never enables Chaos physics"),Train->Body->IsSimulatingPhysics());
    TestTrue(TEXT("Settled carriage remains available to normal pickup"),Train->IsCollectible());
    Ground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Train->UpdateSubwaySupport(1.f/60.f);
    TestTrue(TEXT("Digging away the new resting surface resumes the fall"),Train->GetActorLocation().Z<Landed.Z);

    Train->Tags.Add(TEXT("CollectionPreview"));const FVector PreviewPose=Train->GetActorLocation();
    Train->UpdateSubwaySupport(.1f);
    TestTrue(TEXT("Catalogue preview never falls"),Train->GetActorLocation().Equals(PreviewPose));
    Train->Tags.Remove(TEXT("CollectionPreview"));Train->bCollected=true;
    Train->UpdateSubwaySupport(.1f);
    TestTrue(TEXT("Collected objects do not continue settling"),Train->GetActorLocation().Equals(PreviewPose));
    return true;
}
#endif
