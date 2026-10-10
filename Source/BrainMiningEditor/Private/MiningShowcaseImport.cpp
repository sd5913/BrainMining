#include "MiningDesignerLibrary.h"
#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/ExponentialHeightFog.h"

TArray<AActor*> UMiningDesignerLibrary::ImportShowcaseRoom(const FString& SourceMap, FVector Offset, const TArray<FString>& ExcludedLabels)
{
    TArray<AActor*> Result;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || World->GetOutermost()->GetName() == SourceMap) return Result;
    ULevelStreaming* Source = UEditorLevelUtils::AddLevelToWorld(World, *SourceMap, ULevelStreamingAlwaysLoaded::StaticClass());
    if (!Source || !Source->GetLoadedLevel()) return Result;
    UEditorLevelUtils::SetLevelVisibility(Source->GetLoadedLevel(), true, false);
    TArray<AActor*> Actors;
    for (AActor* Actor : Source->GetLoadedLevel()->Actors)
    {
        if (!Actor || !Actor->GetRootComponent() || Actor->IsA<AWorldSettings>() || Actor->IsA<APlayerStart>() || Actor->IsA<ALevelScriptActor>() ||
            Actor->IsA<APostProcessVolume>() || Actor->IsA<ADirectionalLight>() || Actor->IsA<ASkyLight>() || Actor->IsA<ASkyAtmosphere>() ||
            Actor->IsA<AExponentialHeightFog>() || Actor->IsA<ABrush>() || ExcludedLabels.Contains(Actor->GetActorLabel())) continue;
        Actors.Add(Actor);
    }
    for (AActor* Original : Actors)
    {
        FActorSpawnParameters Parameters;
        Parameters.Template = Original;
        Parameters.OverrideLevel = World->PersistentLevel;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Parameters.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
        const FTransform Transform = Original->GetActorTransform();
        if (AActor* Copy = World->SpawnActor(Original->GetClass(), &Transform, Parameters))
        {
            Copy->SetActorLabel(Original->GetActorLabel());
            Result.Add(Copy);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Continuous import: %s source=%d copied=%d"), *SourceMap, Actors.Num(), Result.Num());
    for (AActor* Actor : Result)
    {
        Actor->SetActorLocation(Actor->GetActorLocation() + Offset, false, nullptr, ETeleportType::TeleportPhysics);
        Actor->SetFolderPath(TEXT("Continuous/ImportedRooms"));
    }
    UEditorLevelUtils::RemoveLevelFromWorld(Source->GetLoadedLevel(), true, false);
    UEditorLevelUtils::MakeLevelCurrent(World->PersistentLevel);
    return Result;
}
