#include "MiningDesignerLibrary.h"
#include "EngineUtils.h"
#include "GeneratedMiningRock.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "Generators/GridBoxMeshGenerator.h"
#include "Operations/MeshBoolean.h"
#include "Operations/MeshPlaneCut.h"
#include "ConstrainedDelaunay2.h"
#include "UDynamicMesh.h"
#include "Components/DynamicMeshComponent.h"

FVector UMiningDesignerLibrary::FindCampaignWall(AActor* Rock,FVector Start,FVector End)
{
    FHitResult Hit;
    if(Rock && Rock->ActorLineTraceSingle(Hit,Start,End,ECC_Visibility,FCollisionQueryParams(NAME_None,true)))return Hit.ImpactPoint;
    UE_LOG(LogTemp,Error,TEXT("No campaign wall found from %s"),*Start.ToString());return End;
}

bool UMiningDesignerLibrary::BuildCampaignSoil(AActor* Actor,FVector Size,FVector Roughness)
{
    using namespace UE::Geometry;
    auto* Rock=Cast<AGeneratedMiningRock>(Actor);if(!Rock)return false;
    FGridBoxMeshGenerator Gen;const FVector Half=Size*.5;
    Gen.Box=FOrientedBox3d(FVector3d::Zero(),FVector3d(Half));
    Gen.EdgeVertices=FIndex3i(FMath::Clamp(FMath::CeilToInt(Size.X/95)+1,3,80),FMath::Clamp(FMath::CeilToInt(Size.Y/95)+1,3,80),FMath::Clamp(FMath::CeilToInt(Size.Z/95)+1,3,80));
    if(Roughness.IsNearlyZero())Gen.EdgeVertices=FIndex3i(2,2,2);
    Gen.Generate();FDynamicMesh3 Mesh(&Gen);Mesh.DiscardAttributes();
    for(int V:Mesh.VertexIndicesItr())
    {
        FVector P=Mesh.GetVertex(V),Offset=FVector::ZeroVector;
        const FVector World=Rock->GetActorTransform().TransformPosition(P);
        for(int A=0;A<3;++A)if(FMath::Abs(FMath::Abs(P[A])-Half[A])<.001)
        {
            // Pin edges so adjoining sections meet; vary face interiors as low-poly rock facets.
            const double Fade=FMath::Clamp((Half[(A+1)%3]-FMath::Abs(P[(A+1)%3]))/90.,0.,1.)*FMath::Clamp((Half[(A+2)%3]-FMath::Abs(P[(A+2)%3]))/90.,0.,1.);
            const double Noise=FMath::PerlinNoise3D(World*.011+FVector(13,29,7))*1.6;
            Offset[A]=FMath::Sign(P[A])*Roughness[A]*Noise*Fade;
            if(Rock->GetActorLabel().StartsWith(TEXT("Campaign_Tunnel")))
            {
                const FString Name=Rock->GetActorLabel();
                const bool Inner=(Name.Contains(TEXT("Ground")) && A==2 && P[A]>0) ||
                    (Name.Contains(TEXT("Roof")) && A==2 && P[A]<0) ||
                    (Name.Contains(TEXT("SoilLeft")) && A==1 && P[A]>0) ||
                    (Name.Contains(TEXT("SoilRight")) && A==1 && P[A]<0);
                Offset[A]=Inner?-FMath::Sign(P[A])*Roughness[A]*(.3+FMath::Abs(Noise))*Fade:0;
            }
        }
        Mesh.SetVertex(V,P+Offset);
    }
    Mesh.EnableAttributes();FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
    Rock->RockSize=Size;Rock->SetGeneratedMesh(MoveTemp(Mesh));return true;
}

