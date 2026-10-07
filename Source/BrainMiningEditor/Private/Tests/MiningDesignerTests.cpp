#include "MiningDesignerCatalog.h"
#include "MiningDesignerLibrary.h"
#include "MiningTunnelGenerator.h"
#include "MiningLayoutRoom.h"
#include "MiningHollowRegion.h"
#include "MiningHollowChamber.h"
#include "MiningLayoutSpace.h"
#include "MiningTunnelRoute.h"
#include "MiningTunnelShapeSection.h"
#include "MiningComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDesignerCatalogTest, "BrainMining.Designer.CatalogAndOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDesignerCatalogTest::RunTest(const FString& Parameters)
{
	for (UClass* Type : {AMiningTunnelGenerator::StaticClass(), AMiningLayoutRoom::StaticClass(), AMiningHollowRegion::StaticClass(), AMiningHollowChamber::StaticClass(), AMiningLayoutSpace::StaticClass(), AMiningTunnelRoute::StaticClass(), AMiningTunnelShapeSection::StaticClass()})
	{
		const UObject* Default = Type->GetDefaultObject();
		TestEqual(TEXT("Every supported object resolves to its real class family"), MiningDesigner::Family(Default), Type);
		for (EMiningDesignerSection Section : {EMiningDesignerSection::Gameplay, EMiningDesignerSection::Mining, EMiningDesignerSection::Terrain, EMiningDesignerSection::Visual, EMiningDesignerSection::Advanced})
		{
			for (FName Name : MiningDesigner::Fields(Default, Section))
			{
				FProperty* Property = Type->FindPropertyByName(Name);
				TestNotNull(*FString::Printf(TEXT("Published parameter exists: %s.%s"), *Type->GetName(), *Name.ToString()), Property);
				TestTrue(TEXT("Published parameter has a Chinese caption"), MiningDesigner::Caption(Name) != Name.ToString());
				if (Property) TestTrue(TEXT("Only editable properties exposed"), Property->HasAnyPropertyFlags(CPF_Edit));
			}
		}
	}
	for (FName Name : {FName("Region"), FName("bManualBranchPath")}) TestFalse(TEXT("Ownership and path state cannot transfer through presets"), MiningDesigner::Transferable(AMiningHollowChamber::StaticClass()->FindPropertyByName(Name)));
	for (FName Name : {FName("ConnectionOrder"), FName("LayoutId")}) TestFalse(TEXT("Room identity cannot be reset or pasted"), MiningDesigner::Transferable(AMiningLayoutRoom::StaticClass()->FindPropertyByName(Name)));
	TestFalse(TEXT("Generated geometry is never preset data"), MiningDesigner::Transferable(AMiningTunnelGenerator::StaticClass()->FindPropertyByName("GeneratedFill")));
	TestFalse(TEXT("Class defaults cannot be edited through this level tool"), UMiningDesignerLibrary::CanEdit(AMiningTunnelGenerator::StaticClass()->GetDefaultObject()));
	TestEqual(TEXT("Triangle cap remains fixed"), AMiningTunnelGenerator::StaticClass()->FindPropertyByName("SegmentTriangleBudget")->GetMetaData("ClampMax"), FString("30000"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDesignerDiggingTest, "BrainMining.Designer.MiningOverrideCompatibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDesignerDiggingTest::RunTest(const FString& Parameters)
{
	UMiningComponent* Mining = NewObject<UMiningComponent>();
	Mining->Radius = 99;
	FMiningDiggingSettings Settings;
	Settings.ApplyTo(*Mining);
	TestEqual(TEXT("Existing blueprint values remain unchanged by default"), Mining->Radius, 99.f);
	Settings.bOverrideCharacter = true;
	Settings.Radius = 120;
	Settings.Reach = 650;
	Settings.Interval = 0.5f;
	Settings.ApplyTo(*Mining);
	TestEqual(TEXT("Per-level radius"), Mining->Radius, 120.f);
	TestEqual(TEXT("Per-level reach"), Mining->Reach, 650.f);
	TestEqual(TEXT("Per-level interval"), Mining->Interval, 0.5f);
	Settings.Radius = std::numeric_limits<float>::quiet_NaN();
	Settings.Interval = 0;
	Settings.FallResetDistance = 0;
	Settings.ApplyTo(*Mining);
	TestEqual(TEXT("Non-finite values are safe at runtime"), Mining->Radius, 160.f);
	TestEqual(TEXT("No zero-duration excavation loop"), Mining->Interval, 0.1f);
	TestEqual(TEXT("Safety reset can still be disabled explicitly"), Mining->FallResetDistance, 0.f);
	return true;
}

#endif
