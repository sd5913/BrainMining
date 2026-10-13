#include "MiningStartupOptimizeCommandlet.h"
#include "GeneratedMiningRock.h"
#include "MiningTerrainSource.h"
#include "UDynamicMesh.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"

UMiningStartupOptimizeCommandlet::UMiningStartupOptimizeCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UMiningStartupOptimizeCommandlet::Main(const FString& Params)
{
	const FString MapPath = TEXT("/Game/Campaign/Maps/Lvl_FullJourney");
	const FString MapFilename = FPackageName::LongPackageNameToFilename(MapPath, FPackageName::GetMapPackageExtension());
	const bool bVerifyOnly = FParse::Param(*Params, TEXT("VerifyOnly"));
	UPackage* MapPackage = LoadPackage(nullptr, *MapPath, LOAD_None);
	UWorld* World = MapPackage ? UWorld::FindWorldInPackage(MapPackage) : nullptr;
	if (!World || !World->PersistentLevel) return 1;
	FString BackupDirectory;
	if (!bVerifyOnly)
	{
		BackupDirectory = FPaths::ProjectSavedDir() / TEXT("CampaignBackups") / (TEXT("shared-terrain-") + FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		IFileManager::Get().MakeDirectory(*BackupDirectory, true);
		if (IFileManager::Get().Copy(*(BackupDirectory / TEXT("Lvl_FullJourney.umap")), *MapFilename) != COPY_OK) return 2;
		UE_LOG(LogTemp, Display, TEXT("Terrain source backup: %s"), *BackupDirectory);
	}
	int32 Rocks = 0;
	int32 Chunks = 0;
	int64 Triangles = 0;
	FString Manifest;
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_None;
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		auto* Rock = Cast<AGeneratedMiningRock>(Actor);
		if (!Rock || !Rock->bChunked || Rock->Chunks.IsEmpty()) continue;
		int64 RockTriangles = 0;
		for (const auto& Chunk : Rock->Chunks)
		{
			if (!Chunk.InitialMesh) return 3;
			RockTriangles += Chunk.InitialMesh->GetTriangleCount();
		}
		if (!Rock->TerrainSource)
		{
			if (bVerifyOnly) return 4;
			const FString AssetPath = TEXT("/Game/Campaign/TerrainData/") + Rock->GetName();
			if (FPackageName::DoesPackageExist(AssetPath)) return 5;
			UPackage* SourcePackage = CreatePackage(*AssetPath);
			auto* Source = NewObject<UMiningTerrainSource>(SourcePackage, *Rock->GetName(), RF_Public | RF_Standalone);
			if (!Rock->ExtractTerrainSource(Source)) return 6;
			const FString Filename = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
			if (!UPackage::SavePackage(SourcePackage, Source, *Filename, SaveArgs)) return 7;
		}
		if (Rock->TerrainSource->Meshes.Num() != Rock->Chunks.Num()) return 8;
		for (int32 Index = 0; Index < Rock->Chunks.Num(); ++Index)
		{
			const auto* Mesh = Rock->Chunks[Index].InitialMesh.Get();
			if (Mesh != Rock->TerrainSource->Meshes[Index] || Mesh->GetOutermost() == MapPackage) return 9;
		}
		Manifest += FString::Printf(TEXT("%s,%d,%lld\n"), *Rock->GetName(), Rock->Chunks.Num(), RockTriangles);
		++Rocks;
		Chunks += Rock->Chunks.Num();
		Triangles += RockTriangles;
	}
	if (!bVerifyOnly)
	{
		if (!FFileHelper::SaveStringToFile(Manifest, *(BackupDirectory / TEXT("TerrainCounts.csv")))) return 10;
		if (!UPackage::SavePackage(MapPackage, World, *MapFilename, SaveArgs)) return 11;
	}
	UE_LOG(LogTemp, Display, TEXT("Shared terrain verified: %d rocks, %d chunks, %lld source triangles, verifyOnly=%d"), Rocks, Chunks, Triangles, bVerifyOnly);
	return Rocks > 0 ? 0 : 12;
}
