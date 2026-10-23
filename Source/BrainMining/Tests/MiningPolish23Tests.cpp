#include "MiningAudioSettings.h"
#include "MiningSaveSubsystem.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPolishSettingsTest,"BrainMining.Polish23.FovPersistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningPolishSettingsTest::RunTest(const FString&)
{
    auto* Owner=NewObject<UGameInstance>();
    auto* Settings=NewObject<UMiningAudioSettings>(Owner);
    TestEqual(TEXT("Wider default"),Settings->GetFieldOfView(),100.f);
    Settings->SetFieldOfView(120);TestEqual(TEXT("Upper clamp"),Settings->GetFieldOfView(),115.f);
    Settings->SetFieldOfView(70);TestEqual(TEXT("Lower clamp"),Settings->GetFieldOfView(),80.f);
    Settings->SetFieldOfView(107);
    const FString File=FPaths::ProjectSavedDir()/TEXT("Validation/Polish23/settings-test.ini");
    Settings->SaveVolumes(File);
    auto* Reloaded=NewObject<UMiningAudioSettings>(Owner);Reloaded->LoadVolumes(File);
    TestEqual(TEXT("FOV persists across sessions"),Reloaded->GetFieldOfView(),107.f);
    IFileManager::Get().Delete(*File);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPolishSaveTest,"BrainMining.Polish23.SaveRoundTrip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningPolishSaveTest::RunTest(const FString&)
{
    auto* Data=NewObject<UMiningSaveGame>();Data->Area=3;Data->UnlockedArea=4;Data->CompletedAreas={0,1,2};
    Data->Minerals.Add(EMiningMineral::Copper,17);Data->Upgrades={2,0,0,0};Data->Collection={0,1,0,1};
    Data->TutorialActive=true;Data->TutorialCompleted=15;Data->PrototypesFound=3;
    Data->TutorialSoilPassed=true;
    Data->CollectedMinerals={2,5};Data->Bomb=true;Data->PlayerTransform.SetLocation(FVector(120,340,560));
    UE::Geometry::FDynamicMesh3 Mesh;
    Mesh.AppendVertex(FVector3d(0,0,0));Mesh.AppendVertex(FVector3d(10,0,0));Mesh.AppendVertex(FVector3d(0,10,0));Mesh.AppendTriangle(0,1,2);
    auto& Saved=Data->Meshes.AddDefaulted_GetRef();Saved.ActorKey=TEXT("Main:Rock");Saved.Cuts=3;Saved.Chunk=8;
    FMemoryWriter Writer(Saved.Data,true);Mesh.Serialize(Writer);Writer.Close();
    TArray<uint8> Bytes;TestTrue(TEXT("Serialize snapshot"),UGameplayStatics::SaveGameToMemory(Data,Bytes));
    auto* Restored=Cast<UMiningSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if(!TestNotNull(TEXT("Snapshot loads"),Restored))return false;
    TestEqual(TEXT("Mineral balance"),Restored->Minerals.FindRef(EMiningMineral::Copper),17);
    TestEqual(TEXT("Upgrade level"),Restored->Upgrades[0],2);TestTrue(TEXT("Ability ownership"),Restored->Bomb);
    TestEqual(TEXT("Tutorial completion persists"),Restored->TutorialCompleted,15);
    TestTrue(TEXT("Soil crossing persists"),Restored->TutorialSoilPassed);
    TestEqual(TEXT("Prototype sequence persists"),Restored->PrototypesFound,3);
    TestTrue(TEXT("Collected mineral IDs"),Restored->CollectedMinerals.Contains(5));
    TestTrue(TEXT("Completed rooms"),Restored->CompletedAreas.Contains(2));
    FMemoryReader Reader(Restored->Meshes[0].Data,true);UE::Geometry::FDynamicMesh3 Reloaded;Reloaded.Serialize(Reader);
    TestEqual(TEXT("Terrain topology retained"),Reloaded.TriangleCount(),1);
    TestTrue(TEXT("Terrain vertices retained"),Reloaded.GetVertex(1).Equals(FVector3d(10,0,0)));
    return true;
}
#endif
