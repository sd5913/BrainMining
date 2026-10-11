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
#include "UObject/UnrealType.h"

TArray<AActor*> UMiningDesignerLibrary::CopyExperienceActors(const FString& SourceMap, FVector Offset, FName AreaTag, TSubclassOf<AActor> ActorClass)
{
    TArray<AActor*> Result;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || World->GetOutermost()->GetName() == SourceMap) return Result;
    ULevelStreaming* Source = UEditorLevelUtils::AddLevelToWorld(World, *SourceMap, ULevelStreamingAlwaysLoaded::StaticClass());
    if (!Source || !Source->GetLoadedLevel()) return Result;
    UEditorLevelUtils::SetLevelVisibility(Source->GetLoadedLevel(), true, false);
    TArray<AActor*> Originals;
    for (AActor* Actor : Source->GetLoadedLevel()->Actors)
        if (Actor && Actor->GetRootComponent() && !Actor->IsA<AWorldSettings>() && !Actor->IsA<ALevelScriptActor>() && (!Actor->IsA<ABrush>() || Actor->IsA<APostProcessVolume>()) && (!ActorClass || Actor->IsA(ActorClass))) Originals.Add(Actor);
    const FString CopyPrefix=TEXT("BM_Copy_")+FGuid::NewGuid().ToString()+TEXT("_");
    for(int32 Index=0; Index<Originals.Num(); ++Index) Originals[Index]->Tags.Add(FName(CopyPrefix+FString::FromInt(Index)));
    UEditorLevelUtils::CopyActorsToLevel(Originals, World->PersistentLevel, false, false, true, &Result);
    TMap<UObject*,UObject*> Replacements;
    for(int32 Index=0; Index<Originals.Num(); ++Index)
    {
        const FName CopyTag(CopyPrefix+FString::FromInt(Index));
        for(AActor* Copy:Result) if(Copy->ActorHasTag(CopyTag))
        { Replacements.Add(Originals[Index],Copy); Copy->Tags.Remove(CopyTag); break; }
        Originals[Index]->Tags.Remove(CopyTag);
    }
    TFunction<void(FProperty*,const void*,void*)> RestoreReferences;
    RestoreReferences=[&](FProperty* Property,const void* OriginalValue,void* CopyValue)
    {
        if(auto* ObjectProperty=CastField<FObjectPropertyBase>(Property))
        {
            if(UObject** Replacement=Replacements.Find(ObjectProperty->GetObjectPropertyValue(OriginalValue)))
                ObjectProperty->SetObjectPropertyValue(CopyValue,*Replacement);
        }
        else if(auto* ArrayProperty=CastField<FArrayProperty>(Property))
        {
            FScriptArrayHelper SourceArray(ArrayProperty,OriginalValue),CopyArray(ArrayProperty,CopyValue);
            for(int32 Index=0; Index<FMath::Min(SourceArray.Num(),CopyArray.Num()); ++Index)
                RestoreReferences(ArrayProperty->Inner,SourceArray.GetRawPtr(Index),CopyArray.GetRawPtr(Index));
        }
        else if(auto* StructProperty=CastField<FStructProperty>(Property))
            for(TFieldIterator<FProperty> Field(StructProperty->Struct); Field; ++Field)
                RestoreReferences(*Field,Field->ContainerPtrToValuePtr<void>(OriginalValue),Field->ContainerPtrToValuePtr<void>(CopyValue));
    };
    for(const auto& Pair:Replacements)
        for(TFieldIterator<FProperty> Property(Pair.Key->GetClass()); Property; ++Property)
            if(!Property->HasAnyPropertyFlags(CPF_Transient))
                RestoreReferences(*Property,Property->ContainerPtrToValuePtr<void>(Pair.Key),Property->ContainerPtrToValuePtr<void>(Pair.Value));
    TArray<FTransform> Targets;
    for (AActor* Actor : Result) { FTransform Target=Actor->GetActorTransform(); Target.AddToTranslation(Offset); Targets.Add(Target); }
    for (int32 Index=0; Index<Result.Num(); ++Index)
    {
        Result[Index]->SetActorTransform(Targets[Index], false, nullptr, ETeleportType::TeleportPhysics);
        Result[Index]->Tags.AddUnique(AreaTag);
        Result[Index]->SetFolderPath(FName(FString(TEXT("Experiences/"))+AreaTag.ToString()));
    }
    UEditorLevelUtils::RemoveLevelFromWorld(Source->GetLoadedLevel(), true, true);
    UEditorLevelUtils::MakeLevelCurrent(World->PersistentLevel);
    return Result;
}

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