bool UMiningDesignerLibrary::SoftenCampaignPassage(AActor* Actor)
{
    using namespace UE::Geometry;
    auto* Rock=Cast<AGeneratedMiningRock>(Actor);
    if(!Rock || !Rock->GetActorLabel().StartsWith(TEXT("Campaign_Tunnel")))return false;
    const FString Name=Rock->GetActorLabel();
    const FVector Size=Rock->RockSize;
    const bool Ground=Name.Contains(TEXT("Ground")), Roof=Name.Contains(TEXT("Roof"));
    const double Height=360;
    double Width=(Ground || Roof)?Size.Y-1400:400;
    // The card-room exit is wider. The paired roof provides the exact shared frame.
    FVector Frame=Rock->GetActorLocation();
    if(Ground)Frame.Z+=500;
    else if(Roof)Frame.Z-=Height+350;
    else
    {
        for(TActorIterator<AGeneratedMiningRock> It(Rock->GetWorld());It;++It)
            if(It->GetActorLabel().StartsWith(TEXT("Campaign_TunnelRoof")) &&
                FMath::Abs(It->GetActorLocation().X-Frame.X)<1 && FMath::Abs(It->RockSize.X-Size.X)<1 &&
                FMath::Abs(It->GetActorLocation().Z-(Frame.Z+Height*.5+350))<1)
            {Width=It->RockSize.Y-1400;break;}
        Frame.Y+=Name.Contains(TEXT("SoilLeft"))?Width*.5+350:-Width*.5-350;
        Frame.Z-=Height*.5;
    }
    FDynamicMesh3 Mesh;Rock->CopyMiningMesh(Mesh);Mesh.DiscardAttributes();
    const FTransform Transform=Rock->GetActorTransform();
    for(int V:Mesh.VertexIndicesItr())
    {
        FVector P=Transform.TransformPosition(Mesh.GetVertex(V))-Frame;
        const double T=FMath::Clamp(P.X/Size.X+.5,0.,1.);
        // Shared deformation for all four pieces keeps their boundaries watertight.
        // Pin the actual door/cave seams and ease into an asymmetric arched section.
        const double Fade=FMath::Square(FMath::Sin(PI*T));
        const double Y=P.Y, Z=P.Z;
        const double Upper=FMath::SmoothStep(Height*.52,Height,Z);
        const double Arch=FMath::Max(0.,1-FMath::Square(Y/(Width*.62)));
        const double Near=FMath::Exp(-FMath::Square(Y/(Width*1.5)));
        P.Y+=Fade*(Y*(.12-.26*Upper)*Near+42*FMath::Sin(T*5+Frame.X*.001));
        P.Z+=Fade*(105*Arch*Upper+2*FMath::Sin(T*9));
        Mesh.SetVertex(V,Transform.InverseTransformPosition(Frame+P));
    }
    Mesh.EnableAttributes();FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
    Rock->Modify();
    if(Mesh.TriangleCount()>12000)return Rock->SetChunkedMesh(Mesh,700,true,true);
    Rock->SetGeneratedMesh(MoveTemp(Mesh));return true;
}

