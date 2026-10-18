#include "Subtitles/SubtitleSettings.h"
#include "Subtitles/SubtitleWidget.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
void GatherSubtitleText(const TSharedRef<SWidget>& Widget, TArray<TSharedRef<STextBlock>>& Blocks)
{
	if (Widget->GetType() == FName(TEXT("STextBlock")))
		Blocks.Add(StaticCastSharedRef<STextBlock>(Widget));
	FChildren* Children = Widget->GetChildren();
	for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
		GatherSubtitleText(Children->GetChildAt(Index), Blocks);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSubtitleSharedStyleTest, "BrainMining.Subtitles.SharedStyle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSubtitleSharedStyleTest::RunTest(const FString&)
{
	auto* Settings = GetMutableDefault<USubtitleSettings>();
	const int32 OldSize = Settings->FontSize;
	const float OldChromatic = Settings->ChromaticStrength, OldFloat = Settings->FloatStrength;
	ON_SCOPE_EXIT { Settings->FontSize = OldSize; Settings->ChromaticStrength = OldChromatic; Settings->FloatStrength = OldFloat; };
	const auto First = USubtitleWidget::CreateStylePreview(FText::FromString(TEXT("第一条字幕")));
	const auto Second = USubtitleWidget::CreateStylePreview(FText::FromString(TEXT("第二条字幕")));
	Settings->FontSize = 36;
	Settings->ChromaticStrength = 2.f;
	Settings->FloatStrength = 2.f;
	for (const auto& Preview : { First, Second })
	{
		Preview->Tick(FGeometry(), 1., .5f);
		TArray<TSharedRef<STextBlock>> Blocks;
		GatherSubtitleText(Preview, Blocks);
		if (!TestEqual(TEXT("Each subtitle has two color layers and main text"), Blocks.Num(), 3)) return false;
		for (const auto& Block : Blocks) TestEqual(TEXT("All layers pick up the shared font immediately"), Block->GetFont().Size, 36.f);
		TestTrue(TEXT("Shared chromatic strength changes opacity"), FMath::IsNearlyEqual(Blocks[0]->GetColorAndOpacity().GetSpecifiedColor().A, .4f));
		TestTrue(TEXT("Shared float strength increases sine movement"), FMath::IsNearlyEqual(Blocks[2]->GetRenderTransform()->GetTranslation().Y, FMath::Sin(.5f * 1.9f) * 11.f));
	}
	Settings->ChromaticStrength = Settings->FloatStrength = 0.f;
	First->Tick(FGeometry(), 2., .25f);
	TArray<TSharedRef<STextBlock>> Blocks;
	GatherSubtitleText(First, Blocks);
	TestEqual(TEXT("Zero chromatic strength hides colored layers"), Blocks[0]->GetColorAndOpacity().GetSpecifiedColor().A, 0.f);
	TestTrue(TEXT("Zero float strength stops translation"), Blocks[2]->GetRenderTransform()->GetTranslation().IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSubtitleConfigPersistenceTest, "BrainMining.Subtitles.ConfigPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSubtitleConfigPersistenceTest::RunTest(const FString&)
{
	auto* Draft = NewObject<USubtitleSettings>();
	Draft->FontSize = 34;
	Draft->ChromaticStrength = 1.75f;
	Draft->FloatStrength = 1.6f;
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Subtitles"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Filename = FPaths::ConvertRelativePathToFull(FPaths::Combine(Directory, FGuid::NewGuid().ToString() + TEXT(".ini")));
	ON_SCOPE_EXIT { GConfig->UnloadFile(Filename); IFileManager::Get().Delete(*Filename); };
	if (!TestTrue(TEXT("Shared settings save to config"), Draft->TryUpdateDefaultConfigFile(Filename))) return false;
	auto* Reloaded = NewObject<USubtitleSettings>();
	Reloaded->LoadConfig(USubtitleSettings::StaticClass(), *Filename);
	TestEqual(TEXT("Font size survives reload"), Reloaded->FontSize, 34);
	TestEqual(TEXT("Chromatic strength survives reload"), Reloaded->ChromaticStrength, 1.75f);
	TestEqual(TEXT("Float strength survives reload"), Reloaded->FloatStrength, 1.6f);
	return true;
}
#endif
