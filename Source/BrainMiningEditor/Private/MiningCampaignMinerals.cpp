#include "MiningDesignerLibrary.h"
#include "Showcase/MiningExhibitHall.h"
#include "GeneratedMiningRock.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"

TArray<FMiningTreasurePlacement> UMiningDesignerLibrary::FindCampaignMineralSites(AActor* Actor)
{
    TArray<FMiningTreasurePlacement> Sites;
    auto* Hall=Cast<AMiningExhibitHall>(Actor);if(!Hall)return Sites;
    for(const auto& Area:Hall->Areas)
    {
        if(Area.Kind!=EMiningExperienceKind::Mining || Area.MapRoute.Num()<2)continue;
        TArray<ADiggableRock*> Rocks;
        for(TActorIterator<ADiggableRock> It(Hall->GetWorld());It;++It)
            if(It->ActorHasTag(Area.Tag) && !It->GetActorLabel().Contains(TEXT("BuriedBank")))Rocks.Add(*It);
        TArray<double> Arcs={0};
        for(int I=1;I<Area.MapRoute.Num();++I)Arcs.Add(Arcs.Last()+FVector::Dist(Area.MapRoute[I-1],Area.MapRoute[I]));
        const double Length=Arcs.Last();
        for(double D=80;D<Length-80;D+=65)
        {
            int Segment=1;while(Segment<Arcs.Num()-1 && Arcs[Segment]<D)++Segment;
            const FVector Center=FMath::Lerp(Area.MapRoute[Segment-1],Area.MapRoute[Segment],(D-Arcs[Segment-1])/FMath::Max(1.,Arcs[Segment]-Arcs[Segment-1]))+FVector(0,0,40);
            for(int Ray=0;Ray<24;++Ray)
            {
                const FVector P=Center+FVector(0,0,(Ray/8-1)*85);
                const float Angle=(Ray%8)*PI/4;const FVector Direction(FMath::Cos(Angle),FMath::Sin(Angle),Ray%3==0?.12:0);
                FHitResult Best;double Distance=1400;ADiggableRock* Host=nullptr;
                for(auto* Rock:Rocks)
                {
                    if(Rock->GetComponentsBoundingBox(true).ComputeSquaredDistanceToPoint(P)>1400*1400)continue;
                    FHitResult Hit;
                    if(Rock->ActorLineTraceSingle(Hit,P,P+Direction*1400,ECC_Visibility,FCollisionQueryParams(NAME_None,true)) && Hit.Distance<Distance && Hit.Distance>25)
                    {Best=Hit;Distance=Hit.Distance;Host=Rock;}
                }
                if(!Host || FMath::Abs(Best.ImpactNormal.Z)>.75)continue;
                FMiningTreasurePlacement S;S.AreaTag=Area.Tag;S.RouteProgress=D/Length;
                S.bGuideLayer=Host->GetActorLabel().Contains(TEXT("引导"));
                S.Surface=FTransform(Best.ImpactNormal.Rotation(),Best.ImpactPoint);
                S.Transform=FTransform(FRotator(0,Ray*47,0),Best.ImpactPoint-Best.ImpactNormal*120);
                S.Hosts.Add(Host);
                TArray<FVector> Probe={S.Transform.GetLocation()};
                for(int Axis=0;Axis<3;++Axis)for(float Side:{-1.f,1.f}){FVector Q=Probe[0];Q[Axis]+=Side*55;Probe.Add(Q);}
                TArray<bool> Covered;AMiningTreasure::MarkCoveredPoints(S.Hosts,Probe,Covered);
                if(Covered.Contains(false))continue;
                Sites.Add(S);
            }
        }
    }
    UE_LOG(LogTemp,Display,TEXT("Campaign mineral candidates: %d"),Sites.Num());
    return Sites;
}