bool UMiningDesignerLibrary::FitCampaignSoil(const TArray<AActor*>& SoilActors,const TArray<AActor*>& SourceActors)
{
    using namespace UE::Geometry;
    TArray<FDynamicMesh3> Solids;
    auto Add=[&](const FDynamicMesh3& Local,const FTransform& T)
    {if(!Local.TriangleCount())return;auto& M=Solids.Add_GetRef(Local);M.DiscardAttributes();for(int V:M.VertexIndicesItr())M.SetVertex(V,T.TransformPosition(M.GetVertex(V)));};
    for(AActor* A:SourceActors)if(auto* R=Cast<AGeneratedMiningRock>(A))
    {
        if(R->bChunked){for(const auto& C:R->Chunks)if(C.InitialMesh)C.InitialMesh->ProcessMesh([&](const FDynamicMesh3& M){Add(M,R->GetActorTransform());});}
        else {FDynamicMesh3 M;R->CopyMiningMesh(M);Add(M,R->GetActorTransform());}
    }
    const int32 SourceSolidCount=Solids.Num();
    for(AActor* A:SoilActors)
    {
        auto* R=Cast<AGeneratedMiningRock>(A);if(!R)return false;
        FDynamicMesh3 Mesh;R->CopyMiningMesh(Mesh);Mesh.DiscardAttributes();
        const FTransform T=R->GetActorTransform();for(int V:Mesh.VertexIndicesItr())Mesh.SetVertex(V,T.TransformPosition(Mesh.GetVertex(V)));
        for(int SolidIndex=0;SolidIndex<Solids.Num();++SolidIndex)
        {
            // The generator cuts the complete tunnel envelope out of source rock first.
            // Those solids only touch this envelope; re-cutting their shared faces creates slivers.
            if(SolidIndex<SourceSolidCount && R->GetActorLabel().StartsWith(TEXT("Campaign_Tunnel")))continue;
            const FDynamicMesh3& Solid=Solids[SolidIndex];
            if(!Mesh.TriangleCount())break;
            const auto B=Mesh.GetBounds(),S=Solid.GetBounds();bool Overlap=true;
            for(int K=0;K<3;++K)Overlap &= FMath::Min(B.Max[K],S.Max[K])-FMath::Max(B.Min[K],S.Min[K])>.05;
            if(!Overlap)continue;
            FDynamicMesh3 Result;bool Good=false;
            for(double Tolerance:{1.e-7,1.e-5,1.e-4,1.e-3,1.e-2})
            {
                Result.Clear();FMeshBoolean Op(&Mesh,&Solid,&Result,FMeshBoolean::EBooleanOp::Difference);Op.SnapTolerance=Tolerance;Op.bPreserveTriangleGroups=false;
                if(Op.Compute() && Op.CreatedBoundaryEdges.IsEmpty() && (!Result.TriangleCount() || Result.IsClosed())){Good=true;break;}
            }
            FDynamicMesh3 Perturbed;
            for(int Retry=0;!Good && Retry<5;++Retry)
            {
                Perturbed=Solid;
                const FVector3d Shift=FVector3d(.013,.021,.017)*(Retry+1);
                for(int V:Perturbed.VertexIndicesItr())Perturbed.SetVertex(V,Perturbed.GetVertex(V)+Shift);
                Result.Clear();FMeshBoolean Op(&Mesh,&Perturbed,&Result,FMeshBoolean::EBooleanOp::Difference);Op.SnapTolerance=1.e-5;Op.bPreserveTriangleGroups=false;
                Good=Op.Compute() && Op.CreatedBoundaryEdges.IsEmpty() && (!Result.TriangleCount() || Result.IsClosed());
            }
            if(!Good){UE_LOG(LogTemp,Error,TEXT("Campaign terrain junction failed for %s: mesh %d solid %d result %d closed %d, solid bounds %s to %s"),*R->GetActorLabel(),Mesh.TriangleCount(),Solid.TriangleCount(),Result.TriangleCount(),Result.IsClosed(),*FVector(S.Min).ToString(),*FVector(S.Max).ToString());return false;}
            Mesh=MoveTemp(Result);
        }
        // Partition shared volume once: original cave and neighboring soil have no overlapping interiors.
        if(Mesh.TriangleCount())Solids.Add(Mesh);
        for(int V:Mesh.VertexIndicesItr())Mesh.SetVertex(V,T.InverseTransformPosition(Mesh.GetVertex(V)));
        Mesh.EnableAttributes();FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
        if(Mesh.TriangleCount()>12000)
        {
            if(!R->SetChunkedMesh(Mesh,1000,true,true))return false;
        }
        else R->SetGeneratedMesh(MoveTemp(Mesh));
    }
    return true;
}

