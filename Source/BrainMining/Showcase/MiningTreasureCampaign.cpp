#include "Showcase/MiningTreasure.h"
#include "DiggableRock.h"
#include "Engine/World.h"

void AMiningTreasureField::SpawnAuthored()
{
    bAuthoredGenerated=true;
    for(const auto& Placement:AuthoredPlacements)
    {
        const auto* Definition=Definitions.FindByPredicate([&](const auto& D){return D.Id==Placement.TreasureId;});
        if(!Definition || !Definition->IsEligible(Placement.AreaTag,Placement.bGuideLayer) ||
           !Definition->ValuesBySize.Contains(Placement.Value) || Placement.Hosts.IsEmpty())
        {UE_LOG(LogTemp,Error,TEXT("Invalid campaign mineral placement %s"),*Placement.TreasureId.ToString());continue;}
        FActorSpawnParameters Params;Params.Owner=this;
        auto* Item=GetWorld()->SpawnActor<AMiningTreasure>(Placement.Transform.GetLocation(),Placement.Transform.Rotator(),Params);
        Item->Tags.Add(Placement.AreaTag);Item->Tags.Add(TEXT("CampaignMineral"));
        Item->CoveringRocks=Placement.Hosts;
        if(!Item->Configure(*Definition,Definition->ValuesBySize.IndexOfByKey(Placement.Value),TreasureMaterial))
        {Item->Destroy();continue;}
        Item->Exposure=Item->MeasureExposure();
        Item->ConfigureCrystalClues(Placement,CrystalClueMesh);
        Items.Add(Item);ObservedCuts.Add(INDEX_NONE);
    }
    GenerationStatus=FString::Printf(TEXT("矿脉 %d / %d"),Items.Num(),AuthoredPlacements.Num());
}
