#include "Showcase/MiningTreasure.h"
#include "Showcase/MiningExhibitHall.h"
#include "DiggableRock.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

bool AMiningTreasureField::InitializeAuthoredItem(AMiningTreasure* Item, const FMiningTreasurePlacement& Placement)
{
    const auto* Definition=Definitions.FindByPredicate([&](const auto& Entry){return Entry.Id==Placement.TreasureId;});
    if(!Definition || !Item->Configure(*Definition,Definition->ValuesBySize.IndexOfByKey(Placement.Value),TreasureMaterial)) return false;
    Item->Exposure=Item->MeasureExposure();
    Item->ConfigureCrystalClues(Placement,CrystalClueMesh);
    Item->SetActorHiddenInGame(false);
    return true;
}

void AMiningTreasureField::PrepareAuthoredArea(FName AreaTag)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Mining_PrepareAuthoredArea);
    for(auto Pending=PendingAuthoredItems.CreateIterator();Pending;++Pending)
    {
        auto* Item=Pending.Key().Get();
        if(!IsValid(Item) || !AuthoredPlacements.IsValidIndex(Pending.Value())) {Pending.RemoveCurrent();continue;}
        const auto& Placement=AuthoredPlacements[Pending.Value()];
        if(Placement.AreaTag!=AreaTag) continue;
        if(!InitializeAuthoredItem(Item,Placement))
        {
            UE_LOG(LogTemp,Error,TEXT("Failed to initialize campaign mineral %s"),*Placement.TreasureId.ToString());
            Item->Destroy();
        }
        Pending.RemoveCurrent();
    }
}

void AMiningTreasureField::SpawnAuthored()
{
    bAuthoredGenerated=true;
    const auto* Hall=AMiningExhibitHall::Find(GetWorld());
    const bool bDeferAreas=GetWorld()->IsGameWorld() && Hall && Hall->bContinuousCampaign;
    for(int32 PlacementIndex=0;PlacementIndex<AuthoredPlacements.Num();++PlacementIndex)
    {
        const auto& Placement=AuthoredPlacements[PlacementIndex];
        const auto* Definition=Definitions.FindByPredicate([&](const auto& D){return D.Id==Placement.TreasureId;});
        if(!Definition || !Definition->IsEligible(Placement.AreaTag,Placement.bGuideLayer) ||
           !Definition->ValuesBySize.Contains(Placement.Value) || Placement.Hosts.IsEmpty())
        {UE_LOG(LogTemp,Error,TEXT("Invalid campaign mineral placement %s"),*Placement.TreasureId.ToString());continue;}
        FActorSpawnParameters Params;Params.Owner=this;
        auto* Item=GetWorld()->SpawnActor<AMiningTreasure>(Placement.Transform.GetLocation(),Placement.Transform.Rotator(),Params);
        Item->Tags.Add(Placement.AreaTag);Item->Tags.Add(TEXT("CampaignMineral"));
        Item->CoveringRocks=Placement.Hosts;
        if(bDeferAreas)
        {
            Item->TreasureId=Definition->Id;
            Item->TreasureName=Definition->Name;
            Item->Value=Placement.Value;
            Item->SizeTier=Definition->ValuesBySize.IndexOfByKey(Placement.Value);
            Item->SetActorHiddenInGame(true);
            PendingAuthoredItems.Add(Item,PlacementIndex);
        }
        else if(!InitializeAuthoredItem(Item,Placement))
        {Item->Destroy();continue;}
        Items.Add(Item);ObservedCuts.Add(INDEX_NONE);
    }
    if(bAreaActive) PrepareAuthoredArea(ActiveAreaTag);
    GenerationStatus=FString::Printf(TEXT("矿脉 %d / %d"),Items.Num(),AuthoredPlacements.Num());
}