bool UMiningDesignerLibrary::BakeCampaignRock(AActor* Actor,FVector BoundsMin,FVector BoundsMax,const TArray<FTransform>& Openings)
{
    using namespace UE::Geometry;
    auto* Rock=Cast<AGeneratedMiningRock>(Actor); if(!Rock) return false;
    const FTransform Original=Rock->GetActorTransform();
    auto Bake=[&](FDynamicMesh3& Mesh)->bool
    {
    Mesh.DiscardAttributes();
    for(int V:Mesh.VertexIndicesItr()) Mesh.SetVertex(V,Original.TransformPosition(Mesh.GetVertex(V)));
    if(!Mesh.TriangleCount() || !Mesh.GetBounds().Intersects(FAxisAlignedBox3d(BoundsMin,BoundsMax))) { Mesh.Clear();return true; }
    if(!Mesh.HasTriangleGroups()) Mesh.EnableTriangleGroups();
    for(const FTransform& Opening:Openings)
    {
        if(!Mesh.TriangleCount()) break;
        FMinimalBoxMeshGenerator Gen;Gen.Box=FOrientedBox3d(FVector3d::Zero(),FVector3d(Opening.GetScale3D()*.5));Gen.Generate();
        FDynamicMesh3 Tool(&Gen);Tool.DiscardAttributes();
        for(int V:Tool.VertexIndicesItr())Tool.SetVertex(V,Opening.GetRotation().RotateVector(Tool.GetVertex(V))+Opening.GetLocation());
        if(!Mesh.GetBounds().Intersects(Tool.GetBounds())) continue;
        bool Success=false;FDynamicMesh3 Result;
        for(double Tolerance:{1.e-7,1.e-5,1.e-4,1.e-3,1.e-2})
        {
            Result.Clear();FMeshBoolean Op(&Mesh,&Tool,&Result,FMeshBoolean::EBooleanOp::Difference);Op.SnapTolerance=Tolerance;
            Op.bPreserveTriangleGroups=false;
            const bool Computed=Op.Compute();
            if(Computed && Op.CreatedBoundaryEdges.IsEmpty() && (Result.TriangleCount()==0 || Result.IsClosed())) { Success=true;break; }
            UE_LOG(LogTemp,Display,TEXT("Campaign boolean retry: computed %d boundary %d result %d closed %d tool %s"),Computed,Op.CreatedBoundaryEdges.Num(),Result.TriangleCount(),Result.IsClosed(),*Opening.GetScale3D().ToString());
        }
        // Sub-millimetre offsets avoid coincident cut vertices on imported planar surfaces.
        for(int Retry=0;!Success && Retry<3;++Retry)
        {
            for(int V:Tool.VertexIndicesItr()) Tool.SetVertex(V,Tool.GetVertex(V)+FVector3d(.013,.021,.017));
            Result.Clear();FMeshBoolean Op(&Mesh,&Tool,&Result,FMeshBoolean::EBooleanOp::Difference);Op.SnapTolerance=1.e-4;Op.bPreserveTriangleGroups=false;
            Success=Op.Compute() && Op.CreatedBoundaryEdges.IsEmpty() && (Result.TriangleCount()==0 || Result.IsClosed());
        }
        if(!Success) { UE_LOG(LogTemp,Error,TEXT("Campaign opening failed: %s at %s, closed %d triangles %d"),*Rock->GetName(),*Opening.GetLocation().ToString(),Mesh.IsClosed(),Mesh.TriangleCount());return false; }
        Mesh=MoveTemp(Result);
    }
    for(int Axis=0;Axis<3;++Axis) for(int Side=0;Side<2;++Side)
    {
        if(!Mesh.TriangleCount()) break;
        const auto Bounds=Mesh.GetBounds();
        if((Side && Bounds.Max[Axis]<=BoundsMax[Axis]) || (!Side && Bounds.Min[Axis]>=BoundsMin[Axis])) continue;
        FVector P=FVector::ZeroVector,N=FVector::ZeroVector;P[Axis]=Side?BoundsMax[Axis]:BoundsMin[Axis];N[Axis]=Side?1:-1;
        FMeshPlaneCut Cut(&Mesh,P,N);Cut.PlaneTolerance=.000001;
        if(!Cut.Cut()) { UE_LOG(LogTemp,Error,TEXT("Campaign crop failed: %s axis %d side %d"),*Rock->GetName(),Axis,Side);return false; }
        if(Mesh.TriangleCount() && !Mesh.IsClosed() && !Cut.HoleFill(ConstrainedDelaunayTriangulate<double>,false))
        { UE_LOG(LogTemp,Error,TEXT("Campaign cap failed: %s axis %d side %d triangles %d"),*Rock->GetName(),Axis,Side,Mesh.TriangleCount());return false; }
    }
    for(int V:Mesh.VertexIndicesItr())Mesh.SetVertex(V,Original.InverseTransformPosition(Mesh.GetVertex(V)));
    Mesh.EnableAttributes(); FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
    return true;
    };
    if(Rock->bChunked)
    {
        TArray<FDynamicMesh3> Results;
        for(const FMiningRockChunk& Chunk:Rock->Chunks)
        {
            auto& Mesh=Results.AddDefaulted_GetRef();
            if(Chunk.InitialMesh) Chunk.InitialMesh->ProcessMesh([&](const FDynamicMesh3& Source){Mesh=Source;});
            if(!Bake(Mesh)) return false;
        }
        Rock->Modify();
        for(int I=0;I<Rock->Chunks.Num();++I)
        {
            auto& Chunk=Rock->Chunks[I];const auto Bounds=Results[I].GetBounds();
            Chunk.Bounds=Results[I].TriangleCount()?FBox(Bounds.Min,Bounds.Max):FBox(ForceInit);
            if(Chunk.InitialMesh) { Chunk.InitialMesh->Modify();Chunk.InitialMesh->SetMesh(MoveTemp(Results[I])); }
        }
        Rock->ResetRock();
    }
    else
    {
        FDynamicMesh3 Mesh;Rock->CopyMiningMesh(Mesh);
        if(!Bake(Mesh)) return false;
        Rock->SetGeneratedMesh(MoveTemp(Mesh));
    }
    return true;
}

