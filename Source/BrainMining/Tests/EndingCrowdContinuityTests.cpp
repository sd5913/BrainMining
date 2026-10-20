#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "EndingCrowd.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingCrowdContinuityTest,
    "BrainMining.EndingEmergence.CrowdContinuity",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FEndingCrowdContinuityTest::RunTest(const FString&)
{
    auto* Assets=LoadObject<UEndingCrowdAssets>(nullptr,TEXT("/Game/EndingEmergence/Crowd/Lite/DA_EndingCrowdLite"));
    if(!TestNotNull(TEXT("Baked crowd asset"),Assets))return false;
    UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
    AActor* Owner=World->SpawnActor<AActor>();
    auto* Crowd=NewObject<UEndingCrowdComponent>(Owner);
    Owner->AddInstanceComponent(Crowd);Owner->SetRootComponent(Crowd);Crowd->RegisterComponent();
    Crowd->SetWorldLocation(FVector(22000,0,0));
    if(!TestTrue(TEXT("Build crowd"),Crowd->Build(Assets)))return false;
    TestEqual(TEXT("All people retained"),Crowd->PersonCount(),1201);
    TSet<FString> Colors;
    int32 NeutralCount=0;
    for(auto Batch:Crowd->Batches)
    {
        TestFalse(TEXT("Moving pedestrians do not use static HISM cluster culling"),Batch->IsA<UHierarchicalInstancedStaticMeshComponent>());
        TestEqual(TEXT("City origin survives root creation"),Batch->GetComponentLocation(),FVector(22000,0,0));
        TestEqual(TEXT("Animation plus shirt/trouser RGB"),Batch->NumCustomDataFloats,8);
        TestTrue(TEXT("All crowd batches cast animated shadows"),bool(Batch->CastShadow));
        for(int32 I=0;I<Batch->GetInstanceCount();++I)
        {
            Colors.Add(FString::Printf(TEXT("%.3f,%.3f,%.3f"),Batch->PerInstanceSMCustomData[I*8+2],Batch->PerInstanceSMCustomData[I*8+3],Batch->PerInstanceSMCustomData[I*8+4]));
            const float R=Batch->PerInstanceSMCustomData[I*8+2],G=Batch->PerInstanceSMCustomData[I*8+3],B=Batch->PerInstanceSMCustomData[I*8+4];
            if(FMath::Max3(R,G,B)-FMath::Min3(R,G,B)<.01f)++NeutralCount;
        }
    }
    TestTrue(TEXT("At least twelve shirt colors"),Colors.Num()>=12);
    TestTrue(TEXT("Mostly black white and grey with a minority of colors"),NeutralCount>1201*.80f&&NeutralCount<1201*.90f);
    // Exercise every real instance across the old +/-42m wrap boundary and
    // through a slowed forty-second street shot, rather than testing a formula.
    double MaxStep=0;bool CrossedOldBoundary=false;
    for(int32 Frame=0;Frame<800;++Frame)
    {
        TArray<TArray<FTransform>> Before=Crowd->BatchTransforms;
        Crowd->UpdateCrowd(true,.05f);
        for(int32 B=0;B<Crowd->Batches.Num()-1;++B)
            for(int32 I=0;I<Before[B].Num();++I)
            {
                FTransform Actual;Crowd->Batches[B]->GetInstanceTransform(I,Actual);
                MaxStep=FMath::Max(MaxStep,FVector::Distance(Before[B][I].GetLocation(),Actual.GetLocation()));
                const FVector P=Actual.GetLocation();
                CrossedOldBoundary|=FMath::Abs(P.X)>4200||FMath::Abs(P.Y)>4200;
            }
    }
    TestTrue(TEXT("Walkers cross the former wrap boundary"),CrossedOldBoundary);
    TestTrue(TEXT("No instance teleports in forty seconds"),MaxStep<8.1);
    Crowd->UpdateCrowd(false,0);
    Crowd->UpdateCrowd(true,0);
    TestEqual(TEXT("Replay resets walking time"),Crowd->TravelTime,0.f);
    TestEqual(TEXT("Replay preserves population"),Crowd->PersonCount(),1201);
    Owner->Destroy();
    return true;
}
#endif
