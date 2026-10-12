#include "Showcase/MiningTreasure.h"
#include "DiggableRock.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTreasureRulesTest, "BrainMining.Treasure.RulesAndWeights", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTreasureRulesTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("Exactly half exposed cannot be collected"), AMiningTreasure::IsExposureEligible(.5f));
    TestTrue(TEXT("Strictly above half can be collected"), AMiningTreasure::IsExposureEligible(.501f));
    TestFalse(TEXT("Buried cannot be collected"), AMiningTreasure::IsExposureEligible(0));
    auto Catalogue = GetDefault<AMiningTreasureField>()->Definitions;
    TestEqual(TEXT("Nine configurable treasures"), Catalogue.Num(), 9);
    FRandomStream Random(77), SameRandom(77);
    for (int32 Index = 0; Index < 50; ++Index)
        TestEqual(TEXT("Same seed reproduces weighted choices"), AMiningTreasureField::ChooseDefinition(Catalogue, TEXT("TreasureShowcase"), false, Random), AMiningTreasureField::ChooseDefinition(Catalogue, TEXT("TreasureShowcase"), false, SameRandom));
    TestEqual(TEXT("Excluded region has no spawn"), AMiningTreasureField::ChooseDefinition(Catalogue, TEXT("Other"), false, Random), INDEX_NONE);
    for (auto& Definition : Catalogue) Definition.Weight = 0;
    Catalogue[4].Weight = 5;
    for (int32 Index = 0; Index < 20; ++Index)
        TestEqual(TEXT("Zero weights excluded"), AMiningTreasureField::ChooseDefinition(Catalogue, TEXT("TreasureShowcase"), false, Random), 4);
    Catalogue[4].bGuideLayer = false;
    TestEqual(TEXT("Layer filtering applies before sampling"), AMiningTreasureField::ChooseDefinition(Catalogue, TEXT("TreasureShowcase"), true, Random), INDEX_NONE);
    Catalogue[4].ValuesBySize[2] = -1;
    TestEqual(TEXT("Invalid size value excluded"), AMiningTreasureField::ChooseDefinition(Catalogue, TEXT("TreasureShowcase"), false, Random), INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTreasureExposureTest, "BrainMining.Treasure.ExposureAndGeneration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTreasureExposureTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Field = World->SpawnActor<AMiningTreasureField>();
    for (const auto& Definition : Field->Definitions)
    {
        TestTrue(TEXT("Nine authored mesh assets replace engine primitives"), Definition.Model && Definition.Model->GetPathName().StartsWith(TEXT("/Game/Showcase/Treasure/Meshes/")));
        auto* Preview = World->SpawnActor<AMiningTreasure>();
        double PreviousSize = 0;
        for (int32 Tier = 0; Tier < 4; ++Tier)
        {
            TestTrue(TEXT("Every model size has sampleable collision"), Preview->Configure(Definition, Tier, Field->TreasureMaterial));
            TestEqual(TEXT("Area-weighted external surface samples for every size"), Preview->GetSurfaceSampleCount(), 384);
            TestEqual(TEXT("Size determines its exact value"), Preview->Value, Definition.ValuesBySize[Tier]);
            TestTrue(TEXT("Drop collision follows the actual model size"), Preview->DropBody->GetScaledBoxExtent().Equals(Preview->Model->Bounds.BoxExtent, .01));
            TestTrue(TEXT("Reconfiguring size does not translate the authored mesh"), Preview->Model->GetComponentLocation().IsNearlyZero(.01));
            const double Size = Preview->Model->Bounds.SphereRadius;
            TestTrue(TEXT("Each higher price has a physically larger model"), Size > PreviousSize);
            PreviousSize = Size;
        }
        Preview->Destroy();
    }
    Field->ResetExhibit();
    TestEqual(TEXT("Both layers populated"), Field->Items.Num(), 18);
    if (Field->Items.Num() != 18) return false;
    TArray<FName> Ids;
    TArray<int32> Values;
    for (AMiningTreasure* Item : Field->Items)
    {
        Ids.Add(Item->TreasureId);
        Values.Add(Item->Value);
        TestTrue(TEXT("Spawn completely buried"), Item->MeasureExposure() < .01f);
        TestFalse(TEXT("Buried drop body cannot fall"), Item->DropBody->IsSimulatingPhysics());
        TestFalse(TEXT("Buried mineral cannot release early"), Item->TryRelease());
        const auto* Definition = Field->Definitions.FindByPredicate([Item](const auto& Entry) { return Entry.Id == Item->TreasureId; });
        TestTrue(TEXT("Value matches the generated size tier"), Definition && Definition->ValuesBySize.IsValidIndex(Item->SizeTier) && Item->Value == Definition->ValuesBySize[Item->SizeTier]);
    }
    auto* Item = Field->Items[0].Get();
    auto* Rock = Item->CoveringRocks[0].Get();
    TestEqual(TEXT("Buried item already supplies rock hint material"), Item->HintMaterials.Num(), 1);
    TestEqual(TEXT("Buried hint enabled before any exposure"), Item->GetHintAmount(), 1.f);
    TestFalse(TEXT("No point light inside solid rock"), Item->GlowLight->IsVisible());
    auto* GuideRock = Field->Rocks[9].Get();
    auto* GuideHint = Field->Items[9]->HintMaterials[0].Get();
    TestFalse(TEXT("Hint uses a supported non-dynamic parent"), GuideHint->Parent->IsA<UMaterialInstanceDynamic>());
    FLinearColor GuideColor;
    GuideHint->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Tint")), GuideColor);
    TestTrue(TEXT("Hint preserves purple guide tint"), GuideColor.Equals(FLinearColor(.065f,.04f,.09f)));
    GuideRock->ResetRock();
    TestTrue(TEXT("Rock reset preserves localized hint material"), GuideRock->RockMaterial == GuideHint);
    TestTrue(TEXT("Real cut succeeds"), Rock->MineSphere(Item->GetActorLocation(), 130));
    TestTrue(TEXT("Cut exposes actual surface"), Item->MeasureExposure() > .99f);
    Item->Exposure = Item->MeasureExposure();
    Item->UpdateGlow(.1f, true);
    TestTrue(TEXT("Exposed item can light nearby rock"), Item->GlowLight->IsVisible());
    Item->bCollected = true;
    Item->UpdateGlow(.3f, true);
    TestEqual(TEXT("Claim fades hint to zero"), Item->GetHintAmount(), 0.f);
    TestFalse(TEXT("Claim disables supplemental light"), Item->GlowLight->IsVisible());
    float HintAmount = -1;
    Item->HintMaterials[0]->GetScalarParameterValue(FMaterialParameterInfo(TEXT("HintAmount")), HintAmount);
    TestEqual(TEXT("Material receives cleared hint"), HintAmount, 0.f);
    Item->bCollected = false;
    auto* OtherRock = World->SpawnActor<ADiggableRock>(Item->GetActorLocation(), FRotator::ZeroRotator);
    OtherRock->RockSize = FVector(180);
    OtherRock->ResetRock();
    Item->CoveringRocks.Add(OtherRock);
    TestTrue(TEXT("Another covering rock prevents false exposure"), Item->MeasureExposure() < .01f);
    OtherRock->Destroy();
    Rock->ResetRock();
    TestTrue(TEXT("Reset reburies model"), Item->MeasureExposure() < .01f);
    Field->Coins = 900;
    Field->SetAreaActive(true);
    Field->SetAreaActive(false);
    TestEqual(TEXT("Leaving area preserves coins"), Field->Coins, int64(900));
    Field->ResetExhibit();
    TestEqual(TEXT("Explicit local restart clears demonstration coins"), Field->Coins, int64(0));
    TestEqual(TEXT("Restart keeps item count"), Field->Items.Num(), Ids.Num());
    for (int32 Index = 0; Index < Field->Items.Num(); ++Index)
    {
        TestEqual(TEXT("Restart reproduces type"), Field->Items[Index]->TreasureId, Ids[Index]);
        TestEqual(TEXT("Restart reproduces value"), Field->Items[Index]->Value, Values[Index]);
    }
    Field->Definitions[0].Id = Field->Definitions[1].Id;
    Field->ResetExhibit();
    TestEqual(TEXT("Duplicate IDs fail without partial generation"), Field->Items.Num(), 0);
    return true;
}
#endif
