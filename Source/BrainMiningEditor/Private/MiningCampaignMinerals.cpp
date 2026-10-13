#include "MiningDesignerLibrary.h"
#include "Showcase/MiningExhibitHall.h"
#include "GeneratedMiningRock.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool UMiningDesignerLibrary::OptimizeCampaignTerrain(AActor* Hall)
{
    if(!Hall || !Hall->GetWorld())return false;
    bool bOK=true;
    for(TActorIterator<AGeneratedMiningRock> It(Hall->GetWorld());It;++It)
    {
        if(It->bChunked || It->TriangleCount<6000 || !It->GetActorLabel().StartsWith(TEXT("Campaign_")))continue;
        UE::Geometry::FDynamicMesh3 Mesh;It->CopyMiningMesh(Mesh);
        const bool bSplit=It->SetChunkedMesh(Mesh,400,true,true);
        UE_LOG(LogTemp,Display,TEXT("Interactive terrain chunks: %s (%d triangles), success=%d"),*It->GetActorLabel(),Mesh.TriangleCount(),bSplit);
        bOK&=bSplit;
    }
    return bOK;
}

TArray<FMiningTreasurePlacement> UMiningDesignerLibrary::FindCampaignMineralSites(AActor* Actor)
{
    TArray<FMiningTreasurePlacement> Sites;
    auto* Hall=Cast<AMiningExhibitHall>(Actor);if(!Hall || !Hall->GetWorld() || Hall->GetWorld()->IsGameWorld())return Sites;
    for(const auto& Area:Hall->Areas)
    {
        if(Area.Kind!=EMiningExperienceKind::Mining || Area.MapRoute.Num()<2)continue;
        TArray<ADiggableRock*> Rocks;
        for(TActorIterator<ADiggableRock> It(Hall->GetWorld());It;++It)
            if(It->ActorHasTag(Area.Tag) && !It->GetActorLabel().Contains(TEXT("BuriedBank")))Rocks.Add(*It);
        Rocks.Sort([](const ADiggableRock& A,const ADiggableRock& B){return A.GetPathName()<B.GetPathName();});
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
    if(!Field || !Field->GetWorld() || Field->GetWorld()->IsGameWorld())return false;
    FRandomStream Random(74621);int Failures=0;
    for(auto& P:Field->AuthoredPlacements)
    {
        const auto* D=Field->Definitions.FindByPredicate([&](const auto& Definition){return Definition.Id==P.TreasureId;});
        if(!D || P.Hosts.IsEmpty()){++Failures;continue;}
        const FVector PreviousCore=P.Transform.GetLocation();
        auto* Preview=Field->GetWorld()->SpawnActor<AMiningTreasure>(P.Transform.GetLocation(),P.Transform.Rotator());
        Preview->CoveringRocks=P.Hosts;
        const bool Configured=Preview->Configure(*D,D->ValuesBySize.IndexOfByKey(P.Value),Field->TreasureMaterial);
        FVector Normal=P.Surface.GetRotation().GetForwardVector();
        const float MinDepth=FMath::Max(float(FVector::Dist(P.Transform.GetLocation(),P.Surface.GetLocation()))+65.f,float(Preview->Model->Bounds.SphereRadius)+120.f);
        const float Rarity=FMath::Clamp((FCString::Atoi(*P.TreasureId.ToString())-1)/8.f,0.f,1.f);
        const float WantedDepth=MinDepth+Random.FRandRange(25.f,100.f+240.f*Rarity+25.f*Preview->SizeTier);
        // Include every nearby solid, including chunks of the original white shell.
        const FBox Search(P.Surface.GetLocation()-FVector(1200),P.Surface.GetLocation()+FVector(1200));
        for(TActorIterator<ADiggableRock> It(Field->GetWorld());It;++It)
            if(It->ActorHasTag(P.AreaTag) && It->GetComponentsBoundingBox(true).Intersect(Search))P.Hosts.AddUnique(*It);
        Preview->CoveringRocks=P.Hosts;
        bool Buried=false;
        for(float Depth=WantedDepth;Depth>=MinDepth-.1f;Depth=FMath::Max(MinDepth,Depth-25.f))
        {
            P.Transform.SetLocation(P.Surface.GetLocation()-Normal*Depth);
            Preview->SetActorLocation(P.Transform.GetLocation());
            if(Configured && Preview->MeasureExposure()<.01f){Buried=true;break;}
            if(Depth<=MinDepth)break;
        }
        // A thin guide layer may have no deeper solid along its original normal.
        // Search the neighboring wall for a thicker pocket, retaining a real surface clue.
        if(!Buried)
        {
            const FVector Original=P.Surface.GetLocation();
            const FVector Side=FVector::CrossProduct(Normal,FVector::UpVector).GetSafeNormal();
            for(float Radius:{100.f,200.f,300.f})for(int I=0;I<8 && !Buried;++I)
            {
                const FVector Q=Original+Side*(Radius*FMath::Cos(I*PI/4))+FVector::UpVector*(Radius*FMath::Sin(I*PI/4));
                FHitResult Best;double Distance=BIG_NUMBER;
                for(ADiggableRock* Host:P.Hosts)if(IsValid(Host))
                {
                    FHitResult Hit;
                    if(Host->ActorLineTraceSingle(Hit,Q+Normal*250,Q-Normal*500,ECC_Visibility,FCollisionQueryParams(NAME_None,true)) && Hit.Distance<Distance)
                    {Best=Hit;Distance=Hit.Distance;}
                }
                if(Distance==BIG_NUMBER)continue;
                for(float Depth=MinDepth;Depth<=MinDepth+350;Depth+=25)
                {
                    P.Transform.SetLocation(Best.ImpactPoint-Best.ImpactNormal*Depth);Preview->SetActorLocation(P.Transform.GetLocation());
                    if(Configured && Preview->MeasureExposure()<.01f)
                    {Buried=true;P.Surface=FTransform(Best.ImpactNormal.Rotation(),Best.ImpactPoint);Normal=Best.ImpactNormal;break;}
                }
            }
        }
        if(!Buried)
        {UE_LOG(LogTemp,Error,TEXT("Mineral not buried: %s %s"),*P.TreasureId.ToString(),*P.Transform.GetLocation().ToString());++Failures;Preview->Destroy();continue;}
        int PreviousInnerCount=0;
        // Count from the authored old core, before rebuilding this deposit's hint field.
        // Surface/trail samples are included: the requested doubling is of visible population.
        for(const auto& F:P.Fragments)
            if(FVector::DistSquared(F.GetLocation(),PreviousCore)<160*160)++PreviousInnerCount;
        P.Fragments.Reset();P.FragmentSupports.Reset();
        const FVector Right=FVector::CrossProduct(Normal,FVector::UpVector).GetSafeNormal();
        const FVector Up=FVector::CrossProduct(Right,Normal).GetSafeNormal();
        auto AddChip=[&](FVector Position,FVector Support,float Scale)
        {
            for(const FVector& Existing:P.FragmentSupports)if(FVector::DistSquared(Existing,Support)<32*32)return;
            TArray<bool> Covered;AMiningTreasure::MarkCoveredPoints(P.Hosts,{Support},Covered);
            if(!Covered[0])return;
            P.Fragments.Add(FTransform(Random.VRand().Rotation(),Position,FVector(Scale,Scale*.85,Scale)));
            P.FragmentSupports.Add(Support);
        };
        // A few flecks break the cave surface; most inclusions remain inside untouched rock.
        for(int I=0;I<100 && P.Fragments.Num()<5;++I)
        {
            const float A=Random.FRandRange(0,2*PI),R=Random.FRandRange(90,330);
            const FVector Base=P.Surface.GetLocation()+Right*FMath::Cos(A)*R+Up*FMath::Sin(A)*R*.8;
            FHitResult Best;float Distance=BIG_NUMBER;
            for(ADiggableRock* Host:P.Hosts)if(IsValid(Host))
            {
                FHitResult Hit;
                if(Host->ActorLineTraceSingle(Hit,Base+Normal*100,Base-Normal*130,ECC_Visibility,FCollisionQueryParams(NAME_None,true)) && Hit.Distance<Distance)
                {Best=Hit;Distance=Hit.Distance;}
            }
            if(Distance<BIG_NUMBER)AddChip(Best.ImpactPoint+Best.ImpactNormal*1.5,Best.ImpactPoint-Best.ImpactNormal*5,Random.FRandRange(.10,.15));
        }
        const int SurfaceCount=P.Fragments.Num();
        // Uniform radial sampling concentrates density towards the core without overcrowding.
        for(int I=0;I<500 && P.Fragments.Num()<45;++I)
        {
            const FVector Q=P.Transform.GetLocation()+Random.VRand()*Random.FRandRange(45,310);
            if(FVector::DotProduct(Q-P.Surface.GetLocation(),Normal)>-12)continue;
            AddChip(Q,Q,Random.FRandRange(.10,.17));
        }
        // Duplicate only the accepted inner population; the outer shell keeps its count.
        const int BaseCount=P.Fragments.Num();int InnerCount=0;
        for(int I=SurfaceCount;I<BaseCount;++I)
            if(FVector::DistSquared(P.FragmentSupports[I],P.Transform.GetLocation())<160*160)++InnerCount;
        const int Target=BaseCount+FMath::Max(0,2*FMath::Max(PreviousInnerCount,InnerCount)-InnerCount);
        for(int I=0;I<1200 && P.Fragments.Num()<Target;++I)
        {
            const FVector Q=P.Transform.GetLocation()+Random.VRand()*Random.FRandRange(45,160);
            if(FVector::DotProduct(Q-P.Surface.GetLocation(),Normal)>-12)continue;
            AddChip(Q,Q,Random.FRandRange(.10,.17));
        }
        if(P.Fragments.Num()!=Target){++Failures;UE_LOG(LogTemp,Error,TEXT("Inner crystal density target not met"));}
        const int TrailCount=FMath::Max(6,FMath::CeilToInt(FVector::Dist(P.Surface.GetLocation(),P.Transform.GetLocation())/65));
        for(int I=1;I<=TrailCount;++I)
        {
            const FVector Q=FMath::Lerp(P.Surface.GetLocation(),P.Transform.GetLocation(),float(I)/(TrailCount+1))+Right*Random.FRandRange(-55,55)+Up*Random.FRandRange(-45,45);
            AddChip(Q,Q,Random.FRandRange(.10,.14));
        }
        if(SurfaceCount<3 || P.Fragments.Num()<20){UE_LOG(LogTemp,Error,TEXT("Too few rock inclusions: %s"),*P.TreasureId.ToString());++Failures;}
        Preview->Destroy();
    }
    UE_LOG(LogTemp,Display,TEXT("Campaign minerals prepared: %d, failures %d"),Field->AuthoredPlacements.Num(),Failures);
    return Failures==0 && RefineCampaignCrystalClues(Field);
}

bool UMiningDesignerLibrary::RefineCampaignCrystalClues(AMiningTreasureField* Field, bool bPreserveSurface)
{
    if(!Field || !Field->GetWorld() || Field->GetWorld()->IsGameWorld())return false;
    Field->Modify();
    FRandomStream Random(482771);
    int Failures=0;
    for(auto& P:Field->AuthoredPlacements)
    {
        const FVector Surface=P.Surface.GetLocation(),Core=P.Transform.GetLocation();
        const FVector Normal=(Surface-Core).GetSafeNormal();
        FVector Right,Up;Normal.FindBestAxisVectors(Right,Up);
        // Rebuild only the clue layout. Core depth, reward, host terrain and rooms
        // are not moved by this idempotent presentation adjustment.
        TArray<FTransform> SurfaceFragments;
        TArray<FVector> SurfaceSupports;
        if(bPreserveSurface)for(int I=0;I<P.Fragments.Num();++I)
        {
            // Authored surface chips have a 6.5 cm inward support offset;
            // underground inclusions use their actual centres as support points.
            if(P.FragmentSupports.IsValidIndex(I) && FVector::DistSquared(P.Fragments[I].GetLocation(),P.FragmentSupports[I])>1)
            {SurfaceFragments.Add(P.Fragments[I]);SurfaceSupports.Add(P.FragmentSupports[I]);}
        }
        P.Fragments=MoveTemp(SurfaceFragments);P.FragmentSupports=MoveTemp(SurfaceSupports);
        auto Add=[&](FVector Position,FVector Support,float Scale,float Spacing=22.f)
        {
            for(const FVector& Existing:P.FragmentSupports)
                if(FVector::DistSquared(Existing,Support)<FMath::Square(Spacing))return;
            TArray<bool> Solid;AMiningTreasure::MarkCoveredPoints(P.Hosts,{Support},Solid);
            if(!Solid[0])return;
            P.FragmentSupports.Add(Support);
            P.Fragments.Add(FTransform(Random.VRand().Rotation(),Position,FVector(Scale,Scale*.85,Scale)));
        };
        // A readable local patch, formerly only five flecks spread over 6.6 m.
        // Project onto the actual wall so the flecks exist before the first hit.
        for(int Attempt=0;!bPreserveSurface && Attempt<320 && P.Fragments.Num()<14;++Attempt)
        {
            const float Angle=Random.FRandRange(0,2*PI),Radius=Random.FRandRange(20,135);
            const FVector Q=Surface+Right*(Radius*FMath::Cos(Angle))+Up*(Radius*FMath::Sin(Angle));
            FHitResult Best;double Distance=BIG_NUMBER;
            for(ADiggableRock* Host:P.Hosts)if(IsValid(Host))
            {
                FHitResult Hit;
                if(Host->ActorLineTraceSingle(Hit,Q+Normal*110,Q-Normal*160,ECC_Visibility,FCollisionQueryParams(NAME_None,true)) && Hit.Distance<Distance)
                {Best=Hit;Distance=Hit.Distance;}
            }
            if(Distance<BIG_NUMBER)Add(Best.ImpactPoint+Best.ImpactNormal*1.5,Best.ImpactPoint-Best.ImpactNormal*5,Random.FRandRange(.11,.16),20);
        }
        const int SurfaceCount=P.Fragments.Num();
        // A 160 cm pickaxe erased the entire old 130 cm-wide trail. Put enough
        // inclusions beyond its centre line to reveal on the newly cut walls.
        // Increasing area density (not just total count) makes the core richer.
        const float Depth=FVector::Dist(Surface,Core);
        const int Layers=FMath::Clamp(FMath::CeilToInt(Depth/24),4,40);
        for(int Layer=1;Layer<=Layers;++Layer)
        {
            const float T=float(Layer)/(Layers+1);
            const int TargetCount=FMath::RoundToInt(24+35*T+70*T*T);
            const int Before=P.Fragments.Num();
            for(int Attempt=0;Attempt<TargetCount*18 && P.Fragments.Num()-Before<TargetCount;++Attempt)
            {
                const float Angle=Random.FRandRange(0,2*PI);
                const float Radius=FMath::Sqrt(Random.FRand())*FMath::Lerp(165.f,225.f,T);
                const FVector Q=FMath::Lerp(Surface,Core,T)+Right*(Radius*FMath::Cos(Angle))+Up*(Radius*FMath::Sin(Angle));
                // Never add new initial surface flecks: require a buried shell
                // around the centre, including on curved or overlapping walls.
                {
                    TArray<bool> Buried;AMiningTreasure::MarkCoveredPoints(P.Hosts,{Q+Normal*18,Q-Normal*18,Q+Right*18,Q-Right*18,Q+Up*18,Q-Up*18},Buried);
                    if(Buried.Contains(false))continue;
                }
                Add(Q,Q,Random.FRandRange(.11,.17),FMath::Lerp(25.f,17.f,T));
            }
        }
        int Inner=0;for(int I=0;I<P.FragmentSupports.Num();++I)
            Inner+=FVector::DistSquared(P.FragmentSupports[I],Core)<225*225 && FVector::DistSquared(P.Fragments[I].GetLocation(),P.FragmentSupports[I])<=1;
        const int Target=P.Fragments.Num()+FMath::Max(0,850-Inner);
        for(int Attempt=0;Attempt<18000 && P.Fragments.Num()<Target;++Attempt)
        {
            // Radial density rises toward the gem, rather than merely increasing
            // the total number uniformly throughout the whole halo.
            const FVector Q=Core+Random.VRand()*225*FMath::Pow(Random.FRand(),.6f);
            // The core halo must not leak additional clues through a shallow wall.
            TArray<bool> Buried;AMiningTreasure::MarkCoveredPoints(P.Hosts,{Q+Normal*18,Q-Normal*18,Q+Right*18,Q-Right*18,Q+Up*18,Q-Up*18},Buried);
            if(Buried.Contains(false))continue;
            Add(Q,Q,Random.FRandRange(.11,.17),17);
        }
        // Geometry clipping can make a particular shell sparser than its outer
        // neighbour. Enforce the density gradient per authored deposit, measured
        // per unit volume, instead of relying on the average of a random sample.
        auto CountShell=[&](float Low,float High)
        {
            int Count=0;for(int I=0;I<P.FragmentSupports.Num();++I)
            {
                // Density is for buried inclusions only; shallow surface clues
                // must not artificially satisfy the inner-shell target.
                if(FVector::DistSquared(P.Fragments[I].GetLocation(),P.FragmentSupports[I])>1)continue;
                const double D=FVector::DistSquared(P.FragmentSupports[I],Core);Count+=D>=Low*Low && D<High*High;
            }
            return Count;
        };
        auto FillShell=[&](float Low,float High,int Wanted)
        {
            int Count=CountShell(Low,High);
            for(int Attempt=0;Count<Wanted && Attempt<12000;++Attempt)
            {
                const float R=FMath::Pow(FMath::Lerp(Low*Low*Low,High*High*High,Random.FRand()),1.f/3);
                const FVector Q=Core+Random.VRand()*R;
                TArray<bool> Buried;AMiningTreasure::MarkCoveredPoints(P.Hosts,{Q+Normal*18,Q-Normal*18,Q+Right*18,Q-Right*18,Q+Up*18,Q-Up*18},Buried);
                if(Buried.Contains(false))continue;
                const int Before=P.Fragments.Num();Add(Q,Q,Random.FRandRange(.11,.17),17);
                Count+=P.Fragments.Num()>Before;
            }
        };
        FillShell(80,160,FMath::CeilToInt(CountShell(160,225)*(160.*160*160-80.*80*80)/(225.*225*225-160.*160*160)*1.4));
        FillShell(0,80,FMath::CeilToInt(CountShell(80,160)/7.*1.55));
        const double MiddleDensity=CountShell(80,160)/(160.*160*160-80.*80*80);
        const double OuterDensity=CountShell(160,225)/(225.*225*225-160.*160*160);
        const double InnerDensity=CountShell(0,80)/(80.*80*80);
        if(OuterDensity<=0 || MiddleDensity+1.e-12<OuterDensity*1.4 || InnerDensity+1.e-12<MiddleDensity*1.55)
        {++Failures;UE_LOG(LogTemp,Error,TEXT("Crystal density cannot fit this rock pocket: %s at %s"),*P.TreasureId.ToString(),*Core.ToString());}
        if(SurfaceCount<8 || P.Fragments.Num()<45)
        {++Failures;UE_LOG(LogTemp,Error,TEXT("Compact vein lacks surface breadcrumbs: %s (%d surface, %d total)"),*P.TreasureId.ToString(),SurfaceCount,P.Fragments.Num());}
    }
    return Failures==0;
}

FString UMiningDesignerLibrary::ValidateCampaignMinerals(AMiningTreasureField* Field)
{
    auto Report=MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Errors,Rows;
    auto Error=[&](const FString& Message){Errors.Add(MakeShared<FJsonValueString>(Message));};
    if(!IsValid(Field) || !Field->GetWorld() || Field->GetWorld()->IsGameWorld())Error(TEXT("Use a valid field in an editor world; stop PIE first."));
    else
    {
        if(!Field->bUseAuthoredPlacements || Field->AuthoredPlacements.IsEmpty())Error(TEXT("Authored placements must be enabled and nonempty."));
        if(!Field->CrystalClueMesh || !Field->TreasureMaterial)Error(TEXT("Missing crystal mesh or treasure material."));
        for(int Index=0;Index<Field->AuthoredPlacements.Num();++Index)
        {
            const auto& P=Field->AuthoredPlacements[Index];
            auto Row=MakeShared<FJsonObject>();
            Row->SetNumberField(TEXT("index"),Index);Row->SetStringField(TEXT("id"),P.TreasureId.ToString());
            Row->SetStringField(TEXT("area"),P.AreaTag.ToString());Row->SetNumberField(TEXT("value"),P.Value);
            auto Fail=[&](const FString& Why){Error(FString::Printf(TEXT("Placement %d (%s): %s"),Index,*P.TreasureId.ToString(),*Why));};
            if(P.Transform.ContainsNaN() || P.Surface.ContainsNaN()){Fail(TEXT("Non-finite core/surface transform."));continue;}
            if(P.Hosts.IsEmpty())Fail(TEXT("No host rocks."));
            for(ADiggableRock* Host:P.Hosts)if(!IsValid(Host) || Host->GetWorld()!=Field->GetWorld() || !Host->ActorHasTag(P.AreaTag))Fail(TEXT("Missing, foreign-world or incorrectly tagged host."));
            if(P.Fragments.Num()!=P.FragmentSupports.Num())Fail(TEXT("Fragment/support counts differ."));
            const auto* Definition=Field->Definitions.FindByPredicate([&](const auto& D){return D.Id==P.TreasureId;});
            if(!Definition || !Definition->ValuesBySize.Contains(P.Value))Fail(TEXT("Unknown mineral or invalid value tier."));
            else
            {
                auto* Preview=Field->GetWorld()->SpawnActor<AMiningTreasure>(P.Transform.GetLocation(),P.Transform.Rotator());
                if(!Preview)Fail(TEXT("Could not create validation preview."));
                else
                {
                    Preview->CoveringRocks=P.Hosts;
                    if(!Preview->Configure(*Definition,Definition->ValuesBySize.IndexOfByKey(P.Value),Field->TreasureMaterial) || Preview->MeasureExposure()>=.01f)Fail(TEXT("Gem is not fully buried in the current terrain."));
                    Preview->Destroy();
                }
            }
            TArray<FVector> Points={P.Transform.GetLocation()};
            bool Finite=true;
            for(const auto& Q:P.FragmentSupports)Finite&=!Q.ContainsNaN();
            for(const auto& F:P.Fragments)Finite&=!F.ContainsNaN();
            if(!Finite){Fail(TEXT("Non-finite crystal coordinates."));continue;}
            Points.Append(P.FragmentSupports);
            TArray<bool> Covered;AMiningTreasure::MarkCoveredPoints(P.Hosts,Points,Covered);
            int Unsupported=0;for(bool Solid:Covered)Unsupported+=!Solid;
            if(Unsupported)Fail(FString::Printf(TEXT("%d core/support points are outside remaining rock; rebuild after terrain edits."),Unsupported));
            int Surface=0;int Counts[3]={0,0,0};
            for(int I=0;I<P.FragmentSupports.Num();++I)
            {
                if(!P.Fragments.IsValidIndex(I))continue;
                if(FVector::DistSquared(P.Fragments[I].GetLocation(),P.FragmentSupports[I])>1){++Surface;continue;}
                const double D=FVector::Dist(P.FragmentSupports[I],P.Transform.GetLocation());
                if(D<80)++Counts[0];else if(D<160)++Counts[1];else if(D<225)++Counts[2];
            }
            const double Volumes[]={4.*PI/3*80*80*80,4.*PI/3*(160.*160*160-80.*80*80),4.*PI/3*(225.*225*225-160.*160*160)};
            TArray<TSharedPtr<FJsonValue>> Densities,ShellCounts;
            for(int I=0;I<3;++I){Densities.Add(MakeShared<FJsonValueNumber>(Counts[I]*1.e6/Volumes[I]));ShellCounts.Add(MakeShared<FJsonValueNumber>(Counts[I]));}
            if(Surface<8)Fail(TEXT("Fewer than 8 surface clues."));
            if(Counts[2]==0 || Counts[1]/Volumes[1]+1.e-12<1.4*Counts[2]/Volumes[2] || Counts[0]/Volumes[0]+1.e-12<1.55*Counts[1]/Volumes[1])Fail(TEXT("Interior density must increase: middle >= 1.40 x outer, inner >= 1.55 x middle."));
            Row->SetNumberField(TEXT("surface_fragments"),Surface);Row->SetNumberField(TEXT("fragments"),P.Fragments.Num());
            Row->SetNumberField(TEXT("unsupported_points"),Unsupported);
            Row->SetArrayField(TEXT("inner_middle_outer_counts"),ShellCounts);Row->SetArrayField(TEXT("inner_middle_outer_per_m3"),Densities);
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    Report->SetBoolField(TEXT("ok"),Errors.IsEmpty());Report->SetArrayField(TEXT("errors"),Errors);Report->SetArrayField(TEXT("placements"),Rows);
    FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));return Json;
}