bool UMiningDesignerLibrary::PrepareCampaignMinerals(AMiningTreasureField* Field)
{
    if(!Field)return false;
    FRandomStream Random(74621);int Failures=0;
    for(auto& P:Field->AuthoredPlacements)
    {
        const auto* D=Field->Definitions.FindByPredicate([&](const auto& Definition){return Definition.Id==P.TreasureId;});
        if(!D || P.Hosts.IsEmpty()){++Failures;continue;}
        auto* Preview=Field->GetWorld()->SpawnActor<AMiningTreasure>(P.Transform.GetLocation(),P.Transform.Rotator());
        Preview->CoveringRocks=P.Hosts;
        const bool Configured=Preview->Configure(*D,D->ValuesBySize.IndexOfByKey(P.Value),Field->TreasureMaterial);
        const FVector Normal=P.Surface.GetRotation().GetForwardVector();
        const float Depth=FMath::Max(85.f,float(Preview->Model->Bounds.SphereRadius)+55.f)+(P.RouteProgress>.82f?35.f:0.f);
        P.Transform.SetLocation(P.Surface.GetLocation()-Normal*Depth);
        Preview->SetActorLocation(P.Transform.GetLocation());
        // Include every nearby solid, including chunks of the original white shell.
        const FBox Search=Preview->Model->Bounds.GetBox().ExpandBy(120);
        for(TActorIterator<ADiggableRock> It(Field->GetWorld());It;++It)
            if(It->ActorHasTag(P.AreaTag) && It->GetComponentsBoundingBox(true).Intersect(Search))P.Hosts.AddUnique(*It);
        Preview->CoveringRocks=P.Hosts;
        if(!Configured || Preview->MeasureExposure()>.01f)
        {UE_LOG(LogTemp,Error,TEXT("Mineral not buried: %s %s"),*P.TreasureId.ToString(),*P.Transform.GetLocation().ToString());++Failures;Preview->Destroy();continue;}
        P.Fragments.Reset();P.FragmentSupports.Reset();
        const FVector Right=FVector::CrossProduct(Normal,FVector::UpVector).GetSafeNormal();
        const FVector Up=FVector::CrossProduct(Right,Normal).GetSafeNormal();
        auto AddChip=[&](FVector Position,FVector Support,float Scale)
        {
            TArray<bool> Covered;AMiningTreasure::MarkCoveredPoints(P.Hosts,{Support},Covered);
            if(!Covered[0])return;
            P.Fragments.Add(FTransform(FRotator(Random.FRandRange(-70,70),Random.FRandRange(0,360),Random.FRandRange(-60,60)),Position,FVector(Scale,Scale*.65,Scale*1.6)));
            P.FragmentSupports.Add(Support);
        };
        // Small shards are half embedded at the actual uneven surface, not floating on a flat plane.
        for(int I=0;I<18;++I)
        {
            const FVector Offset=Right*Random.FRandRange(-145,145)+Up*Random.FRandRange(-110,110);
            const FVector Base=P.Surface.GetLocation()+Offset;FHitResult Hit;
            if(P.Hosts[0]->ActorLineTraceSingle(Hit,Base+Normal*90,Base-Normal*100,ECC_Visibility,FCollisionQueryParams(NAME_None,true)))
                AddChip(Hit.ImpactPoint+Hit.ImpactNormal*3,Hit.ImpactPoint-Hit.ImpactNormal*9,Random.FRandRange(.12,.18));
        }
        const int SurfaceCount=P.Fragments.Num();
        for(int I=1;I<=17;++I)
        {
            const FVector Q=FMath::Lerp(P.Surface.GetLocation(),P.Transform.GetLocation(),I/18.f)+Right*Random.FRandRange(-100,100)+Up*Random.FRandRange(-85,85);
            AddChip(Q,Q,Random.FRandRange(.10,.18));
        }
        if(SurfaceCount<3){UE_LOG(LogTemp,Error,TEXT("Too few surface clues: %s"),*P.TreasureId.ToString());++Failures;}
        Preview->Destroy();
    }
    UE_LOG(LogTemp,Display,TEXT("Campaign minerals prepared: %d, failures %d"),Field->AuthoredPlacements.Num(),Failures);
    return Failures==0;
}
